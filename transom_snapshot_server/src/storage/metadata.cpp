/**
 * @file metadata.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-07
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "storage/metadata.h"

#include <sstream>

#include "mysql/mysql.h"

static std::once_flag once_flag;

using storage::MetaClient;
using storage::MysqlClient;
using storage::TransomServiceClient;
using storage::MetadataClientFactory;

std::shared_ptr<MetaClient> MetadataClientFactory::GetClient() {
    auto option = util::Util::GetEnv(config::ENV_KEY_META_CLIENT, config::META_CLIENT_MYSQL);
    if (option == config::META_CLIENT_MYSQL) {
        return std::make_shared<MysqlClient>();
    }
    LOG_FATAL("meta client config {} unsupported", option);
}

MysqlClient::MysqlClient() {
    db_addr_ = util::Util::GetEnv(config::ENV_KEY_MYSQL_ADDR, "0.0.0.0");
    db_port_ = std::atoi(util::Util::GetEnv(config::ENV_KEY_MYSQL_PORT, "3306").c_str());
    db_user_ = util::Util::GetEnv(config::ENV_KEY_MYSQL_USER, "root");
    db_password_ = util::Util::GetEnv(config::ENV_KEY_MYSQL_PASSWORD);

    // hardcode db name
    db_name_ = std::string("engine");

    sql_ = nullptr;
    rw_mutex_.lock();
    sql_ = mysql_init(sql_);
    rw_mutex_.unlock();
    if (!sql_) {
        LOG_FATAL("cannot init mysql: {}", mysql_error(sql_));
    }
    if (!mysql_real_connect(sql_, db_addr_.c_str(), db_user_.c_str(), db_password_.c_str(),
                            db_name_.c_str(), db_port_, nullptr, 0)) {
        LOG_FATAL("cannot connect to mysql: {}", mysql_error(sql_));
    }

    std::call_once(once_flag, [this]() {
        std::string createCmd = "CREATE TABLE IF NOT EXISTS " + std::string(config::MYSQL_TABLE_NAME)
                                + " (FILE_NAME   varchar(512)   PRIMARY KEY     NOT NULL,"
                                  "  NODE_RANK   INT                            NOT NULL,"
                                  "  ITERATION   TEXT                           NOT NULL,"
                                  "  STATE       INT                            NOT NULL,"
                                  "  SIZE        BIGINT UNSIGNED                NOT NULL);";

        int ret = mysql_query(sql_, createCmd.c_str());
        if (ret) {
            LOG_FATAL("create table failed: {}", mysql_error(sql_));
        }
        LOG_TRACE("Table {} created successfully", config::MYSQL_TABLE_NAME);

        if (util::Util::GetEnv(config::ENV_KEY_MYSQL_FLUSH_TABLE, "false") == "true") {
            std::string delete_cmd = "DELETE FROM " + std::string(config::MYSQL_TABLE_NAME) + " ;";
            ret = mysql_query(sql_, delete_cmd.c_str());
            if (ret) {
                LOG_FATAL("create table failed: {}", mysql_error(sql_));
            }
        }
    });
}

MysqlClient::~MysqlClient() {
    mysql_close(sql_);
}

int MysqlClient::Save(api::Metadata &metadata) {
    std::string valueCmd = "'" + metadata.file_name + "', '"
                           + std::to_string(metadata.node_rank) + "', '"
                           + metadata.iteration + "', '"
                           + std::to_string(metadata.state) + "', '"
                           + std::to_string(metadata.size) + "'";
    std::string cmd = "REPLACE INTO " + std::string(config::MYSQL_TABLE_NAME)
                      + " VALUES (" + valueCmd + ");";

    int ret = mysql_query(sql_, cmd.c_str());
    if (ret) {
        LOG_ERROR("insert entry <{}> failed: {}", metadata.String(), mysql_error(sql_));
        return api::STATUS_UNKNOWN_ERROR;
    }
    LOG_TRACE("insert or replace metadata <{}>", metadata.String());
    return api::STATUS_SUCCESS;
}

int MysqlClient::Load(api::Metadata &metadata) {
    std::string cmd = "SELECT * FROM " + std::string(config::MYSQL_TABLE_NAME)
                      + " WHERE FILE_NAME = '" + metadata.file_name + "'";

    int ret = mysql_query(sql_, cmd.c_str());
    if (ret) {
        LOG_ERROR("query entry with primary key <{}> failed: {}", metadata.file_name, mysql_error(sql_));
        return api::STATUS_UNKNOWN_ERROR;
    }

    auto query_res = mysql_store_result(sql_);
    auto rows_num = mysql_num_rows(query_res);
    if (rows_num != 1) {
        if (rows_num == 0) {
            LOG_WARN("query primary key {}, not found in database", metadata.file_name);
            return api::STATUS_NOT_FOUND;
        }
        LOG_ERROR("query primary key {}, result contain {} rows", metadata.file_name, rows_num);
        return api::STATUS_UNKNOWN_ERROR;
    }
    auto row_data = mysql_fetch_row(query_res);
    metadata.file_name = row_data[0];
    metadata.node_rank = std::atoi(row_data[1]);
    metadata.iteration = row_data[2];
    metadata.state = (api::CheckpointState)std::atoi(row_data[3]);
    metadata.size = static_cast<size_t>(std::atoll(row_data[4]));

    mysql_free_result(query_res);
    return api::STATUS_SUCCESS;
}

int MysqlClient::UpdateState(const std::string &file_name, const api::CheckpointState &state) {
    std::string updateCmd = "UPDATE " + std::string(config::MYSQL_TABLE_NAME)
                            + " SET STATE='"
                            + std::to_string(state)
                            + "' WHERE FILE_NAME='"
                            + file_name + "';";
    int ret = mysql_query(sql_, updateCmd.c_str());
    if (ret) {
        LOG_ERROR("update entry with primary key <{}>, state to {}, failed: {}",
                  file_name, api::CheckpointStateString(state), mysql_error(sql_));
        return api::STATUS_UNKNOWN_ERROR;
    }

    LOG_TRACE("update metadata {} State to {}", file_name, CheckpointStateString(state));
    return api::STATUS_SUCCESS;
}

int MysqlClient::DeleteByFileName(const std::string &file_name) {
    std::string delete_cmd = "DELETE FROM " + std::string(config::MYSQL_TABLE_NAME)
                             + " WHERE FILE_NAME='" + file_name + "';";
    int ret = mysql_query(sql_, delete_cmd.c_str());
    if (ret) {
        LOG_ERROR("delete entry with primary key <{}> failed: {}", file_name, mysql_error(sql_));
        return api::STATUS_UNKNOWN_ERROR;
    }

    LOG_TRACE("delete metadata {}", file_name);
    return api::STATUS_SUCCESS;
}

int MysqlClient::BatchLoad(api::BatchLoadFilter &filter, std::vector<api::Metadata> &vec) {
    std::vector<std::string> filters;
    if (filter.node_rank >= 0) {
        filters.push_back("NODE_RANK = '" + std::to_string(filter.node_rank) + "'");
    }
    if (filter.iteration.size() > 0) {
        filters.push_back("ITERATION = '" + filter.iteration + "'");
    }
    if (filter.state >= api::CheckpointState::PENDING && filter.state < api::CheckpointState::STATE_NUM) {
        filters.push_back("STATE= '" + std::to_string(filter.state) + "'");
    }
    auto filter_str = util::Util::Join(std::ref(filters), " AND ");

    std::string cmd = "SELECT * FROM " + std::string(config::MYSQL_TABLE_NAME);
    if (!filter_str.empty()) {
        cmd += " WHERE " + filter_str;
    }

    int ret = mysql_query(sql_, cmd.c_str());
    if (ret) {
        LOG_ERROR("batch load with condition <{}> failed: {}", filter_str, mysql_error(sql_));
        return api::STATUS_UNKNOWN_ERROR;
    }

    auto query_res = mysql_store_result(sql_);
    auto rows_num = mysql_num_rows(query_res);

    if (rows_num == 0) {
        return api::STATUS_NOT_FOUND;
    }

    for (auto i = 0; i < rows_num; i++) {
        auto rowData = mysql_fetch_row(query_res);
        api::Metadata metadata;
        metadata.file_name = rowData[0];
        metadata.node_rank = std::atoi(rowData[1]);
        metadata.iteration = rowData[2];
        metadata.state = (api::CheckpointState)std::atoi(rowData[3]);
        metadata.size = std::atoll(rowData[4]);
        vec.push_back(metadata);
    }

    mysql_free_result(query_res);
    return api::STATUS_SUCCESS;
}