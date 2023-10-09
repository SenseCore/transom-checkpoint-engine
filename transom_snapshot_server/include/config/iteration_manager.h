/**
 * @file iteration_manager.h
 * @author BaldStrong (BaldStrong@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-28
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

#include <memory>

#include "util/dequeue.h"
#include "util/util.h"

namespace config {
using util::Util;

/**
 * @brief record totalIteration, lastIteration, minIteration
 */
class IterationManager {
public:
    IterationManager() = default;
    IterationManager(const IterationManager &) = delete;
    IterationManager(IterationManager &&) = delete;
    IterationManager &operator=(const IterationManager &) = delete;
    IterationManager &operator=(IterationManager &&) = delete;

    static IterationManager &Instance() {
        static std::unique_ptr<IterationManager> instance_ptr(new IterationManager());
        return *instance_ptr;
    }

    /**
     * @brief return total iterations recorded in queue
     */
    size_t totalIteration() {
        return q_.size();
    }

    /**
     * @brief return the latest iteration recorded in queue
     */
    size_t lastIteration() {
        if (!q_.empty())
            return q_.back();
        return -1;
    }

    /**
     * @brief delete the first element in queue
     */
    void deleteOldestIteration() {
        if (!q_.empty())
            q_.try_pop();
    }

    /**
     * @brief return the oldest(first) element in queue
     * @return
     */
    size_t oldestIteration() {
        if (!q_.empty())
            return q_.front();
        return ULONG_MAX;
    }

    /**
     * @brief add an iteration indicator to queue
     * @param iter iteration indicator
     */
    void pushIteration(size_t iter) {
        LOG_DEBUG("pushIteration {} totalIteration {} minIteration {} lastIteration {}",
                  iter, totalIteration(), oldestIteration(), lastIteration());
        q_.push(iter);
    }

    /**
     * @brief return user-config about max iterations in cache
     */
    size_t maxIteration() {
        return max_iteration_;
    }

    /**
     * @brief check if given iteration exists in queue
     */
    bool isExist(const size_t &iter) {
        return q_.isExist(iter);
    }

private:
    util::SafeDeque<size_t> q_;
    const size_t max_iteration_ = std::stoul(util::Util::GetEnv(config::ENV_MAX_ITERATION_IN_CACHE,
                                                                config::DEFAULT_MAX_ITERATION_IN_CACHE));
};
} // namespace config