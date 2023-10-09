/**
 * @file util.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief various util function, most are string related
 * @version 0.1
 * @date 2023-06-30
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "api/api.h"
#include "logger/logger.h"

namespace util {
constexpr auto base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789+/";

class Util {
public:
    /**
     * @brief get env var. If key is not found, return default value
     * @param key env key
     * @param defaultVar default value
     * @return string the ultimate result
     */
    static std::string GetEnv(std::string &key, const char *defaultVar = nullptr);
    static std::string GetEnv(const char *key, const char *defaultVar = nullptr);

    /**
     * @brief split string into a vector of substring by delim
     *
     * @param str string to split
     * @param delim character, default to ' '
     * @return vector store splitted strings
     */
    static std::vector<std::string> Split(std::string str, const char delim = ' ');

    /**
     * @brief join strings to a long string with given delim. e.g. 'A', 'B' is joined by ' ' to "A B"
     * @param vec strings to join
     * @param delim character
     * @return string joined string
     */
    static std::string Join(std::vector<std::string> &vec, std::string delim);

    /**
     * @brief find target char in given char array
     * @param str char array to find
     * @param t target character
     * @return size_t return index if found, -1 otherwise
     */
    static size_t Find(const char *str, char t);

    /**
     * @brief get current thread id
     * @return integer thread id
     */
    static pid_t GetThreadID();

    /**
     * @brief resolve hostname and set IP address to input `eth_address`, this function is thread-safe
     * @param hostname resolvable domain name
     * @param eth_address where resolved IP address stores
     * @return int status code, non-zero means failure
     */
    static int ResolveHostname(std::string &hostname, std::string &eth_address);

    /**
     * @brief use memfd_create to malloc memory
     *
     * @param metadata checkpoint file metadata
     * @param entry data entry to store memfd
     * @return int status code, non-zero means failure
     */
    static int memfdCalloc(const api::Metadata &metadata, api::DataEntry &entry);

    /**
     * @brief when Filename is in _dict, truncate memory
     *
     * @param metadata checkpoint file metadata
     * @param entry data entry to store memfd
     * @return int status code, non-zero means failure
     */
    static int memfdFtruncate(api::Metadata &metadata, api::DataEntry &entry);

    /**
     * @brief base64 encode
     */
    static std::string base64_encode(std::string const &data);

    /**
     * @brief base64 decode
     */
    static std::string base64_decode(std::string const &data);
};
} // namespace util
