/**
 * @file rate_limiter.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <mutex>

namespace operators {
/**
 * @brief ratelimiter interface
 */
class RateLimiterInterface {
public:
    virtual ~RateLimiterInterface() {
    }

    /**
     * @brief blocking method, acquire one permit
     * @return wait time, unit is seconds
     */
    virtual double aquire() = 0;

    /**
     * @brief blocking method, acquire permits permit
     *
     * @param permits number of permits to acquire
     * @return wait time, unit is seconds
     */
    virtual double aquire(int permits) = 0;

    /**
     * @brief blocking method with timeout, acquire one permit until timeout
     * @param timeout unit is seconds
     * @return bool true: success, false: failed
     */
    virtual bool try_aquire(int timeout) = 0;

    /**
     * @brief blocking method with timeout, acquire permits permit until timeout
     *
     * @param permits number of permits
     * @param timeout unit is seconds
     * @return bool true: success, false: failed
     */
    virtual bool try_aquire(int permits, int timeout) = 0;

    /**
     * @brief get permit generation rate
     */
    virtual double get_rate() const = 0;

    /**
     * @brief set permit generation rate
     */
    virtual void set_rate(double rate) = 0;
};

/**
 * @brief an implementation of rate limiter interface
 */
class RateLimiter : public RateLimiterInterface {
public:
    RateLimiter();
    double aquire() override;
    double aquire(int permits) override;

    bool try_aquire(int timeouts) override;
    bool try_aquire(int permits, int timeout) override;

    double get_rate() const;
    void set_rate(double rate) override;

private:
    void sync(__uint128_t now);
    std::chrono::microseconds claim_next(double permits);

private:
    double interval_;
    double max_permits_;
    double stored_permits_;

    __uint128_t next_free_;

    std::mutex mut_;
};
} // namespace operators