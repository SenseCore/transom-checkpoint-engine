/**
 * @file http_service_impl.h
 * @author BaldStrong (BaldStrong@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <fcntl.h>
#include <linux/memfd.h>

#include <memory>
#include <string>
#include <vector>

#include "brpc/restful.h"
#include "brpc/server.h"

#include "communicator/http/http.pb.h"
#include "communicator/http/remote_file_loader.h"
#include "config/iteration_manager.h"
#include "config/world.h"
#include "logger/logger.h"
#include "monitor/monitor.h"
#include "operator/operator.h"
#include "storage/storage.h"
#include "util/channel.h"
#include "util/util.h"

#define return_resp(status, message, state)     \
    do {                                        \
        make_resp(res, status, message, state); \
        return;                                 \
    } while (0)

namespace communicators {
using util::Util;
using storage::Storage;
using config::WorldState;
using config::IterationManager;
using api::CheckpointState;
using monitor::MemoryMonitor;

/**
 * @brief implementation of http server generated from http.proto
 */
class HttpServiceImpl : public HttpService {
private:
    /**
     * @brief operator for reconciliation
     */
    std::shared_ptr<operators::Operator> controller_;

    /**
     * @brief ready indicator, set to true after bootstrap finishes
     */
    std::shared_ptr<std::atomic<bool>> ready_;

    /**
     * @brief read-write lock to make sure that there is only one request to delete the old Checkpoint
     */
    inline static std::shared_mutex rw_mutex_ = {};

public:
    HttpServiceImpl() = default;
    HttpServiceImpl(std::shared_ptr<operators::Operator> controller,
                    std::shared_ptr<std::atomic<bool>> ready) {
        controller_ = controller;
        ready_ = ready;
    }

    void getMetadata(google::protobuf::RpcController *cntl_base,
                     const HttpRequest *req, HttpResponse *res,
                     google::protobuf::Closure *done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        // Fill response.
        cntl->http_response().set_content_type("application/json");

        std::string file_name = req->filename();
        // LOG_DEBUG("FileName {}", raw_filename);
        // check whether bootstrap is completed
        if (wait_ready() == false) {
            std::string message = "bootstrap timed out in "
                                  + std::to_string(config::CHECK_BOOTSTRAP_RETRY_INTERVAL_SECONDS)
                                  + "s and did not complete. Please check the server";
            return_resp("ERROR", message, -1);
        }
        /* try get metadata */
        api::Metadata metadata(WorldState::Instance().JobName(), file_name);
        auto meta_client = storage::MetadataClientFactory::GetClient();
        int rc = -1;

        /* if metadata not found, read from persistent storage, which is slow */
        rc = meta_client->Load(std::ref(metadata));
        if (!api::IsSuccess(rc)) {
            return_resp("ERROR", "get metadata failed from database, Please check if the file exists", -1);
        }

        /* unless file is obsolescent or broken or pending, it should be in shm or memory already */
        if (metadata.state == api::CheckpointState::BROKEN || metadata.state == api::CheckpointState::OBSOLESCENT
            || metadata.state == api::CheckpointState::PENDING) {
            LOG_WARN("state of file {} is {}, which may indicate an internal error",
                     metadata.file_name, api::CheckpointStateString(metadata.state));
            return_resp("ERROR", "checkpointstate is BROKEN or OBSOLESCENT or PENDING", -1);
        }

        /* if it's backed-up at local node, directly from the backup file torch.load */
        if (((metadata.node_rank + 1) % WorldState::Instance().WorldSize()) == WorldState::Instance().NodeRank()) {
            goto found_metadata;
        }

        /* not found locally in shm, add to queue to avoid data race. e.g 8 ranks save to shm at the same time */
        if (metadata.node_rank != WorldState::Instance().NodeRank()) {
            remoteFileLoader::Instance().AddKey(file_name, metadata.node_rank);
            remoteFileLoader::Instance().WaitUntilFileReady(file_name);
        }

    found_metadata:
        api::DataEntry entry;
        if (!storage::Storage::Instance().Load(std::ref(metadata), std::ref(entry))) {
            LOG_ERROR("load storage failed");
            return_resp("ERROR", "in-memory checkpoint does not exist in local or backup", metadata.state);
        }
        LOG_DEBUG("entry: {}", entry.String());
        res->set_pid(entry.pid);
        res->set_memfd(entry.memfd);
        return_resp("OK", "Metadata was successfully got", metadata.state);
    }

    void createMetadata(google::protobuf::RpcController *cntl_base,
                        const HttpRequest *req, HttpResponse *res,
                        google::protobuf::Closure *done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        // Fill response.
        cntl->http_response().set_content_type("application/json");
        // check whether bootstrap is completed
        if (wait_ready() == false) {
            std::string message = "bootstrap timed out in "
                                  + std::to_string(config::CHECK_BOOTSTRAP_RETRY_INTERVAL_SECONDS)
                                  + "s and did not complete. Please check the server";
            return_resp("ERROR", message, -1);
        }
        std::string file_name = req->filename();
        int state = req->checkpointstate();
        std::string iteration = req->iteration();
        size_t size = req->size();
        api::Metadata metadata(WorldState::Instance().JobName(), file_name,
                               WorldState::Instance().NodeRank(), iteration,
                               static_cast<CheckpointState>(state), size);
        auto meta_client = storage::MetadataClientFactory::GetClient();

        // check whether max_iteration has been reached
        std::string delete_min_iteration_msg = "";
        if (iteration != "unknown") {
            const auto iter = std::stoul(iteration);
            rw_mutex_.lock();
            if (!IterationManager::Instance().isExist(iter)) {
                if (IterationManager::Instance().totalIteration() >= IterationManager::Instance().maxIteration()) {
                    delete_min_iteration_msg = " exceed max iteration: "
                                               + std::to_string(IterationManager::Instance().maxIteration())
                                               + ", delete only the oldest iteration: "
                                               + std::to_string(IterationManager::Instance().oldestIteration())
                                               + " in-memory, we do not delete persistent checkpoints!";
                    LOG_WARN("Exceed max iteration: {} totalIteration: {}",
                             IterationManager::Instance().maxIteration(), IterationManager::Instance().totalIteration());
                    if (!deleteOldestIteration(std::to_string(IterationManager::Instance().oldestIteration()))) {
                        rw_mutex_.unlock();
                        return_resp("ERROR", "deleteMinIteration failed", -1);
                    }
                }
                IterationManager::Instance().pushIteration(iter);
            }
            rw_mutex_.unlock();
        }

        api::DataEntry entry;
        if (!storage::Storage::Instance().Load(std::ref(metadata), std::ref(entry))) {
            LOG_DEBUG("{} doesn't exists, memfdCalloc", metadata.file_name);
            auto rc = MemoryMonitor::Instance().TryMemfdMalloc(std::ref(metadata), std::ref(entry));
            if (api::IsOOM(rc)) {
                return_resp("ERROR", "memfdCalloc failed: out of memory", state);
            }
            if (!api::IsSuccess(rc)) {
                return_resp("ERROR", "memfdCalloc failed: unkonwn error", state);
            }
            if (!Storage::Instance().Save(metadata, entry)) {
                LOG_ERROR("failed to add <{}> into storage", metadata.String());
                return_resp("ERROR", "memfdCalloc failed: Save storage failed", state);
            }
        } else {
            LOG_DEBUG("{} already exists, ftruncate to reuse memfd", metadata.file_name);
            auto rc = Util::memfdFtruncate(std::ref(metadata), std::ref(entry));
            if (!api::IsSuccess(rc)) {
                return_resp("ERROR", " memfdFtruncate failed", state);
            }
        }
        res->set_pid(entry.pid);
        res->set_memfd(entry.memfd);
        LOG_DEBUG("entry: {}", entry.String());

        // Successfully allocated memory, create metadata
        auto rc = meta_client->Save(metadata);
        if (!api::IsSuccess(rc)) {
            return_resp("ERROR", "save Metadata failed", state);
        }
        return_resp("OK", "Metadata was successfully created." + delete_min_iteration_msg, state);
    }

    void updateMetadata(google::protobuf::RpcController *cntl_base,
                        const HttpRequest *req, HttpResponse *res,
                        google::protobuf::Closure *done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        // Fill response.
        cntl->http_response().set_content_type("application/json");

        std::string file_name = req->filename();
        int state = req->checkpointstate();
        /* try update metadata state */
        auto meta_client = storage::MetadataClientFactory::GetClient();
        auto rc = meta_client->UpdateState(file_name, static_cast<CheckpointState>(state));
        if (!api::IsSuccess(rc)) {
            return_resp("ERROR", "update metadata state failed", state - 1);
        }
        controller_->AddRateLimited(file_name);
        return_resp("OK", "Metadata was successfully updated", state);
    }

    void getAllMetadata(google::protobuf::RpcController *cntl_base,
                        const HttpRequest *, CLIResponse *res,
                        google::protobuf::Closure *done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        // Fill response.
        cntl->http_response().set_content_type("application/proto");

        api::BatchLoadFilter filter(-1);
        std::vector<api::Metadata> vec;
        auto meta_client = storage::MetadataClientFactory::GetClient();
        auto ret = meta_client->BatchLoad(filter, vec);
        if (ret == api::STATUS_UNKNOWN_ERROR) {
            LOG_ERROR("get AllMetadata failed");
            res->set_status("ERROR");
            return;
        }
        for (const auto &meta : vec) {
            auto brpc_meta = res->add_metadata();
            brpc_meta->set_filename(meta.file_name);
            brpc_meta->set_noderank(meta.node_rank);
            brpc_meta->set_iteration(meta.iteration);
            brpc_meta->set_checkpointstate(meta.state);
            brpc_meta->set_size(meta.size);
        }
        res->set_status("OK");
    }

    void getAllStorage(google::protobuf::RpcController *cntl_base,
                       const HttpRequest *, CLIResponse *res,
                       google::protobuf::Closure *done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        // Fill response.
        cntl->http_response().set_content_type("application/proto");

        auto dict = Storage::Instance().getDict();
        auto backup_dict = Storage::Instance().getBackupDict();
        for (const auto &[file_name, entry] : dict) {
            auto tmp_dict = res->add_cli_dict();
            tmp_dict->set_filename(file_name);
            tmp_dict->set_address(entry.address);
            tmp_dict->set_pid(entry.pid);
            tmp_dict->set_memfd(entry.memfd);
        }
        for (const auto &[file_name, entry] : backup_dict) {
            auto tmp_dict = res->add_cli_backup_dict();
            tmp_dict->set_filename(file_name);
            tmp_dict->set_address(entry.address);
            tmp_dict->set_pid(entry.pid);
            tmp_dict->set_memfd(entry.memfd);
        }
        res->set_status("OK");
        LOG_DEBUG("dict size {} backup_dict size {}", res->cli_dict_size(), res->cli_backup_dict_size());
    }

    void make_resp(HttpResponse *res, std::string status, std::string message, const int32_t &state) {
        if (status == "ERROR") {
            LOG_ERROR(message);
        } else {
            LOG_INFO(message);
        }
        res->set_status(status);
        res->set_checkpointstate(state);
        res->set_message("server: " + message);
    }

    bool wait_ready() {
        int wait_time = config::BOOTSTRAP_MIN_RETRY_INTERVAL_SECONDS;
        while (!ready_->load()) {
            LOG_INFO("waiting for bootstrap to complete, sleep {}s", wait_time);
            std::this_thread::sleep_for(std::chrono::seconds(wait_time));
            if (wait_time < config::CHECK_BOOTSTRAP_RETRY_INTERVAL_SECONDS) {
                wait_time *= 2;
            } else {
                return false;
            }
        }
        return true;
    }

    // Delete the oldest iteration of this node
    bool deleteOldestIteration(std::string oldest_itreration) {
        api::BatchLoadFilter filter(WorldState::Instance().NodeRank(), oldest_itreration);
        std::vector<api::Metadata> vec;
        auto meta_client = storage::MetadataClientFactory::GetClient();
        auto rc = meta_client->BatchLoad(filter, vec);
        if (!api::IsSuccess(rc)) {
            LOG_ERROR("get oldestItreration metadata failed");
            return false;
        }
        if (vec.size() == 0) {
            LOG_ERROR("get 0 oldestItreration Metadata");
            return false;
        }
        for (auto &meta : vec) {
            // wait for the previous state to complete
            int wait_time = config::BOOTSTRAP_MIN_RETRY_INTERVAL_SECONDS;
            while ((meta.state == api::CheckpointState::CACHED && WorldState::Instance().WorldSize() > 1)
                   || (meta.state == api::CheckpointState::BACKED_UP
                       && util::Util::GetEnv(config::IS_PERSISTENT, "on") == "on")) {
                LOG_INFO("wait for the previous state of {} to complete, wait {}s...", meta.file_name, wait_time);
                std::this_thread::sleep_for(std::chrono::seconds(wait_time));
                wait_time *= 2;
                auto rc = meta_client->Load(meta);
                if (!api::IsSuccess(rc)) {
                    LOG_ERROR("get Metadata failed");
                    return false;
                }
            }
            // mark as OBSOLESCENT state
            auto rc = meta_client->UpdateState(meta.file_name, api::CheckpointState::OBSOLESCENT);
            if (!api::IsSuccess(rc)) {
                LOG_ERROR("update metadata state failed");
                return false;
            }
            controller_->AddRateLimited(meta.file_name);
            // Waiting for deletion to complete
            api::DataEntry entry;
            while (storage::Storage::Instance().Load(std::ref(meta), std::ref(entry))) {
                LOG_INFO("Waiting for deletion to complete {}, wait 0.1s...", meta.file_name);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        IterationManager::Instance().deleteOldestIteration();
        LOG_DEBUG("deleted oldestItreration:{} ckpt nums:{}", oldest_itreration, vec.size());
        return true;
    }
};
} // namespace communicators
