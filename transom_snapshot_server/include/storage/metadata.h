/**
 * @file metadata.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <map>
#include <memory>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#include "brpc/channel.h"
#include "mysql/mysql.h"

#include "api/api.h"
#include "util/util.h"

namespace storage {
using api::CheckpointState;

/**
 * @brief metadata client interface, operating checkpoint file metadata with database
 */
class MetaClient {
public:
    MetaClient() = default;
    ~MetaClient() = default;

    /**
     * @brief insert metadata to database, all fields are requried
     *
     * @param metadata checkpoint file metadata
     * @return int status code, non-zero means failure
     */
    virtual int Save(api::Metadata &metadata) = 0;

    /**
     * @brief load metadata from database, job key and file name are required
     *
     * @param metadata checkpoint file metadata to store loaded result
     * @return int status code, non-zero means failure
     */
    virtual int Load(api::Metadata &metadata) = 0;

    /**
     *@brief update checkpoint file state
     *
     * @param file_name checkpoint file name
     * @param state updated state
     * @return int status code, non-zero means failure
     */
    virtual int UpdateState(const std::string &file_name, const api::CheckpointState &state) = 0;

    /**
     * @brief delete checkpoint file record by file name, job key is persisted at server, thus not needed
     *
     * @param file_name file name
     * @return int status code, non-zero means failure
     */
    virtual int DeleteByFileName(const std::string &file_name) = 0;

    /**
     * @brief batch load checkpoint file records by filter
     *
     * @param filter aka list option
     * @param vec where result stores
     * @return int status code, non-zero means failure
     */
    virtual int BatchLoad(api::BatchLoadFilter &filter, std::vector<api::Metadata> &vec) = 0;
};

class MysqlClient : public MetaClient {
private:
    std::string db_addr_;
    int db_port_;
    std::string db_user_;
    std::string db_password_;
    std::string db_name_;

    MYSQL *sql_;
    inline static std::shared_mutex rw_mutex_ = {};

public:
    MysqlClient();
    ~MysqlClient();

    int Save(api::Metadata &metadata) override;
    int Load(api::Metadata &metadata) override;
    int UpdateState(const std::string &file_name, const api::CheckpointState &state) override;
    int DeleteByFileName(const std::string &file_name) override;
    int BatchLoad(api::BatchLoadFilter &filter, std::vector<api::Metadata> &vec) override;
};

class TransomServiceClient : public MetaClient {
private:
    std::string addr_;
    int port_;
    brpc::Channel chan_;

public:
    TransomServiceClient();
    ~TransomServiceClient();

    int Save(api::Metadata &metadata) override;
    int Load(api::Metadata &metadata) override;
    int UpdateState(const std::string &file_name, const api::CheckpointState &state) override;
    int DeleteByFileName(const std::string &file_name) override;
    int BatchLoad(api::BatchLoadFilter &filter, std::vector<api::Metadata> &vec) override;
};

/**
 * @brief metadata client factory, generate metadata client by environment variable
 */
class MetadataClientFactory {
public:
    /**
     * @brief return a new metadata client
     * @return MetaClient
     */
    static std::shared_ptr<MetaClient> GetClient();
};
} // namespace storage