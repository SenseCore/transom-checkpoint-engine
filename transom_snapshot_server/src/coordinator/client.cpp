/**
 * @file client.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "coordinator/client.h"

#include "config/iteration_manager.h"
#include "monitor/monitor.h"
#include "util/channel.h"
#include "util/util.h"

using coordinator::ClientUtil;
using util::Util;
using util::channel;
using communicators::CommunicatorFactory;
using communicators::RdmaCommunicator;
using communicators::EndpointFactory;
using config::WorldState;
using storage::Storage;
using monitor::MemoryMonitor;
using config::IterationManager;

bool ClientUtil::Backup(api::InterNodeBackupRequest &req, api::InterNodeBackupResponse &rsp) {
    LOG_TRACE("begin of inter-node backup request");
    buffer::Buffer buffer;

    /* init communicator */
    auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_RDMA);
    std::string nextHostAddr;
    if (!getNextNode(std::ref(nextHostAddr))) {
        LOG_ERROR("get next node IP failed");
        return false;
    }
    ep.setAddr(nextHostAddr);
    auto communicator = RdmaCommunicator(ep);
    if (!communicator.Connect()) {
        LOG_ERROR("connect failed");
        return false;
    }

    /* send routine */
    size_t routine = static_cast<size_t>(api::Routine::INTER_NODE_BACKUP);
    buffer.Add(routine);
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node backup routine");
        return false;
    }
    LOG_TRACE("routine {} sent", api::RoutineString((api::Routine)routine));

    /* marshal request */
    buffer.Reset();
    req.Marshal(std::ref(buffer));

    /* send request */
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node backup request");
        return false;
    }
    LOG_TRACE("inter-node backup request body: {}", req.String());

    /* load response */
    buffer.Reset();
    if (!communicator.Read(std::ref(buffer))) {
        LOG_ERROR("recv inter-node backup response");
        return false;
    }

    /* parse response */
    rsp.Unmarshal(std::ref(buffer));
    LOG_TRACE("inter-node backup response {}", rsp.String());

    /* error handling */
    if (rsp.code != api::STATUS_SUCCESS) {
        LOG_ERROR("inter-node backup response code {}", rsp.code);
        return false;
    }

    if (!req.only_metadata) {
        /* rdma handshake, now we have both local address and server side address */
        auto localAddr = req.data_entry.address;
        if (auto rc = communicator.rdma_handshake(false, localAddr, req.metadata.size); !api::IsSuccess(rc)) {
            LOG_ERROR("rdma handshake failed for address {}", (void *)localAddr);
            return false;
        }

        /* rdma write */
        if (!communicator.rdma_write((const char *)localAddr, 0, 0, req.metadata.size)) {
            LOG_ERROR("rdma_write, address {} local_offset 0 remote_offset 0 size {}",
                      (void *)localAddr, req.metadata.size);
            return false;
        }

        /* notify server write finished */
        buffer.Reset();
        buffer.AddString("W");
        if (!communicator.Write(std::ref(buffer))) {
            LOG_ERROR("notify server rdma_write finishes");
            return false;
        }
    }

    LOG_TRACE("end of inter-node backup request");
    return true;
}

bool ClientUtil::LoadRemote(api::InterNodeLoadRequest &req, api::InterNodeLoadResponse &rsp) {
    LOG_TRACE("begin of inter-node load request");
    buffer::Buffer buffer;

    /* init communicator */
    auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_RDMA);
    std::string remoteIP;
    if (!getNodeIP(req.metadata.node_rank, std::ref(remoteIP))) {
        LOG_ERROR("failed to get node IP of rank {}", req.metadata.node_rank);
        return false;
    }
    ep.setAddr(remoteIP);
    auto communicator = RdmaCommunicator(ep);
    if (!communicator.Connect()) {
        return false;
    }

    /* send routine */
    size_t routine = static_cast<size_t>(api::Routine::INTER_NODE_LOAD);
    buffer.Add(routine);
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node load request");
        return false;
    }
    LOG_TRACE("routine {} sent", api::RoutineString((api::Routine)routine));

    /* marshal request */
    buffer.Reset();
    req.Marshal(std::ref(buffer));

    /* send request */
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node load request");
        return false;
    }
    LOG_TRACE("sent inter-node load request");

    /* load response */
    buffer.Reset();
    if (!communicator.Read(std::ref(buffer))) {
        LOG_ERROR("recv inter-node load response");
        return false;
    }

    /* parse response */
    rsp.Unmarshal(std::ref(buffer));
    LOG_TRACE("recved inter-node load response: {}", rsp.String());

    /* handle rsp code */
    if (!api::IsSuccess(rsp.code)) {
        LOG_ERROR("response code {}", rsp.code);
        return false;
    }

    /* exit if only load metadata */
    if (req.only_metadata) {
        return true;
    }

    /* in case memory is not enough */
    auto memStat = monitor::MemoryMonitor::Instance().GetMemoryStat();
    if (memStat.total_idle < rsp.metadata.size) {
        LOG_WARN("rdma read {} bytes data will cause OOM, only {} idle memory!", rsp.metadata.size, memStat.total_idle);
        return false;
    }

    /* rdma handshake, now we have both local address and server side address */
    api::DataEntry entry;
    if (auto rc = MemoryMonitor::Instance().TryMemfdMalloc(std::ref(rsp.metadata), std::ref(entry));
        !api::IsSuccess(rc)) {
        LOG_ERROR("memfdCalloc failed");
        return false;
    }
    Storage::Instance().Save(rsp.metadata, entry);
    LOG_DEBUG("Util::memfdCalloc localAddr: {} length: {}", entry.address, rsp.metadata.size);
    if (auto rc = communicator.rdma_handshake(false, entry.address, rsp.metadata.size); !api::IsSuccess(rc)) {
        LOG_ERROR("rdma handshake failed for address {}", (void *)entry.address);
        return false;
    }

    /* rdma read */
    auto ptr = reinterpret_cast<char *>(entry.address);
    if (!communicator.rdma_read(std::ref(ptr), 0, 0, rsp.metadata.size)) {
        LOG_ERROR("rdma_write, address {} local_offset 0 remote_offset 0 size {}",
                  (void *)entry.address, rsp.metadata.size);
        return false;
    }

    /* notify server that read finished */
    buffer.Reset();
    buffer.AddString(config::RDMA_READ_MSG);
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("notify server rdma_write finishes");
        return false;
    }

    LOG_TRACE("end of inter-node load request");
    return true;
}

bool ClientUtil::BatchLoadRemote(api::InterNodeBatchLoadRequest &req, api::InterNodeBatchLoadResponse &rsp) {
    LOG_TRACE("begin of inter-node batch-load request");
    buffer::Buffer buffer;

    /* init communicator */
    auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_RDMA);
    std::string remoteIP;
    if (!getNextNode(std::ref(remoteIP))) {
        return false;
    }
    ep.setAddr(remoteIP);
    auto communicator = RdmaCommunicator(ep);
    if (!communicator.Connect()) {
        return false;
    }

    /* send routine */
    size_t routine = static_cast<size_t>(api::Routine::INTER_NODE_BATCH_LOAD);
    buffer.Add(routine);
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node batch-load request");
        return false;
    }
    LOG_TRACE("routine {} sent", api::RoutineString((api::Routine)routine));

    /* marshal request */
    buffer.Reset();
    req.Marshal(std::ref(buffer));

    /* send request */
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node batch-load request");
        return false;
    }
    LOG_TRACE("sent inter-node batch-load request");

    /* load response */
    buffer.Reset();
    if (!communicator.Read(std::ref(buffer))) {
        LOG_ERROR("recv inter-node batch-load response");
        return false;
    }

    /* parse response */
    rsp.Unmarshal(std::ref(buffer));
    LOG_TRACE("recved inter-node batch-load response: {}", rsp.String());

    /* handle rsp code */
    if (rsp.code == api::STATUS_UNKNOWN_ERROR) {
        LOG_ERROR("response code {}", rsp.code);
        return false;
    }

    /* exit if only load metadata */
    if (req.only_metadata) {
        return true;
    }

    auto loadFunc = [](channel<api::InterNodeLoadResponse> &ch, channel<bool> &res_ch) {
        /* load each checkpoint */
        auto loadEach = [](api::InterNodeLoadResponse item) -> bool {
            ClientUtil client;
            api::InterNodeLoadRequest req(item.metadata, false);
            api::InterNodeLoadResponse rsp;
            return client.LoadRemote(req, rsp);
        };

        /* fetch task from channel, execute, push result to res_ch */
        for (auto ele : ch) {
            auto ret = loadEach(ele);
            if (!ret) {
                LOG_ERROR("batch-load {} failed", ele.String());
            }
            ret >> res_ch;
        }
        LOG_INFO("channel has been closed, bye...");
    };

    /* use two channel to make sure function exits after all tasks are done */
    channel<api::InterNodeLoadResponse> ch;
    channel<bool> res_ch;

    /* use 8 threads for concurrent load */
    for (auto i = 0; i < config::BOOTSTRAP_CONCURRENT_THREADS; i++) {
        std::thread(loadFunc, std::ref(ch), std::ref(res_ch)).detach();
    }

    /* add tasks into channel in a separate thread */
    std::thread([](channel<api::InterNodeLoadResponse> &ch, api::InterNodeBatchLoadResponse &rsp) {
        for (auto &item : rsp.responses) {
            item >> ch;
        }
        ch.close();
    },
                std::ref(ch), std::ref(rsp))
        .detach();

    /* fetch results */
    bool res = true;
    for (size_t i = 0; i < rsp.responses.size(); i++) {
        bool tmp_res;
        tmp_res << res_ch;
        if (!tmp_res) {
            res = false;
        }
    }
    res_ch.close();

    if (!res) {
        LOG_ERROR("batch load failed!");
        return false;
    }

    LOG_TRACE("end of inter-node batch-load request");
    return true;
}

bool ClientUtil::BatchLoadFromFileSystem() {
    api::BatchLoadFilter filter(config::WorldState::Instance().NodeRank());
    std::vector<api::Metadata> vec;
    auto metaClient = storage::MetadataClientFactory::GetClient();
    auto rc = metaClient->BatchLoad(filter, vec);
    if (rc == api::STATUS_UNKNOWN_ERROR) {
        LOG_ERROR("get AllMetadata failed");
        return false;
    }

    for (auto &metadata : vec) {
        if (metadata.state == api::CheckpointState::OBSOLESCENT) {
            continue;
        }
        // recover _lastIteration and _totalIteration
        auto iteration = metadata.iteration;
        if (iteration != "unknown" && !IterationManager::Instance().isExist(std::stoul(iteration))) {
            IterationManager::Instance().pushIteration(std::stoul(iteration));
        }
        api::DataEntry entry;
        auto rc = MemoryMonitor::Instance().TryLoadFromFile(std::ref(metadata), std::ref(entry));
        if (!api::IsSuccess(rc)) {
            LOG_ERROR("memfdCalloc failed");
            return false;
        }
        if (!storage::Storage::Instance().Save(std::ref(metadata), std::ref(entry))) {
            LOG_ERROR("failed to add <{}> into storage", metadata.String());
            return false;
        }
    }
    return true;
}

bool ClientUtil::NotifyBackup(api::InterNodeNotifyBackupResponse &rsp) {
    LOG_TRACE("begin of notify backup request");
    buffer::Buffer buffer;

    /* init communicator */
    auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_RDMA);
    std::string remoteIP;
    if (!getPrevNode(std::ref(remoteIP))) {
        return false;
    }
    ep.setAddr(remoteIP);
    auto communicator = RdmaCommunicator(ep);
    if (!communicator.Connect()) {
        return false;
    }

    /* send routine */
    size_t routine = static_cast<size_t>(api::Routine::INTER_NODE_NOTIFY_BACKUP);
    buffer.Add(routine);
    if (!communicator.Write(std::ref(buffer))) {
        LOG_ERROR("cannot send notify backup request");
        return false;
    }
    LOG_TRACE("routine {} sent", api::RoutineString((api::Routine)routine));

    /* wait for response */
    buffer.Reset();
    if (!communicator.Read(std::ref(buffer))) {
        LOG_ERROR("cannot receive notify backup response");
        return false;
    }
    rsp.Unmarshal(std::ref(buffer));

    /* check response code */
    if (rsp.code == api::STATUS_UNKNOWN_ERROR) {
        LOG_ERROR("response code {}", rsp.code);
        return false;
    }

    LOG_TRACE("end of notify backup request");
    return true;
}

bool ClientUtil::getNextNode(std::string &addr) {
    auto &world = WorldState::Instance();
    auto host = world.Hosts()[((world.NodeRank() + 1) % world.WorldSize())];
    if (Util::ResolveHostname(std::ref(host), std::ref(addr)) != 0) {
        LOG_ERROR("resolve host {} error", host);
        return false;
    }
    LOG_TRACE("next host {} IP {}", host, addr);
    return true;
}

bool ClientUtil::getPrevNode(std::string &addr) {
    auto &world = WorldState::Instance();
    auto prevRank = world.NodeRank() == 0 ? world.WorldSize() - 1 : world.NodeRank() - 1;
    std::string host = world.Hosts()[prevRank];
    if (Util::ResolveHostname(std::ref(host), std::ref(addr)) != 0) {
        LOG_ERROR("resolve host {} error", host);
        return false;
    }
    LOG_TRACE("prev host {} IP {}", host, addr);
    return true;
}

bool ClientUtil::getNodeIP(int nodeRank, std::string &addr) {
    auto &world = WorldState::Instance();
    if (nodeRank < 0 || nodeRank >= world.WorldSize()) {
        LOG_ERROR("expect rank [0, {}), get {}", world.WorldSize(), nodeRank);
        return false;
    }
    auto host = world.Hosts()[nodeRank];
    if (Util::ResolveHostname(std::ref(host), std::ref(addr)) != 0) {
        LOG_ERROR("resolve host {} error", host);
        return false;
    }
    LOG_TRACE("prev host {} IP {}", host, addr);
    return true;
}
