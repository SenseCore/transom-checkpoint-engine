/**
 * @file api.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief api.h defines everything needed in client & server IPC. It also defines socket server communication 'proto'.
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "buffer/buffer.h"

namespace api {
using buffer::Buffer;

/**
 * @brief common status code used in all clients, indicating success
 */
constexpr int STATUS_SUCCESS = 0;

/**
 * @brief common status code used in all clients, indicating an unknown error.
 * @details caller will alert in logs and report to its caller. No special logic is applied to this kind of error
 */
constexpr int STATUS_UNKNOWN_ERROR = 1;

/**
 * @brief common status code used in all clients, indicating insufficient memory.
 * @details For example, when allocating memory for cache, it may return this type of error.
 */
constexpr int STATUS_OOM = 2;

/**
 * @brief common status code used in all clients, indicating a not-found error.
 * @details Special logic is applied to this type of error. For example, during reconciliation, a not found error indicate
 * record not available in database, and reconciliation will be aborted.
 */
constexpr int STATUS_NOT_FOUND = 404;

inline bool IsSuccess(int code) {
    return code == api::STATUS_SUCCESS;
}

inline bool IsNotFound(int code) {
    return code == api::STATUS_NOT_FOUND;
}

inline bool IsOOM(int code) {
    return code == api::STATUS_OOM;
}

/**
 * @brief checkpoint file state
 */
enum CheckpointState {
    /**
     * @brief initial state, request has been issued, but data not written to cache yet
     */
    PENDING = 0,

    /**
     * @brief data has been written to cache, but has not been backup
     */
    CACHED = 1,

    /**
     * @brief data has backup to next node, but not persistent to storage yet
     */
    BACKED_UP = 2,

    /**
     * @brief the desired state, data has been persistent to storage
     */
    PERSISTENT = 3,

    /**
     * @brief unexpected state, raised by database entry loss, or data corruption, or whatever else.
     * @details broken record will be skipped in reconciliation. User should examine his infrastructure to figure out the root cause.
     */
    BROKEN = 4,

    /**
     * @brief has been evicted due to memory insufficient or aging
     */
    OBSOLESCENT = 5,

    /**
     * @brief number of states in total
     */
    STATE_NUM = 6,

    /**
     * @brief a special state for wildcard query
     */
    STATE_ANY = -1,
};

/**
 * @brief convert checkpoint state into human-readable string
 *
 * @param in checkpoint state to convert
 * @return the string identifier
 */
const char *CheckpointStateString(CheckpointState in);

/**
 * @brief request id used in inter-node socket communication
 */
enum Routine {
    /**
     * @brief backup checkpoint cache data to remote note
     */
    INTER_NODE_BACKUP = 1,

    /**
     * @brief load checkpoint cache from remote node
     */
    INTER_NODE_LOAD = 2,

    /**
     * @brief batch load checkpoint caches from remote node
     */
    INTER_NODE_BATCH_LOAD = 3,

    /**
     * @brief notify remote node to re-backup all its local checkpoint cache
     */
    INTER_NODE_NOTIFY_BACKUP = 4,
};

/**
 * @brief convert request id into human-readable string
 *
 * @param in request id to convert
 * @return the string identifier
 */
const char *RoutineString(Routine in);

class Serializable {
public:
    /**
     * @brief serialize instance and writes to buffer
     * @param buffer a well-designed data structure that holds serialized data of any object
     */
    virtual void Marshal(Buffer &buffer) = 0;

    /**
     * @brief unserialize instance from buffer
     * @param buffer a well-designed data structure that holds serialized data of any object
     */
    virtual void Unmarshal(Buffer &buffer) = 0;

    /**
     * @brief dump metadata to human-readable string for debugging or logging
     * @return string containing all fields
     */
    virtual std::string String() = 0;
};

/**
 * @brief checkpoint file metadata, mark a checkpoint file unique
 * @details A straightforward view is that a metadata instance corresponds to an entry in database. Metadata is widely used
 * throughout this project. It's a concensus of the checkpoint file, unrelated to in-memory cache at each node, e.g. file name, state.
 * Cache-related attribute, such as address, is stored at another struct named `DataEntry`.
 * @todo maybe utilizing an ORM framwork is better?
 */
class Metadata final : public Serializable {
public:
    Metadata() = default;
    Metadata(std::string in_job_name, std::string in_file_name, int in_node_rank = -1, std::string in_iteration = "",
             CheckpointState in_state = CheckpointState::PENDING, size_t in_size = 0);
    Metadata(const Metadata &metadata);
    ~Metadata() = default;

    /**
     * @brief unique key of job
     */
    std::string job_name;

    /**
     * @brief path of checkpoint file
     */
    std::string file_name;

    /**
     * @brief rank of the node who saves this checkpoint file
     */
    int node_rank;

    /**
     * @brief indicate when the checkpoint is triggered
     * @details in native pytorch, extract from 'epoch'; in deepspeed, extract from file path.
     * If extraction fails, set it to "unknown"
     */
    std::string iteration;

    /**
     * @brief current state of this checkpoint file
     */
    CheckpointState state;

    /**
     * @brief amount of data, the unit is 'bytes'
     */
    size_t size;

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;
};

/**
 * @brief cache-related attributes of a checkpoint file
 * @details Currently we have three attributes, namely address, pid and memfd. These attributes are not a concensus, thus
 * a data-entry is stored in server's memory. Upon restart, server re-builds data entries during bootstrap.
 */
class DataEntry final : public Serializable {
public:
    DataEntry() {
        address = 0;
    }

    DataEntry(size_t ptr, int _pid, int _memfd) {
        address = ptr;
        pid = _pid;
        memfd = _memfd;
    }

    DataEntry(const DataEntry &entry) {
        address = entry.address;
        pid = entry.pid;
        memfd = entry.memfd;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String();

    /**
     * @brief address of the cache in heap memory
     */
    size_t address;

    /**
     * @brief id of the process who holds this cache
     */
    int pid = 0;

    /**
     * @brief memfd of the heap memory
     * @details to know more about `memfd`, refer to [memfd](https://man7.org/linux/man-pages/man2/memfd_create.2.html)
     */
    int memfd = 0;
};

/**
 * @brief basic response class, only containing status code
 */
class BasicResponse {
public:
    BasicResponse() :
        code(STATUS_SUCCESS) {
    }

    /**
     * @brief response code
     */
    int code;
};

/**
 * @brief body of inter-node backup request
 */
class InterNodeBackupRequest final : public Serializable {
public:
    InterNodeBackupRequest() = default;

    /**
     * @brief serialize instance and writes to buffer
     * @param buffer a well-designed data structure that holds serialized data of any object
     */
    InterNodeBackupRequest(Metadata in_metadata, DataEntry in_entry, bool in_only_metadata) {
        metadata = in_metadata;
        data_entry = in_entry;
        only_metadata = in_only_metadata;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief metadata of the checkpoint file to backup
     */
    Metadata metadata;

    /**
     * @brief data entry of the checkpoint at sender node
     */
    DataEntry data_entry;

    /**
     * @brief only update metadata if this field is set to true; otherwise transmit cache data
     */
    bool only_metadata;
};

/**
 * @brief body of inter-node backup response
 */
class InterNodeBackupResponse final : public Serializable, public BasicResponse {
public:
    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;
};

/**
 * @brief body of inter-node load checkpoint file request
 */
class InterNodeLoadRequest final : public Serializable {
public:
    InterNodeLoadRequest() = default;

    InterNodeLoadRequest(Metadata in_metadata, bool in_only_metadata) {
        metadata = in_metadata;
        only_metadata = in_only_metadata;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief metadata of checkpoint file to load, only job_name and file_name is required
     */
    Metadata metadata;

    /**
     * @brief set to true if only load metadata; otherwise will load cache through RDMA
     */
    bool only_metadata;
};

/**
 * @brief body of inter-node load response
 */
class InterNodeLoadResponse final : public Serializable, public BasicResponse {
public:
    InterNodeLoadResponse() = default;
    InterNodeLoadResponse(Metadata in_metadata) {
        metadata = in_metadata;
        code = STATUS_SUCCESS;
    }

    InterNodeLoadResponse(Metadata in_metadata, DataEntry in_data_entry, int in_code) {
        metadata = in_metadata;
        data_entry = in_data_entry;
        code = in_code;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief complete metadata of the file to load
     */
    Metadata metadata;

    /**
     * @brief `DataEntry` of the file at remote node
     */
    DataEntry data_entry;
};

/**
 * @brief filter when batch load checkpoint files.
 * Three conditions are available, namely node_rank, iteration, state.
 */
class BatchLoadFilter final : public Serializable {
public:
    BatchLoadFilter(int in_node_rank = -1, std::string in_iteration = "",
                    CheckpointState in_state = CheckpointState::STATE_ANY) {
        node_rank = in_node_rank;
        iteration = in_iteration;
        state = in_state;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief -1 for unspecified, otherwise filter result with given node rank
     */
    int node_rank;

    /**
     * @brief empty for unspecified, otherwise filter result with given iteration
     */
    std::string iteration;

    /**
     * @brief -1 for unspecified, otherwise filter result with given state
     */
    CheckpointState state;
};

/**
 * @brief body of inter-node batch load request
 */
class InterNodeBatchLoadRequest final : public Serializable {
public:
    InterNodeBatchLoadRequest(int node_rank = 0, std::string iteration = "",
                              CheckpointState state = api::CheckpointState::STATE_ANY,
                              bool in_only_metadata = false) {
        filter.node_rank = node_rank;
        filter.iteration = iteration;
        filter.state = state;
        only_metadata = in_only_metadata;
    }

    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief filter, like 'list option'
     */
    BatchLoadFilter filter;

    /**
     * @brief set to true if not loading real cache data
     */
    bool only_metadata;
};

/**
 * @brief body of inter-node batch load response
 */
class InterNodeBatchLoadResponse final : public Serializable, public BasicResponse {
public:
    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;

    /**
     * @brief a vector containing response, with each a response of a certain checkpoint file
     */
    std::vector<InterNodeLoadResponse> responses;
};

/**
 * @brief body of inter-node notify backup response. Request body is empty, so it's ignored
 *
 */
class InterNodeNotifyBackupResponse final : public Serializable, public BasicResponse {
public:
    void Marshal(Buffer &buffer) override;
    void Unmarshal(Buffer &buffer) override;
    std::string String() override;
};
} // namespace api