/**
 * @file server.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-03
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <memory>
#include <string>
#include <vector>

#include "api/api.h"
#include "communicator/communicator.h"
#include "config/world.h"
#include "operator/operator.h"

#pragma once

namespace coordinator {
using communicators::CommunicatorFactory;
using communicators::RdmaCommunicator;

/**
 * @brief coordinator server for inter-node communication
 * @details Due we transfers huge checkpoint cache utilizing RDMA network, there's no mature framework.
 * So a light-weighted rdma communicator and server is designed for this scenario.
 *
 * Server follows classical socket server implementation. It recursively read request id from socket, then trigger
 * the handler according to request id. Any error will destroy the request context, in which case the thread is finished.
 * Server uses simple but powerful cpp thread. Optimizations, such as epoll, are not critical in the case,
 * due to users usually do checkpoint twice per day, lol.
 *
 */
class Server {
private:
    std::shared_ptr<RdmaCommunicator> communicator_;
    std::shared_ptr<operators::Operator> controller_;

public:
    explicit Server(std::shared_ptr<operators::Operator> controller);

    /**
     * @brief call Serve to let server hang and deal with connections
     */
    void Serve();

private:
    /**
     * @brief handle messages in a seprate routine
     */
    void execute(std::shared_ptr<RdmaCommunicator> c);

    /**
     * @brief handle inter-node backup request
     * @details Detailed procedure are
     *  1. receive metadata through socket from previous node
     *  2. load metadata and entry locally to see if data need overwrite. If get metadata error,
     *     response code is set properly. If metadata not exist, or entry is empty, or size in entry mismatch,
     *     need overwrite.
     *  3. updates metadata, set response code if failed
     *  4. send response. if code not 0, send and exit
     *  5. if overwrite or needOverwrite, rdma handshake, wait until client notify that writes succeeds
     *
     * @param c rdma communicator
     */
    void handleBackup(std::shared_ptr<RdmaCommunicator> c);

    /**
     * @brief handle inter-node load request
     * @details Detailed procedure are
     *  1. receive metadata through socket from previous node
     *  2. load metadata, set response code on error
     *  3. if not only load metadata, load entry. If entry is empty, set response code
     *  3. send response. If code not 0, send and exit
     *  5. rdma handshake, wait until client notify that read succeeds

     * @param c rdma communicator
     */
    void handleLoad(std::shared_ptr<RdmaCommunicator> c);

    /**
     * @brief handle inter-node load request
     * @details Detailed procedure are
     *  1. receive metadata through socket from previous node
     *  2. load metadata, set response code on error
     *  3. if not only load metadata, load entry. If entry is empty, set response code
     *  3. send response. If code not 0, send and exit
     *  5. rdma handshake, wait until client notify that read succeeds

     * @param c rdma communicator
     */
    void handleBatchLoad(std::shared_ptr<RdmaCommunicator> c);

    /**
     * @brief handle inter-node notify backup request
     * @detals Detailed procedure are
     *  1. load metadata locally
     *  2. backup each ckpt to next node
     *  3. send response
     * @param c rdma communicator
     */
    void handleNotifyBackup(std::shared_ptr<RdmaCommunicator> c);
};
} // namespace coordinator