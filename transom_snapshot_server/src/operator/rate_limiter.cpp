/**
 * @file rate_limiter.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "operator/rate_limiter.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "logger/logger.h"

using operators::RateLimiter;

RateLimiter::RateLimiter() :
    interval_(0), max_permits_(0), stored_permits_(0), next_free_(0) {
}

double RateLimiter::aquire() {
    return aquire(1);
}

double RateLimiter::aquire(int permits) {
    if (permits <= 0) {
        LOG_FATAL("RateLimiter: Must request positive amount of permits");
    }

    auto wait_time = claim_next(permits);
    std::this_thread::sleep_for(wait_time);

    return static_cast<double>(wait_time.count()) / 1000.0;
}

bool RateLimiter::try_aquire(int permits) {
    return try_aquire(permits, 0);
}

bool RateLimiter::try_aquire(int permits, int timeout) {
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    // Check to see if the next free aquire time is within the
    // specified timeout. If it's not, return false and DO NOT BLOCK,
    // otherwise, calculate time needed to claim, and block
    if (next_free_ > now + timeout * 1000)
        return false;

    aquire(permits);
    return true;
}

void RateLimiter::sync(__uint128_t now) {
    // If we're passed the next_free, then recalculate
    // stored permits, and update _next_free
    if (now > next_free_) {
        stored_permits_ = std::min(max_permits_, stored_permits_ + (now - next_free_) / interval_);
        next_free_ = now;
    }
}

std::chrono::microseconds RateLimiter::claim_next(double permits) {
    std::lock_guard<std::mutex> lock(mut_);

    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    // Make sure we're synced
    sync(now);

    // Since we synced before hand, this will always be >= 0.
    auto wait = next_free_ - now;

    // Determine how many stored and freh permits to consume
    double stored = std::min(permits, stored_permits_);
    double fresh = permits - stored;

    // In the general RateLimiter, stored permits have no wait time,
    // and thus we only have to wait for however many fresh permits we consume
    auto next_free = static_cast<uint64_t>(fresh * interval_);

    next_free_ += next_free;
    stored_permits_ -= stored;

    return std::chrono::microseconds(wait);
}

double RateLimiter::get_rate() const {
    return 1000000.0 / interval_;
}

void RateLimiter::set_rate(double rate) {
    if (rate <= 0.0) {
        throw std::runtime_error("RateLimiter: Rate must be greater than 0");
    }

    std::lock_guard<std::mutex> lock(mut_);
    interval_ = 1000000.0 / rate;
}
