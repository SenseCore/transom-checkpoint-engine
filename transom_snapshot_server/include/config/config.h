/**
 * @file config.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief user and system config
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <string>

namespace config {
/**
 * @brief initial buffer capacity, 4k, enough for most cases
 */
constexpr size_t BUFFER_BLOCK_SIZE = 4096;

/**
 * @brief power of 2 to BUFFER_BLOCK_SIZE, used for fast resizing
 * @todo is it actually needed?
 */
constexpr size_t BUFFER_BLOCK_POW = 12;

/**
 * @brief when add host pointer to buffer, may add a nullptr, should convert to 0
 */
constexpr size_t BUFFER_NULL_VAL = 0;

/**
 * @brief when sending data through rdma networking, rdma has single message limit.
 * Also considering network is not always stable, should set to reasonable size
 */
constexpr size_t RDMA_CHUNK_SIZE = 1073741824; /* 1GB */

/**
 * @brief A state indicating Queue Pair is abnormal.
 * @details For QP state, refer to https://www.rdmamojo.com/2012/05/05/qp-state-machine/.
 * We consider IBV_QPS_RESET, IBV_QPS_ERR, IBV_QPS_UNKNOWN as abnormal.
 */
constexpr int RDMA_QP_STATE_ABNORMAL = 99;

/**
 * @brief RDMA queue poll interval at first time
 * @details when polling completion queue, too frequent result into higher CPU usage.
 * min ms used for the first time. Expotention backoff is applied to latter retries.
 */
constexpr size_t RDMA_SLEEP_MIN_MILLISECONDS = 25;

/**
 * @brief RDMA queue poll max interval
 */
constexpr size_t RDMA_SLEEP_MAX_MILLISECOND = 200;

/**
 * @brief max elements in queue, if elements exceeds limit, error 12 NOMEM is thrown
 */
constexpr size_t RDMA_CQ_SIZE = 100;

/**
 * @brief after rdma write, send this msg by socket for validation and synchronization
 * @todo is it needed?
 */
constexpr auto RDMA_WRITE_MSG = "W";

/**
 * @brief after rdma read, send this msg by socket for validation and synchronization
 * @todo is it needed?
 */
constexpr auto RDMA_READ_MSG = "R";

/**
 * @brief workqueue max depth, if workqueue is full, add method will block
 */
constexpr size_t OPERATOR_WORKQUEUE_BUFFER = 10000;

/**
 * @brief ratelimiter rate for workqueue, unit is per second
 */
constexpr double OPERATOR_RATELIMITER_RATE = 500;

/**
 * @brief concurrent threads to reconcile in operator
 */
constexpr int OPERATOR_N_THREADS = 8;

/**
 * @brief environment variable key to configure log level
 */
constexpr auto ENV_KEY_LOG_LEVEL = "CKPT_ENGINE_LOG_LEVEL";

/**
 * @brief default log level
 */
constexpr int SPDLOG_DEFAULT_LOG_LEVEL = 0;

/**
 * @brief environment variable key to configure log pattern
 */
constexpr auto ENV_KEY_LOG_PATTERN = "CKPT_ENGINE_LOG_PATTERN";

/**
 * @brief default log pattern
 */
constexpr auto SPDLOG_LOG_DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%t] %^[%l]%$ [%!] %v";

/**
 * @brief environment variable key to configure metadata client type
 */
constexpr auto ENV_KEY_META_CLIENT = "CKPT_ENGINE_META_CLIENT";

/**
 * @brief metadata client to connect to mysql
 */
constexpr auto META_CLIENT_MYSQL = "mysql";

/**
 * @brief mysql table name
 */
constexpr auto MYSQL_TABLE_NAME = "METADATA";

/**
 * @brief environment variable key to configure mysql address
 */
constexpr auto ENV_KEY_MYSQL_ADDR = "CKPT_ENGINE_MYSQL_ADDR";

/**
 * @brief environment variable key to configure mysql port
 */
constexpr auto ENV_KEY_MYSQL_PORT = "CKPT_ENGINE_MYSQL_PORT";

/**
 * @brief environment variable key to configure mysql username
 */
constexpr auto ENV_KEY_MYSQL_USER = "CKPT_ENGINE_MYSQL_USER";

/**
 * @brief environment variable key to configure mysql password
 */
constexpr auto ENV_KEY_MYSQL_PASSWORD = "CKPT_ENGINE_MYSQL_PASSWORD";

/**
 * @brief environment variable key to configure if flush table on connected to mysql
 */
constexpr auto ENV_KEY_MYSQL_FLUSH_TABLE = "CKPT_ENGINE_MYSQL_FLUSH";

/**
 * @brief communicator type, http
 */
constexpr auto COMM_TYPE_HTTP = "http";

/**
 * @brief communicator type, rdma
 */
constexpr auto COMM_TYPE_RDMA = "rdma";

/**
 * @brief environment variable key to configure TCP port in rdma communicator
 */
constexpr auto ENV_KEY_TCP_PORT = "CKPT_ENGINE_TCP_PORT";

/**
 * @brief environment variable key to configure http port in http communicator
 */
constexpr auto ENV_KEY_HTTP_PORT = "CKPT_ENGINE_HTTP_PORT";

/**
 * @brief default tcp port in rdma communicator
 */
constexpr auto DEFAULT_COMM_TCP_PORT = "18080";

/**
 * @brief default http port in http communicator
 */
constexpr auto DEFAULT_COMM_HTTP_PORT = "15345";

/**
 * @brief max wait interval during bootstrap
 */
constexpr auto BOOTSTRAP_MAX_RETRY_INTERVAL_SECONDS = 10;

/**
 * @brief min wait interval during bootstrap
 */
constexpr auto BOOTSTRAP_MIN_RETRY_INTERVAL_SECONDS = 1;

/**
 * @brief use 8 threads to concurrent bootstrap, to use multiple NICs
 */
constexpr auto BOOTSTRAP_CONCURRENT_THREADS = 8;

/**
 * @brief retry interval during bootstrap
 */
constexpr auto CHECK_BOOTSTRAP_RETRY_INTERVAL_SECONDS = BOOTSTRAP_MAX_RETRY_INTERVAL_SECONDS * 5;

/**
 * @brief **Just for debugging**, environment variable key to configure whether to skip bootstrap
 */
constexpr auto ENV_KEY_SKIP_BOOTSTRAP = "CKPT_ENGINE_SKIP_BOOTSTRAP";

/**
 * @brief **Just for debugging**, skip bootstrap
 */
constexpr auto EXPERIMENTAL_SKIP_BOOTSTRAP = "on";

/**
 * @brief max num of iteration saved in cache
 */
constexpr auto ENV_MAX_ITERATION_IN_CACHE = "CKPT_ENGINE_MAX_ITERATION_IN_CACHE";

/**
 * @brief default max interation saved in cache
 */
constexpr auto DEFAULT_MAX_ITERATION_IN_CACHE = "999";

/**
 * @brief cgroup directory to read memory state
 */
constexpr auto MEM_CGROUP_DIR = "/sys/fs/cgroup/memory/";

/**
 * @brief interval to re-collect cgroup stat
 */
constexpr auto MEM_WATCH_PERIOD_SECONDS = 1000000;

/**
 * @brief environment variable key to configure cache memory consumption limit
 */
constexpr auto ENV_KEY_MEMORY_LIMIT_GB = "CKPT_ENGINE_MEM_LIMIT_GB";

/**
 * @brief **Just for debugging**, environment variable key to configure whether to skip persistence
 */
constexpr auto IS_PERSISTENT = "CKPT_ENGINE_ENABLE_PERSISTENT";

/**
 * @brief environment variable key to configure transom job key
 */
constexpr auto ENV_KEY_TRANSOM_JOB_KEY = "TRANSOM_JOBNAME";

/**
 * @brief default transom job key
 */
constexpr auto DEFAULT_TRANSOM_JOB_KEY = "test-job";

/**
 * @brief environment variable key to configure transom node rank
 */
constexpr auto ENV_KEY_TRANSOM_RANK = "TRANSOM_RANK";

/**
 * @brief default transom node rank
 */
constexpr auto DEFAULT_TRANSOM_RANK = "0";

/**
 * @brief environment variable key to configure transom world size
 */
constexpr auto ENV_KEY_TRANSOM_WORLD_SIZE = "TRANSOM_WORLD_SIZE";

/**
 * @brief default transom world size
 */
constexpr auto DEFAULT_TRANSOM_WORLD_SIZE = "1";

/**
 * @brief environment variable key to configure transom hosts
 */
constexpr auto ENV_KEY_TRANSOM_HOSTS = "TRANSOM_HOSTS";
} // namespace config