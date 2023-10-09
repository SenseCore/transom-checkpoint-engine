/**
 * @file persistence.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-03
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <memory>
#include <string>

#include "logger/logger.h"

namespace storage {
/**
 * @brief persistent module
 */
class Persistence {
public:
    Persistence() = default;
    Persistence(const Persistence &) = delete;
    Persistence(Persistence &&) = delete;
    Persistence &operator=(const Persistence &) = delete;
    Persistence &operator=(Persistence &&) = delete;

    static Persistence &Instance() {
        static std::unique_ptr<Persistence> instance_ptr_(new Persistence());
        return *instance_ptr_;
    }

    /**
     * @brief dump to file storage or local file system
     * @param file_name file name to persistent
     * @param data binary data
     * @param size data size
     * @return bool true: success
     */
    bool WriteToDisk(std::string &file_name, const void *data, size_t size);

    /**
     * @brief dump to SSO
     * @return bool true: success
     */
    bool WriteToSSO() {
        LOG_FATAL("persistent to SSO not implemented");
        return true;
    }

private:
    /* user configuration, e.g. AK/SK */
};
} // namespace storage