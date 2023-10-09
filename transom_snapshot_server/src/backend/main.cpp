/**
 * @file main.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "communicator/communicator.h"
#include "coordinator/coordinator.h"
#include "logger/logger.h"
#include "monitor/monitor.h"
#include "operator/operator.h"

int main(int argc, char **argv) {
    logger::Logger::InitLogger();
    LOG_INFO("logger inited");

    /* start memory monitor */
    monitor::MemoryMonitor::Instance().Start();

    /* start a operator for reconciliation */
    auto controller = std::make_shared<operators::Operator>();
    controller->SetHandler(coordinator::Coordinator::Reconcile);
    controller->Run();

    /* start local server for intra-node comm */
    auto backend = communicators::CommunicatorFactory::getHttpCommunicator(controller);

    /* start inter node server */
    coordinator::Coordinator coordinator(controller);
    coordinator.Run();

    /* mark backend as ready, which means coordinator finishes bootstraping */
    backend->MarkReady();
    backend->Serve();
    return 0;
}
