/**
 * @file dequeue.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief a thread-safe dequeue implementation
 * @version 0.1
 * @date 2023-07-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

namespace util {
/**
 * @brief thread-safe dequeue
 *
 * @tparam T
 */
template <typename T>
class SafeDeque {
private:
    mutable std::mutex mut;
    std::deque<T> queue;
    std::condition_variable data_cond;

public:
    SafeDeque() {
    }

    SafeDeque(SafeDeque const &other) {
        // std::lock_guard<std::mutex> lk(other.mut);
        // queue = other.queue;
        std::lock(other.mut, mut);
        std::lock_guard<std::mutex> lk1(other.mut, std::adopt_lock);
        std::lock_guard<std::mutex> lk2(mut, std::adopt_lock);
        using std::swap;
        swap(queue, other.queue);
    }

    void push(T new_value) {
        std::lock_guard<std::mutex> lk(mut);
        queue.push_back(new_value);
        data_cond.notify_one();
    }

    bool try_pop() {
        std::lock_guard<std::mutex> lk(mut);
        if (queue.empty())
            return false;
        queue.pop_front();
        return true;
    }

    bool empty() const {
        std::unique_lock<std::mutex> lk(mut, std::try_to_lock);
        if (!lk.owns_lock()) {
            return false;
        }
        return queue.empty();
    }

    size_t size() {
        std::unique_lock<std::mutex> lk(mut, std::try_to_lock);
        if (!lk.owns_lock()) {
            return 0;
        }
        return queue.size();
    }

    T &front() {
        std::lock_guard<std::mutex> lk(mut);
        return queue.front();
    }

    T &back() {
        std::lock_guard<std::mutex> lk(mut);
        return queue.back();
    }

    bool isExist(const T &iter) {
        std::lock_guard<std::mutex> lk(mut);
        for (const auto &v : queue) {
            if (v == iter) {
                return true;
            }
        }
        return false;
    }
};
} // namespace util
