/**
 * @file operator.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <operator/operator.h>

using operators::Operator;

void Operator::Run() {
    for (auto i = 0; i < nthreads_; i++) {
        std::thread([this]() {
            LOG_INFO("started reconciliation thread {}", util::Util::GetThreadID());
            this->run();
        }).detach();
    }
    LOG_INFO("all reconciliation threads started");
}

void Operator::run() {
    for (const auto &out : work_queue_) {
        LOG_TRACE("fetch key {}", out);
        if (!handler_(out)) {
            AddRateLimited(out);
        }
    }
}

void Operator::SetHandler(std::function<bool(std::string)> handler) {
    handler_ = std::move(handler);
}

void Operator::AddRateLimited(std::string key) {
    auto interval = rate_limiter_->aquire();
    LOG_TRACE("spend {} ms waiting on ratelimiter, add {} to queue", interval, key);
    key >> work_queue_;
}
