/**
 * @file client.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <string>
#include <vector>

#include "api/api.h"
#include "communicator/communicator.h"
#include "config/world.h"
#include "storage/storage.h"
#include "util/util.h"

namespace coordinator {
using communicators::CommunicatorFactory;
using communicators::RdmaCommunicator;
using util::Util;
using api::Metadata;
using api::DataEntry;
using config::WorldState;

/**
 * @brief client biz logic implementation
 */
class ClientUtil {
public:
    /**
     * @brief backup local checkpoint to next node. Note data is prepared outside of this function.
     * if overwrite, data will be transferred to remote, otherwise only update metadata.
     * @details Detailed procedure:
     *  1. get next node IP, if world size is 1, skip
     *  2. connect to peer node
     *  3. send routine 1, which is INTER_NODE_BACKUP
     *  4. send metadata through socket
     *  5. recv remote node response, containing response code, remote address and needOverwrite
     *  6. if response code not 0, return
     *  7. if caller sets overwrite or needOverwrite, rdma write and sync
     *
     * @param req reference of inter node backup request
     * @param rsp reference of inter node backup response
     */
    bool Backup(api::InterNodeBackupRequest &req, api::InterNodeBackupResponse &rsp);

    /**
     * @brief load target checkpoint from a node. Note loaded data should be released manually
     * @details Detailed procedure:
     *  1. get target node IP and connect to remote node
     *  2. send routine 2, which is INTER_NODE_LOAD
     *  3. send request, waiting for response
     *  4. continue if not only load metadata, rmda read and sync
     *
     * @param req reference of inter node load request
     * @param rsp reference of inter node load response
     */
    bool LoadRemote(api::InterNodeLoadRequest &req, api::InterNodeLoadResponse &rsp);

    /**
     * @brief load target checkpoint from a node. Note loaded data should be released manually
     * @details Detailed procedure:
     *  1. get target node IP and connect to remote node
     *  2. send routine 2, which is INTER_NODE_LOAD
     *  3. send request, waiting for response
     *  4. continue if not only load metadata, rmda read and sync
     *
     * @param req reference of inter node batch load request
     * @param rsp reference of inter node batch load response
     */
    bool BatchLoadRemote(api::InterNodeBatchLoadRequest &req, api::InterNodeBatchLoadResponse &rsp);

    /**
     * @brief load target checkpoint from FileSystem in case cache is lost
     *
     * @return true success
     */
    bool BatchLoadFromFileSystem();

    /**
     * @brief notify prev node to backup checkpoint
     * @details Detailed procedure:
     *  1. get prev node IP and connect to remote node
     *  2. send routine, NOTIFY_BACKUP, request is empty
     *  3. waiting for response(remote may do Backup many times, which is in other threads concurrently)
     *
     * @param rsp reference of inter node notify backup response
     */
    bool NotifyBackup(api::InterNodeNotifyBackupResponse &rsp);

private:
    /**
     * @brief get next node IP, if world size is 1, return IP of self
     * @param addr store IP into this string
     * @return false means parse failure
     */
    bool getNextNode(std::string &addr);

    /**
     * @brief get prev node IP, if world size is 1, return IP of self
     * @param addr store IP into this string
     * @return false means parse failure
     */
    bool getPrevNode(std::string &addr);

    /**
     * @brief get IP of node with given rank
     * @param node_rank node rank
     * @return false means failure
     */
    bool getNodeIP(int node_rank, std::string &addr);
};
} // namespace coordinator