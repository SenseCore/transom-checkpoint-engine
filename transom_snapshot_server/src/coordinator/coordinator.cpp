/**
 * @file coordinator.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "coordinator/coordinator.h"

#include "api/api.h"
#include "storage/storage.h"

using coordinator::Coordinator;
using coordinator::ClientUtil;
using config::WorldState;

Coordinator::Coordinator(std::shared_ptr<operators::Operator> controller) {
    controller_ = controller;
    s_ = std::make_shared<Server>(controller);
}

void Coordinator::Run() {
    std::thread([this]() { s_->Serve(); }).detach();
    LOG_INFO("coordinator server started");
    bootstrap();
}

void Coordinator::bootstrap() {
    if (util::Util::GetEnv(config::ENV_KEY_SKIP_BOOTSTRAP, "off") == config::EXPERIMENTAL_SKIP_BOOTSTRAP) {
        return;
    }

    if (auto size = config::WorldState::Instance().WorldSize(); size < 2) {
        LOG_WARN("world size is {}, skip bootstrap", size);
        return;
    }
    LOG_INFO("---------------------------------");
    LOG_INFO("          bootstrap start");
    LOG_INFO("---------------------------------");
    auto start_time = std::chrono::high_resolution_clock::now();
    /* retrieve ckpt from next node */
    auto t1 = std::thread([this]() {
        int wait_time = config::BOOTSTRAP_MIN_RETRY_INTERVAL_SECONDS;
        while (wait_time <= config::BOOTSTRAP_MAX_RETRY_INTERVAL_SECONDS) {
            if (retriveCheckpoint()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(wait_time));
            wait_time *= 2;
        }
        retriveCheckpointFromFileSystem();
    });

    /* ask prev node to backup ckpt */
    auto t2 = std::thread([this]() {
        int wait_time = config::BOOTSTRAP_MIN_RETRY_INTERVAL_SECONDS;
        while (true) {
            if (triggerCheckpoint()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(wait_time));
            if (wait_time <= config::BOOTSTRAP_MAX_RETRY_INTERVAL_SECONDS) {
                wait_time *= 2;
            }
        }
    });

    t1.join();
    t2.join();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    LOG_INFO("---------------------------------");
    LOG_INFO("bootstrap succeess! elapsed {}s", diff.count());
    LOG_INFO("---------------------------------");
}

bool Coordinator::retriveCheckpoint() {
    LOG_INFO("try retrive checkpoint from next node");
    coordinator::ClientUtil client;

    api::InterNodeBatchLoadRequest req(config::WorldState::Instance().NodeRank(),
                                       "", api::CheckpointState::STATE_ANY, false);
    api::InterNodeBatchLoadResponse rsp;
    if (!client.BatchLoadRemote(std::ref(req), std::ref(rsp))) {
        LOG_WARN("failed to retrive checkpoint from next node, retry...");
        return false;
    }
    LOG_INFO("successfully retrived checkpoints from next node");
    return true;
}

bool Coordinator::retriveCheckpointFromFileSystem() {
    LOG_INFO("try retrive checkpoint from FileSystem");
    coordinator::ClientUtil client;
    if (!client.BatchLoadFromFileSystem()) {
        LOG_WARN("failed to retrive checkpoint from FileSystem");
        return false;
    }
    LOG_INFO("successfully retrived checkpoints from FileSystem");
    return true;
}

bool Coordinator::triggerCheckpoint() {
    LOG_INFO("try retrive checkpoint from next node");
    coordinator::ClientUtil client;

    api::InterNodeNotifyBackupResponse rsp;
    if (!client.NotifyBackup(std::ref(rsp))) {
        LOG_WARN("cannot notify prev node to backup, retry...");
        return false;
    }
    LOG_INFO("successfully notify prev node to backup checkpoints");
    return true;
}

bool Coordinator::Reconcile(std::string key) {
    LOG_INFO("start reconcile {}", key);

    auto world_size = WorldState::Instance().WorldSize();
    api::Metadata metadata(WorldState::Instance().JobName(), key);
    api::DataEntry entry;
    auto meta_client = storage::MetadataClientFactory::GetClient();
    int rc = -1;

    /* get metadata and data from local storage */
    rc = meta_client->Load(std::ref(metadata));
    if (!api::IsSuccess(rc)) {
        if (api::IsNotFound(rc)) {
            LOG_WARN("primary key {} not found in database, no longer reconcile");
            return true;
        }
        LOG_ERROR("load metadata failed, retry...");
        return false;
    }

    /* if data is backup data, does not need reconciliation unless it's obsolescent */
    if (metadata.node_rank != WorldState::Instance().NodeRank()
        && metadata.state != api::CheckpointState::OBSOLESCENT) {
        LOG_INFO("file {} does not belong to current node, it's backup file, skip reconciliation",
                 metadata.file_name);
        return true;
    }

    /* check if data is complete */
    auto checkDataCompletion = [](api::Metadata &metadata, api::DataEntry &entry) -> bool {
        if (metadata.size == 0) {
            LOG_ERROR("INTERNAL ERROR! data size is 0 in reconciliation!");
            return false;
        }
        if (!storage::Storage::Instance().Load(std::ref(metadata), std::ref(entry))) {
            LOG_ERROR("INTERNAL ERROR! data pointer not found in reconciliation!");
            return false;
        }
        return true;
    };

    /* update state to given state */
    auto updateState = [meta_client](api::Metadata metadata, api::CheckpointState state) -> bool {
        if (metadata.state == state) {
            return true;
        }
        auto rc = meta_client->UpdateState(metadata.file_name, state);
        if (!api::IsSuccess(rc)) {
            LOG_ERROR("cannot update state of metadata {} to {}",
                      metadata.file_name, api::CheckpointStateString(state));
            return false;
        }
        return true;
    };

    /* always check if data is complete */
    if (!checkDataCompletion(std::ref(metadata), std::ref(entry))) {
        // broken but OBSOLESCENT
        if (metadata.state == api::CheckpointState::OBSOLESCENT) {
            return true;
        }
        LOG_ERROR("data of {} is incomplete, state:{}, mark it broken",
                  metadata.file_name, api::CheckpointStateString(metadata.state));
        if (!updateState(std::ref(metadata), api::CheckpointState::BROKEN)) {
            LOG_ERROR("failed to update state of {} to broken", metadata.file_name);
            return false;
        }
        /* do not enqueue again, broken data will not be reconciled */
        return true;
    }
    LOG_INFO("data of {} is complete, state is {}", metadata.file_name, CheckpointStateString(metadata.state));

    /* here we define handlers for different states */
    auto backUp = [](api::Metadata &metadata, api::DataEntry &entry, bool only_metadata) -> bool {
        api::InterNodeBackupRequest req(metadata, entry, only_metadata);
        api::InterNodeBackupResponse rsp;
        ClientUtil remoteClient;
        if (!remoteClient.Backup(std::ref(req), std::ref(rsp))) {
            LOG_ERROR("backup ckpt {} to remote node error", metadata.file_name);
            return false;
        }
        return true;
    };

    /* write to disk. TODO: support AOSS */
    auto persistence = [](api::Metadata &metadata, api::DataEntry &entry) -> bool {
        if (metadata.node_rank != WorldState::Instance().NodeRank()) { /* backup data only in memory */
            return true;
        }
        if (!storage::Persistence::Instance().WriteToDisk(
                metadata.file_name, (const void *)entry.address, metadata.size)) {
            return false;
        }
        return true;
    };

    auto deleteCkpt = [](api::Metadata &metadata) -> bool {
        if (!storage::Storage::Instance().Delete(std::ref(metadata))) {
            LOG_ERROR("failed to remove key {} from storage", metadata.file_name);
            return false;
        }
        return true;
    };

    auto start_time = std::chrono::high_resolution_clock::now();
    bool do_not_requeue = false;

    /*
     * do one thing at a time, it let us do fast backup and long-tail persistent
     * if data is
     *  - PENDING: do nothing(actually will not happen)
     *  - CACHED: backup            -> BACKED_UP
     *  - BACKED_UP: persistent     -> PERSISTENT
     *  - PERSISTENT: do nothing
     *  - OBSOLESCENT: delete file and notify next node to delete file
     *  - BROKEN: what can I do?
     */

    api::CheckpointState to_update_state = api::CheckpointState::STATE_ANY; /* delcare ahead in cater to compiler */

    switch (metadata.state) {
    case api::CheckpointState::PENDING:
        LOG_INFO("ignore pending checkpoint...", metadata.file_name);
        do_not_requeue = true;
        break;

    case api::CheckpointState::CACHED: /* backup */
        if (world_size < 2) {
            if (util::Util::GetEnv(config::IS_PERSISTENT, "on") == "off") {
                LOG_DEBUG("skip persistent {}", metadata.file_name);
                // If the user chooses not persistent, in order to compatible with DeepSpeed,
                // we need to create an empty file
                auto fp = fopen(metadata.file_name.c_str(), "a"); /* append or create */
                if (!fp) {
                    LOG_ERROR("failed to open or create file {} error: {}, you may not have permission to create it",
                              metadata.file_name, strerror(errno));
                }
                do_not_requeue = true;
                break;
            }
            LOG_INFO("start persistent {}", metadata.file_name);
            /* just persistent ckpt */
            if (!persistence(std::ref(metadata), std::ref(entry))) {
                LOG_ERROR("persistence {} failed", metadata.file_name);
                break;
            }
        } else {
            LOG_INFO("start backup {} to other nodes", metadata.file_name);
            /* backup and update state to BACKED_UP */
            if (!backUp(std::ref(metadata), std::ref(entry), false)) {
                LOG_ERROR("failed to backup {}", metadata.file_name);
                // FIX will keep retrying when failed
                std::this_thread::sleep_for(std::chrono::seconds(3));
                break;
            }
        }

        /* udpate state according to world size */
        to_update_state = world_size > 1 ? api::CheckpointState::BACKED_UP : api::CheckpointState::PERSISTENT;
        if (!updateState(std::ref(metadata), to_update_state)) {
            LOG_ERROR("cannot update {} state to {}", metadata.file_name, to_update_state);
        }
        LOG_INFO("re-enqueue {}...", metadata.file_name);
        /* always re-enqueue */
        break;

    case api::CheckpointState::BACKED_UP: /* persistent */
        LOG_INFO("start persistent {}", metadata.file_name);
        if (util::Util::GetEnv(config::IS_PERSISTENT, "on") == "off") {
            LOG_DEBUG("skip persistent {}", metadata.file_name);
            // If the user chooses not persistent, in order to compatible with DeepSpeed,
            // we need to create an empty file
            auto fp = fopen(metadata.file_name.c_str(), "a"); /* append or create */
            if (!fp) {
                LOG_ERROR("failed to open or create file {} error: {}, you may not have permission to create it",
                          metadata.file_name, strerror(errno));
            }
            do_not_requeue = true;
            break;
        }
        /* persistent */
        if (!persistence(std::ref(metadata), std::ref(entry))) {
            LOG_ERROR("persistence {} failed", metadata.file_name);
            do_not_requeue = true;
            break;
        }
        LOG_INFO("file {} persistent", metadata.file_name);

        /* update state to persistent */
        to_update_state = api::CheckpointState::PERSISTENT;
        if (!updateState(std::ref(metadata), to_update_state)) {
            LOG_ERROR("cannot update {} state to {}", metadata.file_name, api::CheckpointStateString(to_update_state));
        }
        LOG_INFO("update state of {} to PERSISTENT, re-enqueue...", metadata.file_name);
        break;

    case api::CheckpointState::PERSISTENT:
        LOG_DEBUG("ignore persistent ckpt {}", metadata.file_name);
        do_not_requeue = true;
        break;

    case api::CheckpointState::OBSOLESCENT: /* delete data */
        LOG_INFO("ckpt {} is OBSOLESCENT, delete file or in-memory backup", metadata.file_name);
        if (world_size > 1 && metadata.node_rank == WorldState::Instance().NodeRank()) {
            /* send a backup request, so that key is added to workqueue */
            if (!backUp(std::ref(metadata), std::ref(entry), true)) {
                LOG_ERROR("failed to backup {}", metadata.file_name);
                // FIX will keep retrying when failed
                std::this_thread::sleep_for(std::chrono::seconds(3));
                break;
            }
        }
        /* delete local data */
        if (!deleteCkpt(std::ref(metadata))) {
            LOG_ERROR("failed to delete ckpt of key {}", metadata.file_name);
            break;
        }
        /* file has been deleted, no longer reconcile */
        do_not_requeue = true;
        break;

    case api::CheckpointState::BROKEN:
        LOG_ERROR("file {} has been broken, no longer reconcile it!", metadata.file_name);
        do_not_requeue = true;
        break;

    default:
        LOG_FATAL("FATAL: abnormal checkpoint state {}", CheckpointStateString(metadata.state));
    }

    auto timeval = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    if (to_update_state != api::CheckpointState::STATE_ANY) {
        LOG_DEBUG("reconcile {} with state {} → {} finishes, spend {} ms", metadata.file_name,
                  CheckpointStateString(metadata.state), CheckpointStateString(to_update_state), timeval.count());
    }
    return do_not_requeue;
}
