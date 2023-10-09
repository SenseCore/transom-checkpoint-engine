/**
 * @file coordinator.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-03
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "config/world.h"
#include "coordinator/client.h"
#include "coordinator/server.h"
#include "operator/operator.h"
#include "storage/persistence.h"

namespace coordinator {
using storage::Persistence;

/**
 * @brief Coordinator backups cached checkpoint to other nodes and persistents to storage
 * @details Coordinator consists of a k8s-style operator, a server and a client.
 *
 * Server is responsible for
 *
 * 1. backing up ckpts and its metadata to _next node
 * 2. aggregating ckpt metadata to master node, due to deepspeed may read model state from other nodes
 *
 * Caller invokes controller.add(key: str), adding a key to workqueue. Background threads pops
 * the key, doing backup by invoking client. On failure, automatically re-enqueue the key to controller.
 *
 * Upon construction, coordinator interacts with next and prev node. It asks prev node to backup existing cache to itself.
 * Also, it fetches backup cache from next. We call this procedure "bootstrap". After bootstrap, cache state is recovered by best effort.
 */
class Coordinator {
private:
    std::shared_ptr<Server> s_;
    std::shared_ptr<operators::Operator> controller_;

    void bootstrap();

    bool retriveCheckpoint();

    bool retriveCheckpointFromFileSystem();

    bool triggerCheckpoint();

public:
    explicit Coordinator(std::shared_ptr<operators::Operator> controller);

    /**
     * @brief the reconciliation function, to be registered to controller(damn, operator is a keyword in cpp)
     *
     * @param key file name
     * @return true success, false failed and re-enqueue
     */
    static bool Reconcile(std::string key);

    /**
     * @brief start the coordinator. This function must be called
     */
    void Run();
};
} // namespace coordinator