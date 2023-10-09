/**
 * @file monitor.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "monitor/monitor.h"

#include "util/util.h"

using monitor::MemoryMonitor;
using monitor::MemoryStat;
using util::Util;

MemoryMonitor::MemoryMonitor() {
    buffer_ = static_cast<char *>(calloc(sizeof(char), 100));
    auto user_limit = util::Util::GetEnv(config::ENV_KEY_MEMORY_LIMIT_GB); // unit: GB
    user_limit_ = user_limit.size() == 0 ? 0 : std::stoull(user_limit) * 1024 * 1024 * 1024L;
    collectMetric(true);
}

MemoryMonitor::~MemoryMonitor() {
    free(buffer_);
}

void MemoryMonitor::Start() {
    std::thread([this]() {
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        while (true) {
            std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_seconds = current_time - start_time;
            if (elapsed_seconds.count() >= config::MEM_WATCH_PERIOD_SECONDS) {
                collectMetric(false);
                start_time = std::chrono::steady_clock::now();
                continue;
            }
        }
    }).detach();
    LOG_INFO("memory monitor started, collect metric every {} seconds", config::MEM_WATCH_PERIOD_SECONDS);
}

void MemoryMonitor::collectMetric(bool collectCapacity) {
    auto readAndSet = [this](std::string file_name, size_t &val) {
        std::ifstream infile;
        infile.open(std::string(config::MEM_CGROUP_DIR) + file_name);
        infile >> buffer_;
        val = std::stoul(buffer_);
        infile.close();
    };

    mu_.lock();

    /* read total mem info from cgroup config */

    readAndSet("memory.usage_in_bytes", std::ref(stat_.total_usage));
    readAndSet("memory.max_usage_in_bytes", std::ref(stat_.total_max_usage));

    if (collectCapacity) {
        readAndSet("memory.limit_in_bytes", std::ref(stat_.total_capacity));
        /* if cgroup limit is not set, cap may exceeds max available memory */
        auto pages = sysconf(_SC_PHYS_PAGES);
        auto page_size = sysconf(_SC_PAGE_SIZE);
        size_t max_physical_memory = pages * page_size;
        if (user_limit_ > 0) {
            stat_.total_capacity = std::min(stat_.total_capacity, user_limit_);
        } else {
            stat_.total_capacity = std::min(stat_.total_capacity, max_physical_memory);
        }
    }
    if (user_limit_ > 0) {
        stat_.total_idle = stat_.total_capacity - stat_.self_total_usage;
    } else {
        stat_.total_idle = stat_.total_capacity - stat_.total_usage;
    }

    LOG_INFO("memory monitor statistics: {}", stat_.String());

    mu_.unlock();
}

bool MemoryMonitor::enough(size_t toAlloc) {
    collectMetric(false);
    return stat_.total_idle > toAlloc;
}

int MemoryMonitor::TryMemfdMalloc(const api::Metadata &metadata, api::DataEntry &entry) {
    if (enough(metadata.size) == false) {
        LOG_WARN("memory insuficient, require {}, idle {}", metadata.size, stat_.total_idle);
        return api::STATUS_OOM;
    }
    stat_.self_total_usage += metadata.size;
    return Util::memfdCalloc(metadata, entry);
}

void MemoryMonitor::memfdFree(api::Metadata &metadata, api::DataEntry &entry) {
    LOG_TRACE("delete {} address {} size {} memfd {} in storage",
              metadata.file_name, reinterpret_cast<void *>(entry.address), metadata.size, entry.memfd);
    auto async_munmap = [](api::Metadata &metadata, api::DataEntry &entry) {
        LOG_TRACE("delete {} address {} size {} memfd {} in storage", metadata.file_name,
                  reinterpret_cast<void *>(entry.address), metadata.size, entry.memfd);
        if (munmap(reinterpret_cast<void *>(entry.address), metadata.size) != 0) {
            LOG_FATAL("munmap failed: {}", strerror(errno));
        }
    };
    // async_munmap(std::ref(metadata), std::ref(entry));
    std::thread(std::move(async_munmap), std::ref(metadata), std::ref(entry)).detach();
    stat_.self_total_usage -= metadata.size;
}

int MemoryMonitor::TryLoadFromFile(const api::Metadata &metadata, api::DataEntry &entry) {
    if (enough(metadata.size) == false) {
        LOG_WARN("memory insuficient, require {}, idle {}", metadata.size, stat_.total_idle);
        return api::STATUS_OOM;
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    stat_.self_total_usage += metadata.size;
    auto rc = Util::memfdCalloc(metadata, entry);

    auto fp = fopen(metadata.file_name.c_str(), "r");
    if (!fp) {
        LOG_ERROR("failed to open file {} error: {}, you may not have permission to create it",
                  metadata.file_name, strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    // if (file_len != metadata.Size) {
    // }
    auto read = fread(reinterpret_cast<void *>(entry.address), sizeof(char), metadata.size, fp);
    if (read != metadata.size) {
        LOG_ERROR("read from file {}, expect read {} bytes, return {}, failed: {}",
                  metadata.file_name, metadata.size, read, strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    auto ret = fclose(fp);
    if (ret != 0) {
        LOG_ERROR("failed to close file {}: {}", metadata.file_name, strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    auto time_val = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    LOG_INFO("ReadFromFS performance: read {} bytes use {} milliseconds", metadata.size, time_val.count());
    return rc;
}

MemoryStat MemoryMonitor::GetMemoryStat() {
    return stat_;
}
