/**
 * @file storage.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief stores cache-related locally data
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/api.h"
#include "config/world.h"
#include "logger/logger.h"
#include "storage/metadata.h"
#include "storage/persistence.h"

namespace storage {

/**
 * @brief stores cache-related local data, e.g. address, memfd, etc. These data are stored at each node's memory
 * @details Note that the nature of checkpoint ensures thread-safety, lock is unnecessary
 */
class Storage {
public:
    Storage() = default;
    ~Storage();
    Storage(const Storage &) = delete;
    Storage(Storage &&) = delete;
    Storage &operator=(const Storage &) = delete;
    Storage &operator=(Storage &&) = delete;

    static Storage &Instance() {
        static std::unique_ptr<Storage> instance_ptr_(new Storage());
        return *instance_ptr_;
    }

    /**
     * @brief save a data entry into local storage
     * @param metadata provided metadata
     * @param entry data entry to save
     * @return bool true: success
     */
    bool Save(api::Metadata metadata, api::DataEntry entry);

    /**
     * @brief load data entry from storage
     * @param metadata file name is required
     * @param entry where queried data stores
     * @return bool true: success
     */
    bool Load(api::Metadata &metadata, api::DataEntry &entry);

    /**
     * @brief delete record from storage, it it's a backup cache, free memory
     * @param metadata file name is required
     * @return bool true: success
     */
    bool Delete(api::Metadata &metadata);

    /**
     * @brief get _dict's constant reference
     */
    const std::map<std::string, api::DataEntry> &getDict() const;

    /**
     * @brief get _backup_dict's constant reference
     */
    const std::map<std::string, api::DataEntry> &getBackupDict() const;

private:
    std::map<std::string, api::DataEntry> dict_;
    std::map<std::string, api::DataEntry> backup_dict_;
    inline static std::shared_mutex rw_mutex_ = {};
};
} // namespace storage
