/**
 * @file persistence.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-11
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "storage/persistence.h"

#include <error.h>
#include <stdio.h>
#include <string.h>

#include "logger/logger.h"

using storage::Persistence;

bool Persistence::WriteToDisk(std::string &file_name, const void *data, size_t size) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto fp = fopen(file_name.c_str(), "w+"); /* create or write after truncate */
    if (!fp) {
        LOG_ERROR("failed to open file {} error: {}, you may not have permission to create it",
                  file_name, strerror(errno));
        return false;
    }

    auto written = fwrite(data, sizeof(char), size, fp);
    if (written != size) {
        LOG_ERROR("write to file {}, expect write {} bytes, return {}, failed: {}",
                  file_name, size, written, strerror(errno));
        return false;
    }

    auto ret = fclose(fp);
    if (ret != 0) {
        LOG_ERROR("failed to close file {}: {}", file_name, strerror(errno));
        return false;
    }

    auto timeval = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    LOG_INFO("WriteToDisk performance: write {} bytes use {} milliseconds", size, timeval.count());
    return true;
}
