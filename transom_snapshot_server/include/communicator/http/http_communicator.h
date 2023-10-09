/**
 * @file http_communicator.h
 * @author BaldStrong (BaldStrong@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "brpc/server.h"

#include "communicator/endpoint.h"
#include "operator/operator.h"

namespace communicators {
/**
 * @brief A wrapper of brpc rest server, serving for intra-node communication
 * @details A wrapper is needed due to server also add key to channel for reconciliation
 */
class HttpCommunicator {
private:
    /**
     * @brief http server address
     */
    std::string addr_;

    /**
     * @brief http server port
     */
    uint16_t port_;

    /**
     * @brief operator for reconciliation
     */
    std::shared_ptr<operators::Operator> controller_;

    /**
     * @brief real http server utilizing brpc
     */
    brpc::Server server_;

    /**
     * @brief indicator, indicating if bootstrap finishes
     */
    std::shared_ptr<std::atomic<bool>> ready_;

public:
    HttpCommunicator(Endpoint ep, std::shared_ptr<operators::Operator> controller);
    ~HttpCommunicator();

    /**
     * @brief mark the ready indicator as true
     */
    void MarkReady() {
        ready_->store(true);
    }

    /**
     * @brief start serving, this function is blocking
     */
    void Serve();
};
} // namespace communicators
