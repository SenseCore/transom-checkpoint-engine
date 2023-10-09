/**
 * @file channel.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief a thread-safe golang channel implementation
 * @version 0.1
 * @date 2023-07-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace util {

/**
 * @brief An iterator that block the current thread,
 * waiting to fetch elements from the channel.
 *
 * Used to implement channel range-based for loop.
 *
 * @tparam Channel Instance of channel.
 */
template <typename Channel>
class blocking_iterator {
public:
    using value_type = typename Channel::value_type;

    explicit blocking_iterator(Channel &ch) :
        ch_{ch} {
    }

    /**
     * Advances to next element in the channel.
     */
    blocking_iterator<Channel> operator++() const noexcept {
        return *this;
    }

    /**
     * Returns an element from the channel.
     */
    value_type operator*() const {
        value_type value;
        value << ch_;

        return value;
    }

    /**
     * Makes iteration continue until the channel is closed and empty.
     */
    bool operator!=(blocking_iterator<Channel>) const {
        std::unique_lock<std::mutex> lock{ch_.mtx_};
        ch_.waitBeforeRead(lock);

        return !(ch_.closed() && ch_.empty());
    }

private:
    Channel &ch_;
};

#if (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
#define NODISCARD [[nodiscard]]
#else
#define NODISCARD
#endif

/**
 * @brief Exception thrown if trying to write on closed channel.
 */
class closed_channel : public std::runtime_error {
public:
    explicit closed_channel(const char *msg) :
        std::runtime_error{msg} {
    }
};

/**
 * @brief Thread-safe container for sharing data between threads.
 *
 * Implements a blocking input iterator.
 *
 * @tparam T The type of the elements.
 */
template <typename T>
class channel {
public:
    using value_type = T;
    using iterator = blocking_iterator<channel<T>>;
    using size_type = std::size_t;

    /**
     * Creates an unbuffered channel.
     */
    constexpr channel() = default;

    /**
     * Creates a buffered channel.
     *
     * @param capacity Number of elements the channel can store before blocking.
     */
    explicit constexpr channel(size_type capacity);

    /**
     * Pushes an element into the channel.
     *
     * @throws closed_channel if channel is closed.
     */
    template <typename Type>
    friend void operator>>(Type &&, channel<typename std::decay<Type>::type> &);

    /**
     * Pops an element from the channel.
     *
     * @tparam Type The type of the elements
     */
    template <typename Type>
    friend void operator<<(Type &, channel<Type> &);

    /**
     * Returns the number of elements in the channel.
     */
    NODISCARD inline size_type constexpr size() const noexcept;

    /**
     * Returns true if there are no elements in channel.
     */
    NODISCARD inline bool constexpr empty() const noexcept;

    /**
     * Closes the channel.
     */
    inline void close() noexcept;

    /**
     * Returns true if the channel is closed.
     */
    NODISCARD inline bool closed() const noexcept;

    /**
     * Iterator
     */
    iterator begin() noexcept;
    iterator end() noexcept;

    /**
     * Channel cannot be copied or moved.
     */
    channel(const channel &) = delete;
    channel &operator=(const channel &) = delete;
    channel(channel &&) = delete;
    channel &operator=(channel &&) = delete;
    virtual ~channel() = default;

private:
    const size_type cap_{0};
    std::queue<T> queue_;
    std::atomic<std::size_t> size_{0};
    std::mutex mtx_;
    std::condition_variable cnd_;
    std::atomic<bool> is_closed_{false};

    inline void waitBeforeRead(std::unique_lock<std::mutex> &);
    inline void waitBeforeWrite(std::unique_lock<std::mutex> &);
    friend class blocking_iterator<channel>;
};

template <typename T>
constexpr channel<T>::channel(const size_type capacity) :
    cap_{capacity} {
}

template <typename T>
void operator>>(T &&in, channel<typename std::decay<T>::type> &ch) {
    if (ch.closed()) {
        throw closed_channel{"cannot write on closed channel"};
    }

    {
        std::unique_lock<std::mutex> lock{ch.mtx_};
        ch.waitBeforeWrite(lock);

        ch.queue_.push(std::forward<T>(in));
        ++ch.size_;
    }

    ch.cnd_.notify_one();
}

template <typename T>
void operator<<(T &out, channel<T> &ch) {
    if (ch.closed() && ch.empty()) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock{ch.mtx_};
        ch.waitBeforeRead(lock);

        if (!ch.empty()) {
            out = std::move(ch.queue_.front());
            ch.queue_.pop();
            --ch.size_;
        }
    }

    ch.cnd_.notify_one();
}

template <typename T>
constexpr typename channel<T>::size_type channel<T>::size() const noexcept {
    return size_;
}

template <typename T>
constexpr bool channel<T>::empty() const noexcept {
    return size_ == 0;
}

template <typename T>
void channel<T>::close() noexcept {
    is_closed_.store(true);
    cnd_.notify_all();
}

template <typename T>
bool channel<T>::closed() const noexcept {
    return is_closed_.load();
}

template <typename T>
blocking_iterator<channel<T>> channel<T>::begin() noexcept {
    return blocking_iterator<channel<T>>{*this};
}

template <typename T>
blocking_iterator<channel<T>> channel<T>::end() noexcept {
    return blocking_iterator<channel<T>>{*this};
}

template <typename T>
void channel<T>::waitBeforeRead(std::unique_lock<std::mutex> &lock) {
    cnd_.wait(lock, [this]() { return !empty() || closed(); });
}

template <typename T>
void channel<T>::waitBeforeWrite(std::unique_lock<std::mutex> &lock) {
    if (cap_ > 0 && size_ == cap_) {
        cnd_.wait(lock, [this]() { return size_ < cap_; });
    }
}
} // namespace util

/**
 * @brief Output iterator specialization
 */
template <typename T>
struct std::iterator_traits<util::blocking_iterator<T>> {
    using value_type = typename util::blocking_iterator<T>::value_type;
    using iterator_category = std::output_iterator_tag;
};
