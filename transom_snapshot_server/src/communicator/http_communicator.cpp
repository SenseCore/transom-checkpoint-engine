/**
 * @file http_communicator.cpp
 * @author BaldStrong (BaldStrong@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "communicator/http/http_communicator.h"

#include "communicator/http/http_service_impl.h"

using communicators::Endpoint;
using communicators::HttpCommunicator;
using communicators::HttpServiceImpl;

DEFINE_int32(idle_timeout_s, -1,
             "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");

/**
 * @brief Construct a new http Communicator object
 *
 * @param ep Endpoint
 */
HttpCommunicator::HttpCommunicator(Endpoint ep, std::shared_ptr<operators::Operator> controller) {
    addr_ = ep.addr();
    port_ = ep.port();
    controller_ = controller;
    ready_ = std::make_shared<std::atomic<bool>>(false);
    // close brpc log
    logging::SetMinLogLevel(logging::LOG_NUM_SEVERITIES);
    static HttpServiceImpl http_svc(controller, ready_);
    // Add services into server. Notice the second parameter, because the
    // service is put on stack, we don't want server to delete it, otherwise
    // use brpc::SERVER_OWNS_SERVICE.
    if (server_.AddService(&http_svc, brpc::SERVER_DOESNT_OWN_SERVICE,
                           "/createMetadata   => createMetadata,"
                           "/updateMetadata   => updateMetadata,"
                           "/getMetadata      => getMetadata,"
                           "/getAllMetadata   => getAllMetadata,"
                           "/getAllStorage    => getAllStorage,")
        != 0) {
        LOG_FATAL("Fail to add http_svc: {}", strerror(errno));
    }
    // Start the server.
    // static brpc::ServerOptions options;
    // options.idle_timeout_sec = FLAGS_idle_timeout_s;
    if (server_.Start(port_, nullptr) != 0) {
        LOG_FATAL("Fail to start HttpServer: {}", strerror(errno));
    }
}

/**
 * @brief Destructor, shutdown and close socket on exit
 *
 */
HttpCommunicator::~HttpCommunicator() {
}

void HttpCommunicator::Serve() {
    server_.Join();
}
