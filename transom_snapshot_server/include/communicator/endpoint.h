/**
 * @file endpoint.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief defines endpoint model
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <filesystem>
#include <string>

#include "communicator/endpoint.h"
#include "logger/logger.h"
#include "util/util.h"

namespace communicators {
using util::Util;
namespace fs = std::filesystem;

/**
 * Endpoint class is a model to represent the endpoint property for inter/intra node communication.
 */
class Endpoint {
public:
    explicit Endpoint(std::string addr = "0.0.0.0", uint16_t port = 18080) {
        addr_ = addr;
        port_ = port;
    }

    /**
     * @brief This method return an object description
     *
     * @return string that represents the concatenation between class member
     */
    inline const std::string to_string() const {
        return addr_ + ":" + std::to_string(port_);
    }

    /**
     * @brief This method is an overload of == operator.
     *
     * @param endpoint object to compare
     * @return true if two object are similar, false otherwise
     */
    inline bool operator==(const Endpoint &endpoint) const {
        return to_string() == endpoint.to_string();
    }

    /**
     * @brief set address
     */
    void setAddr(std::string &addr) {
        addr_ = addr;
    }

    /**
     * @brief set port
     */
    void setPort(uint16_t port) {
        port_ = port;
    }

    /**
     * @brief get address
     */
    std::string &addr() {
        return addr_;
    }

    /**
     * @brief get port
     */
    uint16_t port() {
        return port_;
    }

private:
    /**
     * @brief IP address or domain name
     */
    std::string addr_;

    /**
     * @brief port
     */
    uint16_t port_;
};

/**
 * @brief a factory to generate different endpoints according to the communicator type
 * @details two communicator types are optional, RDMA and http.
 */
class EndpointFactory {
public:
    static Endpoint getEndpoint(const char *comm_type) {
        auto tcp_port = std::atoi(Util::GetEnv(config::ENV_KEY_TCP_PORT, config::DEFAULT_COMM_TCP_PORT).c_str());
        auto http_port = std::atoi(Util::GetEnv(config::ENV_KEY_HTTP_PORT, config::DEFAULT_COMM_HTTP_PORT).c_str());

        auto IsRdmaComm = [](const char *comm_type) -> bool {
            return strcmp(comm_type, config::COMM_TYPE_RDMA) == 0;
        };
        auto IsHttpComm = [](const char *comm_type) -> bool {
            return strcmp(comm_type, config::COMM_TYPE_HTTP) == 0;
        };

        if (IsRdmaComm(comm_type)) {
            return Endpoint("0.0.0.0", tcp_port);
        }
        if (IsHttpComm(comm_type)) {
            return Endpoint("0.0.0.0", http_port);
        }

        LOG_FATAL("communication type {} unsupported", comm_type);
    }
};
} // namespace communicators