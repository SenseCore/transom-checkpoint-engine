/**
 * @file buffer.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief A general purpose for marshalling and unmarshalling data.
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "config/config.h"
#include "logger/logger.h"

namespace buffer {
/**
 * @brief Buffer is a general purpose for marshalling and unmarshalling data.
 * @details It's used for exchanging data beetwen two processes. It has the functionality to
 * be created starting from an input stream and to be sent over an output.
 * It contains a piece of memory storing data, and some indicators to help serialization.
 * It uses a light-weighted protocol for safely, simplicity and efficiency.
 */
class Buffer {
private:
    /**
     * @brief allocated memory size, aka capacity
     */
    size_t size_;

    /**
     * @brief memory buffer
     */
    char *buffer_;

    /**
     * @brief used in marshal(such as `Add()`), length of useful data in total. The unit is bytes
     */
    size_t length_;

    /**
     * @brief used in unmarshal(such as `Get()`), position to read binary data. The unit is bytes.
     */
    size_t offset_;

public:
    /**
     * @brief Construct a new Buffer object with initial size
     * @details If failed to allocate memory, program will exit.
     */
    Buffer() {
        size_ = config::BUFFER_BLOCK_SIZE;
        if (buffer_ = static_cast<char *>(calloc(size_, sizeof(char))); buffer_ == nullptr) {
            LOG_FATAL("Can't allocate memory.");
        }
        length_ = 0;
        offset_ = 0;
    }

    Buffer(const Buffer &orig) = delete;

    ~Buffer() {
        free(buffer_);
    }

    /**
     * @brief Marshal non-pointer data and write into memory. It's expected to succeed, otherwise let it crash.
     *
     * @tparam T data type, do not use pointer
     * @param item data, do not use pointer
     */
    template <class T>
    void Add(const T item) {
        auto t_size = sizeof(T);
        if ((length_ + t_size) >= size_) {
            // reallocate just enough space
            size_ = (((length_ + t_size) >> config::BUFFER_BLOCK_POW) << config::BUFFER_BLOCK_POW)
                    + config::BUFFER_BLOCK_SIZE;
            if (buffer_ = static_cast<char *>(realloc(buffer_, size_)); buffer_ == nullptr) {
                LOG_FATAL("Can't reallocate memory. size: {}.", size_);
            }
        }
        memmove(buffer_ + length_, static_cast<const void *>(&item), t_size);
        length_ += t_size;
    }

    /**
     * @brief Add data with type T, with n bytes, into buffer
     *
     * @tparam T data type, should be a pointer
     * @param item data
     * @param n data size, unit is bytes
     */
    template <class T>
    void Add(const T *item, size_t n = 1) {
        if (item == nullptr) {
            Add(config::BUFFER_NULL_VAL);
            return;
        }
        auto size = sizeof(T) * n;
        if ((length_ + size) >= size_) {
            size_ = (((length_ + size) >> config::BUFFER_BLOCK_POW) << config::BUFFER_BLOCK_POW)
                    + config::BUFFER_BLOCK_SIZE;
            if (buffer_ = static_cast<char *>(realloc(buffer_, size_)); buffer_ == nullptr) {
                LOG_FATAL("Can't reallocate memory. size: {}.", size_);
            }
        }
        memmove(buffer_ + length_, static_cast<const void *>(item), size);
        length_ += size;
    }

    /**
     * @brief Add human-readable string into buffer
     *
     * @param s const char array
     */
    void AddString(const char *s) {
        size_t size = strlen(s) + 1;
        Add(size);
        Add(s, size);
    }

    /**
     * @brief Add human-readable string into buffer
     *
     * @param s cpp std::string
     */
    void AddString(std::string s) {
        AddString(s.c_str());
    }

    /**
     * @brief Convert item into a uint64_t value and put into buffer.
     * @details Any data type is acceptable. If item is a pointer, will add its address;
     * otherwise reiterpret into 8-bytes memory, so be careful with it.
     *
     * @tparam T
     * @param item
     */
    template <class T>
    void AddMarshal(T item) {
        auto casted_item = reinterpret_cast<uint64_t>(item);
        Add(casted_item);
    }

    /**
     * @brief Get data from the offset of buffer and convert to type T
     *
     * @tparam T data type, should not be pointer
     * @return T copied value, safe to edit
     */
    template <class T>
    T Get() {
        if (offset_ + sizeof(T) > length_) {
            LOG_FATAL("Can't read any {}.", typeid(T).name());
        }
        T result = *(reinterpret_cast<T *>(buffer_ + offset_));
        offset_ += sizeof(T);
        return result;
    }

    /**
     * @brief Get data with size n from the offset of buffer and convert to type T *.
     * @details data is not deepcopied, just hold a reference and is immutable.
     * That means, you cannot keep the pointer out of the lifecycle of buffer object.
     *
     * @tparam T data type, should not be pointer
     * @param n data size
     * @return reference of data in buffer
     */
    template <class T>
    T *Get(size_t n) {
        auto data_size = sizeof(T) * n;
        if (offset_ + data_size > length_) {
            LOG_FATAL("Can't read {}", typeid(T).name());
        }
        T *result = reinterpret_cast<T *>(buffer_ + offset_);
        offset_ += data_size;
        return result;
    }

    /**
     * @brief Get human-readable string
     *
     * @return a deepcopied std::string object, it's safe to edit
     */
    std::string GetString() {
        size_t size = Get<size_t>();
        return std::string(Get<char>(size));
    }

    /**
     * @brief read a 8-bytes data from buffer, convert to target data type
     *
     * @tparam T data type
     * @return T converted data
     */
    template <class T>
    T GetFromMarshal() {
        return reinterpret_cast<T>(Get<uint64_t>());
    }

    /**
     * @brief Get the underlying allocated memory, only used to collaborate with system socket API
     */
    char *GetBuffer() {
        return buffer_;
    }

    /**
     * @brief Get the length of data, only used to collaborate with system socket API
     * @return size_t mLength
     */
    size_t GetBufferSize() const {
        return length_;
    }

    /**
     * @brief set the length of data, only used to collaborate with system socket API
     * @param length
     */
    void SetBufferSize(size_t length) {
        length_ = length;
    }

    /**
     * @brief realloc to target size on necessary, if current cap is larger, do nothing.
     * Previous data will be lost. Only used to collaborate with system socket API
     * @param target desired capacity
     */
    void Realloc(size_t target) {
        if (size_ < target) {
            if (buffer_ = static_cast<char *>(realloc(buffer_, target)); buffer_ == nullptr) {
                LOG_FATAL("Can't reallocate memory. size: {}.", target);
            }
            size_ = target;
        }
    }

    /**
     * @brief reset all indicators, however memory is not cleaned for re-use.
     */
    void Reset() {
        memset(buffer_, 0, size_);
        length_ = 0;
        offset_ = 0;
    }
};
} // namespace buffer
