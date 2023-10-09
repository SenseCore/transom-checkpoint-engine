/**
 * @file server.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-04
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "coordinator/server.h"

#include "api/api.h"
#include "coordinator/client.h"
#include "monitor/monitor.h"
#include "storage/storage.h"
#include "util/channel.h"
#include "util/util.h"

using coordinator::Server;
using util::Util;
using util::channel;
using communicators::CommunicatorFactory;
using communicators::RdmaCommunicator;
using storage::Storage;
using monitor::MemoryMonitor;

Server::Server(std::shared_ptr<operators::Operator> controller) {
    controller_ = controller;
    communicator_ = CommunicatorFactory::getRdmaCommunicator();
}

void Server::Serve() {
    communicator_->Serve();
    while (true) {
        auto c = communicator_->Accept();
        std::thread([this, c]() { this->execute(c); }).detach();
    }
}

/**
 * @brief endlessly handle requests
 *
 * @param c accepted communicator at server
 */
void Server::execute(std::shared_ptr<RdmaCommunicator> c) {
    auto tid = Util::GetThreadID();

    size_t routine = -1;
    buffer::Buffer buffer;

    while (c->Read(std::ref(buffer))) {
        if (auto size = buffer.GetBufferSize(); size != sizeof(size_t)) {
            LOG_ERROR("expect reading 8 bytes from client which stores routine, receive {} bytes", size);
            break;
        }
        routine = buffer.Get<size_t>();
        buffer.Reset();
        LOG_INFO("routine {} thread {} enter execution loop",
                 api::RoutineString(static_cast<api::Routine>(routine)), tid);
        switch (routine) {
        case static_cast<size_t>(api::Routine::INTER_NODE_BACKUP):
            handleBackup(c);
            break;
        case static_cast<size_t>(api::Routine::INTER_NODE_LOAD):
            handleLoad(c);
            break;
        case static_cast<size_t>(api::Routine::INTER_NODE_BATCH_LOAD):
            handleBatchLoad(c);
            break;
        case static_cast<size_t>(api::Routine::INTER_NODE_NOTIFY_BACKUP):
            handleNotifyBackup(c);
            break;
        default:
            LOG_ERROR("routine {} undefined", routine);
            goto clean;
        }
    }

clean:
    LOG_INFO("thread {} leave execution loop", tid);
}

void Server::handleBackup(std::shared_ptr<RdmaCommunicator> c) {
    LOG_TRACE("begin of handle inter-node backup");
    buffer::Buffer buffer;

    /* recv buffer from socket */
    if (!c->Read(std::ref(buffer))) {
        LOG_ERROR("recv inter-node backup request");
        return;
    }
    LOG_TRACE("recved inter-node backup request");

    /* unmarshal request */
    api::InterNodeBackupRequest req;
    req.Unmarshal(std::ref(buffer));
    LOG_DEBUG("inter-node backup req: {}", req.String());

    /* prepare response */
    api::InterNodeBackupResponse rsp;
    rsp.code = api::STATUS_SUCCESS;

    /* in case memory is not enough */
    if (!req.only_metadata) {
        auto mem_stat = monitor::MemoryMonitor::Instance().GetMemoryStat();
        if (mem_stat.total_idle < req.metadata.size) {
            LOG_WARN("rdma: alloc {} bytes data will cause OOM, only {} idle memory!",
                     req.metadata.size, mem_stat.total_idle);
            rsp.code = api::STATUS_UNKNOWN_ERROR;
        }
    }

    /* send response, only continue if response code is 0 */
    buffer.Reset();
    rsp.Marshal(std::ref(buffer));
    if (!c->Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node backup response");
        return;
    }
    if (rsp.code != api::STATUS_SUCCESS) {
        return;
    }

    /* Now we guarantee metadata has been updated. deal with data */
    if (!req.only_metadata) {
        // In order to directly from the backup file torch.load, use memfdCalloc instead of Calloc
        api::DataEntry entry;
        if (!storage::Storage::Instance().Load(std::ref(req.metadata), std::ref(entry))) {
            LOG_DEBUG("{} doesn't exists, memfdCalloc", req.metadata.file_name);
            auto rc = MemoryMonitor::Instance().TryMemfdMalloc(std::ref(req.metadata), std::ref(entry));
            if (api::IsOOM(rc)) {
                LOG_ERROR("memfdCalloc failed: out of memory");
            }
            if (!api::IsSuccess(rc)) {
                LOG_ERROR("memfdCalloc failed: unkonwn error");
                return;
            }
            Storage::Instance().Save(req.metadata, entry);
        } else {
            LOG_DEBUG("{} already exists, ftruncate to reuse memfd", req.metadata.file_name);
            auto rc = Util::memfdFtruncate(std::ref(req.metadata), std::ref(entry));
            if (!api::IsSuccess(rc)) {
                LOG_ERROR("memfdFtruncate failed");
                return;
            }
        }
        auto rc = c->rdma_handshake(true, entry.address, req.metadata.size);
        if (!api::IsSuccess(rc)) {
            LOG_ERROR("rdma handshake failed for address {}", (void *)entry.address);
            return;
        }

        /* wait for recv signal */
        buffer.Reset();
        if (!c->Read(std::ref(buffer))) {
            LOG_ERROR("receive rdma_write finish notification");
            return;
        }
        if (auto sign = buffer.GetString(); sign != config::RDMA_WRITE_MSG) {
            LOG_FATAL("internal fatal error! rdma write finish notification mismatch, expect {}, get {}",
                      config::RDMA_WRITE_MSG, sign);
        }
        LOG_TRACE("receive rdma write finish notification");

        /* validate if memory has been written */
        LOG_TRACE("saved into storage, address {}", (void *)entry.address);
    }

    /* at last add to workqueue, just for testing */
    controller_->AddRateLimited(req.metadata.file_name);

    LOG_TRACE("end of handle inter-node backup");
}

void Server::handleLoad(std::shared_ptr<RdmaCommunicator> c) {
    LOG_TRACE("begin of handle inter-node load");

    buffer::Buffer buffer;
    if (!c->Read(std::ref(buffer))) {
        LOG_ERROR("read inter-node load request");
        return;
    }

    /* unmarshal request */
    api::InterNodeLoadRequest req;
    req.Unmarshal(std::ref(buffer));
    LOG_DEBUG(req.String());

    /* prepare response */
    api::InterNodeLoadResponse rsp;
    rsp.code = api::STATUS_SUCCESS;

    /* load metadata */
    api::Metadata metadata(req.metadata);
    auto meta_client = storage::MetadataClientFactory::GetClient();
    auto rc = meta_client->Load(std::ref(metadata));
    if (!api::IsSuccess(rc)) {
        LOG_ERROR("load metadata failed");
        rsp.code = rc;
    } else {
        rsp.metadata = metadata;
    }

    /* load entry if necessary */
    if (!req.only_metadata) {
        api::DataEntry entry;
        if (!Storage::Instance().Load(std::ref(metadata), std::ref(entry))) {
            LOG_ERROR("load from storage");
            rsp.code = api::STATUS_UNKNOWN_ERROR;
        } else {
            rsp.data_entry = entry;
        }
    }

    /* send response */
    buffer.Reset();
    rsp.Marshal(std::ref(buffer));
    if (!c->Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node load response");
        return;
    }
    if (rsp.code != api::STATUS_SUCCESS) {
        return;
    }

    /* rdma handshake */
    if (req.only_metadata) {
        return;
    }
    if (auto rc = c->rdma_handshake(true, rsp.data_entry.address, rsp.metadata.size); !api::IsSuccess(rc)) {
        LOG_ERROR("rdma handshake failed for address {}", (void *)rsp.data_entry.address);
        return;
    }

    /* wait client rdma read succeess */
    buffer.Reset();
    if (!c->Read(std::ref(buffer))) {
        LOG_ERROR("receive read finish notification");
        return;
    }
    if (auto sign = buffer.GetString(); sign != config::RDMA_READ_MSG) {
        LOG_FATAL("internal fatal error! rdma write finish notification mismatch, expect {}, get {}",
                  config::RDMA_READ_MSG, sign);
    }
    LOG_TRACE("receive rdma read finish notification");
    LOG_TRACE("end of handle inter-node load");
}

void Server::handleBatchLoad(std::shared_ptr<RdmaCommunicator> c) {
    LOG_TRACE("begin of handle inter-node batch-load");

    buffer::Buffer buffer;
    if (!c->Read(std::ref(buffer))) {
        LOG_ERROR("read inter-node batch-load request");
        return;
    }

    /* unmarshal request */
    api::InterNodeBatchLoadRequest req;
    req.Unmarshal(std::ref(buffer));
    LOG_DEBUG(req.String());

    /* prepare response */
    api::InterNodeBatchLoadResponse rsp;
    rsp.code = api::STATUS_SUCCESS;

    /* load metadata */
    std::vector<api::Metadata> vec;
    auto meta_client = storage::MetadataClientFactory::GetClient();
    auto rc = meta_client->BatchLoad(std::ref(req.filter), std::ref(vec));
    if (api::IsNotFound(rc)) {
        LOG_INFO("batch-load 0 metadata, continue");
        rsp.code = rc;
    } else if (!api::IsSuccess(rc)) {
        LOG_ERROR("batch-load metadata failed");
        rsp.code = rc;
    } else {
        for (auto &item : vec) {
            // dont load OBSOLESCENT ckpt
            if (item.state == api::CheckpointState::OBSOLESCENT) {
                continue;
            }

            api::InterNodeLoadResponse data(item);
            /* load entry if necessary */
            if (!req.only_metadata) {
                if (!Storage::Instance().Load(item, std::ref(data.data_entry))) {
                    LOG_ERROR("batch-load from storage, data of file {} not exist", item.file_name);
                    rsp.code = api::STATUS_UNKNOWN_ERROR;
                    break;
                }
            }
            rsp.responses.push_back(data);
        }
    }

    /* send response */
    buffer.Reset();
    rsp.Marshal(std::ref(buffer));
    if (!c->Write(std::ref(buffer))) {
        LOG_ERROR("send inter-node batch-load response");
        return;
    }
    LOG_DEBUG("sent inter-node batch-load response: {}", rsp.String());
    if (rsp.code != api::STATUS_SUCCESS) {
        return;
    }

    /* rdma handshake */
    if (req.only_metadata) {
        return;
    }
    LOG_TRACE("end of handle inter-node batch-load");
}

void Server::handleNotifyBackup(std::shared_ptr<RdmaCommunicator> c) {
    LOG_TRACE("begin of handle inter-node notify backup");

    /* prepare response early */
    api::InterNodeNotifyBackupResponse rsp;

    /* load metadata locally */
    api::BatchLoadFilter filter(WorldState::Instance().NodeRank(), "", api::CheckpointState::STATE_ANY);
    std::vector<api::Metadata> vec;
    auto meta_client = storage::MetadataClientFactory::GetClient();
    auto rc = meta_client->BatchLoad(std::ref(filter), std::ref(vec));
    if (api::IsNotFound(rc)) {
        LOG_INFO("batch-load 0 metadata, continue");
        rsp.code = rc;
    } else if (!api::IsSuccess(rc)) {
        LOG_ERROR("batch-load metadata failed");
        rsp.code = rc;
    }

    /* do not backup before retrived complete checkpoints */
    size_t need_backup_checkpoint_num = 0;
    for (auto metadata : vec) {
        if (metadata.state == api::CheckpointState::BACKED_UP || metadata.state == api::CheckpointState::PERSISTENT) {
            need_backup_checkpoint_num++;
        }
    }
    if (storage::Storage::Instance().getDict().empty()
        || need_backup_checkpoint_num != storage::Storage::Instance().getDict().size()) {
        LOG_ERROR("need_backup_checkpoint_num {} dict size {}", need_backup_checkpoint_num,
                  storage::Storage::Instance().getDict().size());
        rsp.code = api::STATUS_UNKNOWN_ERROR;
        return;
    }

    /* use two channel to ensure exit after all tasks are done */
    channel<api::Metadata> ch;
    channel<bool> res_ch;

    auto backupFunc = [](channel<api::Metadata> &ch, channel<bool> &res_ch) {
        /* backup each file */
        auto backupEach = [](api::Metadata metadata) -> bool {
            api::DataEntry entry;
            if (!storage::Storage::Instance().Load(std::ref(metadata), std::ref(entry))) {
                LOG_ERROR("cannot load {} from storage", metadata.file_name);
                return false;
            }
            if (metadata.state == api::CheckpointState::OBSOLESCENT) {
                return true;
            }
            ClientUtil client;
            api::InterNodeBackupRequest req(metadata, entry, false);
            api::InterNodeBackupResponse rsp;
            if (!client.Backup(std::ref(req), std::ref(rsp))) {
                LOG_ERROR("cannot backup {} to next node", metadata.file_name);
                return false;
            }
            LOG_DEBUG("successfully backup {}", metadata.String());
            return true;
        };

        /* receive task from channel, execute, push to res_ch */
        for (auto metadata : ch) {
            auto ret = backupEach(metadata);
            if (!ret) {
                LOG_ERROR("batch-backup {} failed", metadata.String());
            }
            ret >> res_ch;
        }
        LOG_INFO("channel has been closed, bye...");
    };

    /* use 8 threads for concurrent backup */
    for (auto i = 0; i < config::BOOTSTRAP_CONCURRENT_THREADS; i++) {
        std::thread(backupFunc, std::ref(ch), std::ref(res_ch)).detach();
    }

    /* add tasks in a separate thread */
    std::thread([vec](channel<api::Metadata> &ch) {
        for (auto metadata : vec) {
            metadata >> ch;
        }
        ch.close();
    },
                std::ref(ch))
        .detach();

    /* receive results */
    bool res = true;
    for (size_t i = 0; i < vec.size(); i++) {
        bool tmp_res;
        tmp_res << res_ch;
        if (!tmp_res) {
            res = false;
        }
    }
    res_ch.close();

    if (!res) {
        rsp.code = api::STATUS_UNKNOWN_ERROR;
    }

    /* send response */
    buffer::Buffer buffer;
    rsp.Marshal(std::ref(buffer));
    if (!c->Write(std::ref(buffer))) {
        LOG_ERROR("cannot send inter-node notify backup response");
    }

    LOG_TRACE("end of handle inter-node notify backup");
}
