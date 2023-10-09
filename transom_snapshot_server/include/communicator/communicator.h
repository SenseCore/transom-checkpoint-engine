/**
 * @file communicator.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <memory>
#include <string>

#include "buffer/buffer.h"
#include "communicator/http/http_communicator.h"
#include "communicator/rdma_communicator.h"
#include "config/config.h"
#include "operator/operator.h"

namespace communicators {
/**
 * @brief CommunicatorFactory reads endpoint configuration and returns a specified communicator
 */
class CommunicatorFactory {
public:
    /**
     * @brief return a http communicator
     * @param controller operator used to init communicator
     * @return http communicator
     */
    static std::shared_ptr<HttpCommunicator> getHttpCommunicator(
        std::shared_ptr<operators::Operator> controller) {
        auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_HTTP);
        return std::make_shared<HttpCommunicator>(ep, controller);
    }

    /**
     * @brief return a rdma communicator
     * @param fd socket fd
     * @return rdma communicator
     */
    static std::shared_ptr<RdmaCommunicator> getRdmaCommunicator(int fd = -1) {
        auto ep = EndpointFactory::getEndpoint(config::COMM_TYPE_RDMA);
        return std::make_shared<RdmaCommunicator>(ep, fd);
    }
};
} // namespace communicators
