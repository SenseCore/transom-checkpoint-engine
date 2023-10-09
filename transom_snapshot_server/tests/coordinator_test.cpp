/**
 * @file coordinator_test.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <memory>
#include <thread>
#include <vector>

#include "communicator/communicator.h"
#include "coordinator/client.h"
#include "coordinator/coordinator.h"
#include "coordinator/server.h"
#include "logger/logger.h"
#include "monitor/monitor.h"
#include "storage/metadata.h"
#include "util/util.h"

constexpr auto rawData = "I'm building castle in the air";
size_t dataSize = strlen(rawData) + 1;

void load() {
    coordinator::ClientUtil client;
    api::InterNodeLoadRequest req2;
    api::InterNodeLoadResponse rsp2;
    req2.metadata.file_name = std::string("test");
    if (!client.LoadRemote(std::ref(req2), std::ref(rsp2))) {
        LOG_ERROR("load failed");
        return;
    }
    LOG_INFO("read data is: {}", reinterpret_cast<char *>(rsp2.data_entry.address));
}

int main(int argc, char **argv) {
    logger::Logger::InitLogger();

    bool isServer = static_cast<bool>(std::atoi(util::Util::GetEnv("SERVER", "0").c_str()));
    if (isServer) {
        /* start memory monitor */
        monitor::MemoryMonitor::Instance().Start();

        /* start a operator for reconciliation */
        auto controller = std::make_shared<operators::Operator>();
        controller->SetHandler(coordinator::Coordinator::Reconcile);
        controller->Run();

        /* start inter node server */
        coordinator::Coordinator coordinator(controller);
        coordinator.Run();

        std::this_thread::sleep_for(std::chrono::seconds(10000000000));
    } else {
        /* client backup */
        coordinator::ClientUtil client;
        // 1GB
        // auto size = 1073741824;
        auto size = dataSize;
        auto data = reinterpret_cast<char *>(calloc(sizeof(char), size));
        memcpy(data, rawData, dataSize);
        api::Metadata meta("test", "test", 0, "iter0", api::CheckpointState::CACHED, size);
        api::DataEntry entry(reinterpret_cast<size_t>(data), 0, 0);
        api::InterNodeBackupRequest req(meta, entry, false);
        api::InterNodeBackupResponse rsp;
        if (!client.Backup(std::ref(req), std::ref(rsp))) {
            LOG_ERROR("backup failed");
            return 0;
        }
        free(data);

        // /* client load */
        // auto t1 = std::thread(load);
        // auto t2 = std::thread(load);
        // t1.join();
        // t2.join();

        // /* batch load */
        // api::InterNodeBatchLoadRequest req2(0);
        // api::InterNodeBatchLoadResponse rsp2;
        // api::InterNodeBatchLoadExternalState state2;
        // if (!client.BatchLoadRemote(std::ref(req2), std::ref(rsp2), std::ref(state2))) {
        //     LOG_ERROR("batch load failed");
        //     return 0;
        // }
        // LOG_INFO("response is {}", rsp2.String());
    }

    return 0;
}
