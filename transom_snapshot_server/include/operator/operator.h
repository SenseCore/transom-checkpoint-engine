/**
 * @file operator.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-03
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "config/config.h"
#include "config/world.h"
#include "logger/logger.h"
#include "operator/rate_limiter.h"
#include "util/channel.h"
#include "util/util.h"

namespace operators {

/**
 * @brief kubernetes-style operator, build a bridge between backend and coordinator.
 * backend server adds a key into workqueue after saving checkpoint, then coordinator fetch from workqueue,
 * backup to other node.
 */
class Operator {
private:
    std::shared_ptr<RateLimiter> rate_limiter_;
    util::channel<std::string> work_queue_ = util::channel<std::string>(config::OPERATOR_WORKQUEUE_BUFFER);
    int nthreads_ = config::OPERATOR_N_THREADS;
    std::vector<std::thread> threads_;
    std::function<bool(std::string)> handler_;

    /**
     * @brief blocking: forever waiting for channel items and reconcile
     */
    void run();

public:
    Operator() {
        rate_limiter_ = std::make_shared<RateLimiter>();
        rate_limiter_->set_rate(config::OPERATOR_RATELIMITER_RATE);
        LOG_DEBUG("workqueue ratelimit: {} permit per second", config::OPERATOR_RATELIMITER_RATE);
    }

    /**
     * @brief starts the operator
     */
    void Run();

    /**
     * @brief add key into workqueue
     *
     * @param key file name
     */
    void AddRateLimited(std::string key);

    /**
     * @brief register a handler for reconciliation
     * @param handler function
     */
    void SetHandler(std::function<bool(std::string)> handler);
};
} // namespace operators