/**
 * @file util.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-06-30
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "util/util.h"

#include <linux/memfd.h>

#include "monitor/monitor.h"

using util::Util;

std::string Util::GetEnv(std::string &key, const char *defaultVar) {
    return Util::GetEnv(key.c_str(), defaultVar);
}

std::string Util::GetEnv(const char *key, const char *defaultVar) {
    auto val = getenv(key);
    if (!val) {
        return defaultVar ? std::string(defaultVar) : std::string("");
    }
    return std::string(val);
}

int Util::GetThreadID() {
    return syscall(__NR_gettid);
}

std::vector<std::string> Util::Split(std::string in, const char delim) {
    auto len = in.length();
    auto begin = 0, end = 0;
    std::vector<std::string> res;
    while (begin < len) {
        // in case there're multiple delims, search for the first char
        if (in[begin] == delim || in[begin] == '\t') {
            begin++;
            continue;
        }
        // construct a string
        std::string word;
        word.push_back(in[begin]);
        // find the end of word
        end = begin + 1;
        while (end < len && in[end] != delim && in[end] != '\t') {
            word.push_back(in[end]);
            end++;
        }
        res.push_back(word);
        begin = end + 1;
    }
    return res;
}

std::string Util::Join(std::vector<std::string> &vec, std::string delim) {
    std::string res;
    for (auto i = 0; i < vec.size(); i++) {
        res += vec[i];
        if (i != vec.size() - 1) {
            res += delim;
        }
    }
    return res;
}

size_t Util::Find(const char *str, char t) {
    auto size = strlen(str);
    for (size_t i = 0; i < size; i++) {
        if (str[i] == t) {
            return i;
        }
    }
    return -1;
}

int Util::ResolveHostname(std::string &hostname, std::string &eth_address) {
    /* if hostname already is an IP, skip */
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, hostname.c_str(), &(sa.sin_addr));
    if (result == 1) {
        eth_address = hostname;
        return 0;
    }

    /* resolve hostname */
    auto ret = gethostbyname(hostname.c_str());
    if (!ret) {
        return -1;
    }
    for (int i = 0; ret->h_addr_list[i]; i++) {
        // get the first ip address, it's ethernet address
        eth_address = std::string(inet_ntoa(*(struct in_addr *)ret->h_addr_list[i]));
        return 0;
    }
    return -1;
}

int Util::memfdCalloc(const api::Metadata &metadata, api::DataEntry &entry) {
    if (metadata.size <= 0) {
        LOG_ERROR("request to calloc non-positive size {}", metadata.size);
        return api::STATUS_UNKNOWN_ERROR;
    }
    /* create shared memory */
    // const int memfd = memfd_create(metadata.FileName.c_str(), MFD_HUGETLB);
    const int memfd = memfd_create(metadata.file_name.c_str(), 0);
    if (memfd < 0) {
        LOG_ERROR("memfd_create error: {}", strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    if (ftruncate(memfd, metadata.size) == -1) {
        LOG_ERROR("ftruncate error: {}", strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    // in order to reuse memfd, can't add seals
    // if (fcntl(memfd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    //     LOG_FATAL("fcntl error: {}", strerror(errno));
    //     return false;
    // }
    const int pid = getpid();
    // std::string str = "ls -l /proc/" + std::to_string(pid) + "/fd/ | grep memfd";
    // system(str.c_str());

    size_t localAddr = reinterpret_cast<size_t>(
        mmap(NULL, metadata.size, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0));
    if (localAddr == reinterpret_cast<size_t>(MAP_FAILED)) {
        LOG_ERROR("mmap error: {}", strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    entry.address = localAddr;
    entry.pid = pid;
    entry.memfd = memfd;
    LOG_DEBUG("memfd localAddr:{} size:{} pid:{} memfd:{}",
              reinterpret_cast<void *>(localAddr), metadata.size, pid, memfd);
    return api::STATUS_SUCCESS;
}

int Util::memfdFtruncate(api::Metadata &metadata, api::DataEntry &entry) {
    if (metadata.size <= 0) {
        LOG_ERROR("request to calloc non-positive size {}", metadata.size);
        return api::STATUS_UNKNOWN_ERROR;
    }

    struct stat sb;
    fstat(entry.memfd, &sb);
    size_t old_size = sb.st_size;
    if (old_size == metadata.size) {
        return api::STATUS_SUCCESS;
    }
    if (ftruncate(entry.memfd, metadata.size) == -1) {
        LOG_ERROR("ftruncate error: {}", strerror(errno));
        return api::STATUS_UNKNOWN_ERROR;
    }
    LOG_DEBUG("ftruncate size: {} → {}", old_size, metadata.size);

    return api::STATUS_SUCCESS;
}

std::string Util::base64_encode(std::string const &data) {
    int counter = 0;
    uint32_t bit_stream = 0;
    std::string encoded;
    int offset = 0;
    for (unsigned char c : data) {
        auto num_val = static_cast<unsigned int>(c);
        offset = 16 - counter % 3 * 8;
        bit_stream += num_val << offset;
        if (offset == 16) {
            encoded += util::base64_chars[bit_stream >> 18 & 0x3f];
        }
        if (offset == 8) {
            encoded += util::base64_chars[bit_stream >> 12 & 0x3f];
        }
        if (offset == 0 && counter != 3) {
            encoded += util::base64_chars[bit_stream >> 6 & 0x3f];
            encoded += util::base64_chars[bit_stream & 0x3f];
            bit_stream = 0;
        }
        counter++;
    }
    if (offset == 16) {
        encoded += util::base64_chars[bit_stream >> 12 & 0x3f];
        // encoded += "=="; // "=" is not convenient to handle in HTTP Params, give up
    }
    if (offset == 8) {
        encoded += util::base64_chars[bit_stream >> 6 & 0x3f];
        // encoded += '=';
    }
    return encoded;
}

std::string Util::base64_decode(std::string const &data) {
    int counter = 0;
    uint32_t bit_stream = 0;
    std::string decoded;
    int offset = 0;
    for (unsigned char c : data) {
        auto num_val = Util::Find(util::base64_chars, c);
        if (num_val != std::string::npos) {
            offset = 18 - counter % 4 * 6;
            bit_stream += num_val << offset;
            if (offset == 12) {
                decoded += static_cast<char>(bit_stream >> 16 & 0xff);
            }
            if (offset == 6) {
                decoded += static_cast<char>(bit_stream >> 8 & 0xff);
            }
            if (offset == 0 && counter != 4) {
                decoded += static_cast<char>(bit_stream & 0xff);
                bit_stream = 0;
            }
        } else if (c != '=') {
            return std::string();
        }
        counter++;
    }
    return decoded;
}
