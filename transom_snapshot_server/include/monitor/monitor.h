/**
 * @file monitor.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include "config/config.h"
#include "util/util.h"

namespace monitor {
constexpr double gb = 1073741824;

/**
 * @brief memory stat struct, containing capcity, idle, usage, etc
 */
struct MemoryStat {
    /**
     * @brief min(physical memory, user-set limit)
     */
    size_t total_capacity;

    /**
     * @brief capacity - usage
     */
    size_t total_idle;

    /**
     * @brief memory usage, directly read from cgroup
     */
    size_t total_usage;

    /**
     * @brief max memory usage, directly read from cgroup
     */
    size_t total_max_usage;

    /**
     * @brief memory usage by checkpoint cache
     */
    size_t self_total_usage;

    MemoryStat() {
        total_capacity = 0;
        total_idle = 0;
        total_usage = 0;
        total_max_usage = 0;
        self_total_usage = 0;
    }

    MemoryStat(const MemoryStat &stat) {
        total_capacity = stat.total_capacity;
        total_idle = stat.total_idle;
        total_usage = stat.total_usage;
        total_max_usage = stat.total_max_usage;
        self_total_usage = stat.self_total_usage;
    }

    /**
     * @brief convert size with unit bytes to GB
     * @param in size with unit bytes
     * @return double value
     */
    static double toGB(size_t in) {
        return static_cast<double>(in) / gb;
    }

    /**
     * @brief dump instance to human-readable string
     */
    std::string String() {
        std::stringstream ss;
        ss << "mem_total " << MemoryStat::toGB(total_capacity) << " GB, "
           << "mem_idle " << MemoryStat::toGB(total_idle) << " GB, "
           << "mem_self_usage " << MemoryStat::toGB(self_total_usage) << " GB, "
           << "mem_usage " << MemoryStat::toGB(total_usage) << " GB, "
           << "mem_max_usage " << MemoryStat::toGB(total_max_usage) << " GB";
        return ss.str();
    }
};

/**
 * @brief A memory manager that tries avoiding OOM and controls checkpoint cache memory usage
 * @details To avoid checkpoint cache exceeds memory limit or affects user logic, the memory consumption
 * should be controlled. Max rounds and max usage limit are provided, thus a memory monitor/manager is necessary.
 */
class MemoryMonitor {
private:
    /* internal buffer for reading from file */
    char *buffer_;

    /* user specified total memory for checkpoint */
    size_t user_limit_;

    MemoryStat stat_;
    std::mutex mu_;

    void collectMetric(bool);

    /**
     * @brief check if next malloc operator will fail
     *
     * @param to_alloc size to alloc
     * @return
     */
    bool enough(size_t to_alloc);

public:
    MemoryMonitor();
    ~MemoryMonitor();
    MemoryMonitor(const MemoryMonitor &) = delete;
    MemoryMonitor(MemoryMonitor &&) = delete;
    MemoryMonitor &operator=(const MemoryMonitor &) = delete;
    MemoryMonitor &operator=(MemoryMonitor &&) = delete;

    /**
     * @brief singleton instance
     * @return reference of singleton instance, cannot be copied or deleted
     */
    static MemoryMonitor &Instance() {
        static std::unique_ptr<MemoryMonitor> instance_ptr_(new MemoryMonitor());
        return *instance_ptr_;
    }

    /**
     * @brief watch memory usage periodically
     */
    void Start();

    /**
     * @brief return memory statistics, contain MemoryStat
     * @return MemoryStat
     */
    MemoryStat GetMemoryStat();

    /**
     * @brief try malloc memory with given size using memfd mechanism
     *
     * @param metadata metadata of checkpoint file
     * @param entry memfd and pid is recorded into entry
     * @return int status_code, non-zero value indicates failure
     */
    int TryMemfdMalloc(const api::Metadata &metadata, api::DataEntry &entry);

    /**
     * @brief free allocated memfd memory
     *
     * @param metadata metadata of checkpoint file
     * @param entry memfd and pid is recorded into entry
     */
    void memfdFree(api::Metadata &metadata, api::DataEntry &entry);

    /**
     * @brief load checkpoint from FileSystem
     *
     * @param metadata metadata of checkpoint file
     * @return int status_code, non-zero value indicates failure
     */
    int TryLoadFromFile(const api::Metadata &metadata, api::DataEntry &entry);
};
} // namespace monitor