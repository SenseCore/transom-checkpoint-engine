/**
 * @file remote_file_loader.h
 * @author BaldStrong (BaldStrong@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "config/world.h"
#include "coordinator/client.h"
#include "logger/logger.h"
#include "storage/storage.h"
#include "util/channel.h"
#include "util/util.h"

namespace communicators {
using util::Util;
using storage::Storage;
using config::WorldState;

/**
 * @brief helper util for loading checkpoint cache from remote nodes.
 * @details multiple ranks may require the same model state from remote node, so it's necessary to de-duplicate.
 * This class wraps a thread-safe channel for async loading cache, which is inspired by kubernetes operator.
 */
class remoteFileLoader {
private:
    struct keyAndNode {
        std::string key;
        int rank;
    };

    /*
     * all ranks may require the same model state, load remotely once is enough. reqCh accepts client request
     */
    util::channel<keyAndNode> req_ch_{4};
    std::unordered_map<std::string, bool> ongoing_files_;
    std::shared_mutex rw_mutex_;

    void safeInsert(std::string key, bool val, bool &already_exist) {
        rw_mutex_.lock();
        auto iter = ongoing_files_.find(key);
        if (iter != ongoing_files_.end()) {
            already_exist = true;
            return;
        }
        already_exist = false;
        ongoing_files_.insert_or_assign(key, val);
        rw_mutex_.unlock();
    }

    void safeUpdate(std::string key, bool val) {
        rw_mutex_.lock();
        ongoing_files_.insert_or_assign(key, val);
        rw_mutex_.unlock();
    }

    void safeRead(std::string key, bool &val, bool &exist) {
        rw_mutex_.lock_shared();
        auto iter = ongoing_files_.find(key);
        if (iter == ongoing_files_.end()) {
            exist = false;
        } else {
            exist = true;
            val = iter->second;
        }
        rw_mutex_.unlock_shared();
    }

    /* erase entry so that blocking client can enqueue key again(mostly for retry on error) */
    void safeErase(std::string key) {
        rw_mutex_.lock();
        ongoing_files_.erase(key);
        rw_mutex_.unlock();
    }

    /**
     * @brief finally key is filename, val is true, indicating file is already in shm
     */
    void startReconcile() {
        auto reconcile = [this]() {
            coordinator::ClientUtil client;

            for (auto item : req_ch_) {
                auto file_name = item.key;
                if (auto iter = ongoing_files_.find(file_name); iter != ongoing_files_.end()) {
                    LOG_TRACE("client request to read file {}, workqueue already processing, wait...", file_name);
                    continue;
                }
                bool already_exist;
                safeInsert(file_name, false, std::ref(already_exist));
                if (already_exist) {
                    continue;
                }

                /* read from remote into shm directly */
                api::Metadata metadata(config::WorldState::Instance().JobName(), file_name,
                                       item.rank, "", api::STATE_ANY);
                api::InterNodeLoadRequest req(metadata, false);
                api::InterNodeLoadResponse rsp;

                if (!client.LoadRemote(std::ref(req), std::ref(rsp)) || rsp.code != api::STATUS_SUCCESS) {
                    LOG_ERROR("failed to load {}", file_name);
                    safeErase(file_name);
                    continue;
                }
                safeUpdate(file_name, true);
                LOG_DEBUG(" loaded {} from remote and written to /dev/shm, ", file_name);
            }
        };

        for (auto i = 0; i < 1; i++) {
            std::thread(reconcile).detach();
        }
    }

public:
    /**
     * @brief upon construction, start the reconciliation thread
     */
    remoteFileLoader() {
        startReconcile();
    }
    ~remoteFileLoader() = default;
    remoteFileLoader(const remoteFileLoader &) = delete;
    remoteFileLoader(remoteFileLoader &&) = delete;
    remoteFileLoader &operator=(const remoteFileLoader &) = delete;
    remoteFileLoader &operator=(remoteFileLoader &&) = delete;

    /**
     * @brief this class is singleton, call Instance() to get the reference of instance.
     * This reference cannot be copied or destroyed.
     * @return reference of singleton instance
     */
    static remoteFileLoader &Instance() {
        static std::unique_ptr<remoteFileLoader> instance_ptr_(new remoteFileLoader());
        return *instance_ptr_;
    }

    /**
     * @brief a blocking method without timeout, waiting for given checkpoint file is loaded from remote node to local memory
     * @param file_name checkpoint file name
     */
    void WaitUntilFileReady(std::string file_name) {
        bool exist = false;
        bool ready = false;
        while (true) {
            safeRead(file_name, std::ref(ready), std::ref(exist));
            if (exist && ready) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    /**
     * @brief add a key(actually file name) for reconciliation
     * @details This method may block if channel is always full
     * @param key file name
     * @param node_rank which node to load from
     */
    void AddKey(std::string key, int node_rank) {
        keyAndNode val{key, node_rank};
        val >> req_ch_;
    }
};
} // namespace communicators