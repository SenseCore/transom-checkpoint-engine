/**
 * @file storage.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "storage/storage.h"

#include "monitor/monitor.h"

using storage::Storage;
using api::DataEntry;
using api::Metadata;
using config::WorldState;
using monitor::MemoryMonitor;

/**
 * @brief release all memory
 */
Storage::~Storage() {
    for (auto &item : backup_dict_) {
        free(reinterpret_cast<void *>(item.second.address));
    }
}

bool Storage::Save(Metadata metadata, DataEntry entry) {
    LOG_INFO("inserted primary key {} address {} into data storage",
             metadata.file_name, (void *)entry.address);

    if (entry.address == 0) {
        LOG_FATAL("address is 0, illegal!");
        return false;
    }

    rw_mutex_.lock();
    if (metadata.node_rank == WorldState::Instance().NodeRank()) {
        dict_.insert_or_assign(metadata.file_name, entry);
        rw_mutex_.unlock();
        return true;
    }
    backup_dict_.insert_or_assign(metadata.file_name, entry);
    rw_mutex_.unlock();
    return true;
}

bool Storage::Load(Metadata &metadata, DataEntry &entry) {
    if (metadata.file_name.size() == 0) {
        LOG_FATAL("filename empty during load from storage");
    }
    rw_mutex_.lock_shared();
    if (metadata.node_rank == WorldState::Instance().NodeRank()) {
        auto iter = dict_.find(metadata.file_name);
        if (iter == dict_.end()) {
            LOG_WARN("filename {} not found in storage", metadata.file_name);
            rw_mutex_.unlock_shared();
            return false;
        }
        entry = iter->second;
        rw_mutex_.unlock_shared();
        return true;
    }

    auto iter = backup_dict_.find(metadata.file_name);
    if (iter == backup_dict_.end()) {
        LOG_WARN("filename {} not found in storage", metadata.file_name);
        rw_mutex_.unlock_shared();
        return false;
    }
    entry = iter->second;
    rw_mutex_.unlock_shared();
    return true;
}

bool Storage::Delete(api::Metadata &metadata) {
    LOG_INFO("deleting {} from storage", metadata.file_name);
    if (metadata.file_name.size() == 0) {
        LOG_FATAL("filename empty during deleting from storage");
    }
    rw_mutex_.lock();
    if (metadata.node_rank == WorldState::Instance().NodeRank()) {
        auto iter = dict_.find(metadata.file_name);
        if (iter != dict_.end()) {
            close(iter->second.memfd);
            MemoryMonitor::Instance().memfdFree(std::ref(metadata), std::ref(iter->second));
            dict_.erase(metadata.file_name);
            LOG_INFO("deleted {} from storage complete in dict_", metadata.file_name);
            rw_mutex_.unlock();
            return true;
        }
    }
    auto iter = backup_dict_.find(metadata.file_name);
    if (iter != backup_dict_.end()) {
        close(iter->second.memfd);
        MemoryMonitor::Instance().memfdFree(std::ref(metadata), std::ref(iter->second));
        backup_dict_.erase(metadata.file_name);
        LOG_INFO("deleted {} from storage complete in backup_dict_", metadata.file_name);
    }
    rw_mutex_.unlock();
    return true;
}

const std::map<std::string, api::DataEntry> &Storage::getDict() const {
    return dict_;
}

const std::map<std::string, api::DataEntry> &Storage::getBackupDict() const {
    return backup_dict_;
}
