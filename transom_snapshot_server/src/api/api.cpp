/**
 * @file api.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <api/api.h>

namespace api {
static std::map<CheckpointState, const char *> checkpoint_state_string_map = {
    {CheckpointState::PENDING, "PENDING"},
    {CheckpointState::CACHED, "CACHED"},
    {CheckpointState::BACKED_UP, "BACKED_UP"},
    {CheckpointState::PERSISTENT, "PERSISTENT"},
    {CheckpointState::BROKEN, "BROKEN"},
    {CheckpointState::OBSOLESCENT, "OBSOLESCENT"},
};

const char *CheckpointStateString(CheckpointState in) {
    return checkpoint_state_string_map[in];
}

static std::map<api::Routine, const char *> routine_string_map = {
    {Routine::INTER_NODE_BACKUP, "INTER_NODE_BACKUP"},
    {Routine::INTER_NODE_LOAD, "INTER_NODE_LOAD"},
    {Routine::INTER_NODE_BATCH_LOAD, "INTER_NODE_BATCH_LOAD"},
    {Routine::INTER_NODE_NOTIFY_BACKUP, "INTER_NODE_NOTIFY_BACKUP"},
};

const char *RoutineString(Routine in) {
    return routine_string_map[in];
}

Metadata::Metadata(std::string in_job_name, std::string in_file_name, int in_node_rank,
                   std::string in_iteration, CheckpointState in_state, size_t in_size) {
    job_name = in_job_name;
    file_name = in_file_name;
    node_rank = in_node_rank;
    iteration = in_iteration;
    state = in_state;
    size = in_size;
}

Metadata::Metadata(const Metadata &metadata) {
    job_name = metadata.job_name;
    file_name = metadata.file_name;
    node_rank = metadata.node_rank;
    iteration = metadata.iteration;
    state = metadata.state;
    size = metadata.size;
}

void Metadata::Marshal(Buffer &buffer) {
    buffer.AddString(job_name);
    buffer.AddString(file_name);
    buffer.Add(node_rank);
    buffer.AddString(iteration);
    buffer.Add(state);
    buffer.Add(size);
}

void Metadata::Unmarshal(Buffer &buffer) {
    job_name = buffer.GetString();
    file_name = buffer.GetString();
    node_rank = buffer.Get<int>();
    iteration = buffer.GetString();
    state = buffer.Get<CheckpointState>();
    size = buffer.Get<size_t>();
}

std::string Metadata::String() {
    std::stringstream ss;
    ss << "JobName " << job_name
       << " FileName " << file_name
       << " NodeRank " << node_rank
       << " Iteration " << iteration
       << " state " << CheckpointStateString(state)
       << " size " << size;
    return ss.str();
}

void DataEntry::Marshal(Buffer &buffer) {
    buffer.Add(address);
    buffer.Add(pid);
    buffer.Add(memfd);
}

void DataEntry::Unmarshal(Buffer &buffer) {
    address = buffer.Get<size_t>();
    pid = buffer.Get<int>();
    memfd = buffer.Get<int>();
}

std::string DataEntry::String() {
    std::stringstream ss;
    ss << "Address " << reinterpret_cast<void *>(address)
       << " pid " << pid
       << " memfd " << memfd;
    return ss.str();
}

void InterNodeBackupRequest::Marshal(Buffer &buffer) {
    metadata.Marshal(buffer);
    data_entry.Marshal(buffer);
    buffer.Add(only_metadata);
}

void InterNodeBackupRequest::Unmarshal(Buffer &buffer) {
    metadata.Unmarshal(buffer);
    data_entry.Unmarshal(buffer);
    only_metadata = buffer.Get<bool>();
}

std::string InterNodeBackupRequest::String() {
    std::stringstream ss;
    ss << "metadata: " << metadata.String()
       << " DataEntry: " << data_entry.String()
       << " OnlyMetadata: " << only_metadata;
    return ss.str();
}

void InterNodeBackupResponse::Marshal(Buffer &buffer) {
    buffer.Add(code);
}

void InterNodeBackupResponse::Unmarshal(Buffer &buffer) {
    code = buffer.Get<int>();
}

std::string InterNodeBackupResponse::String() {
    std::stringstream ss;
    ss << "Code " << code;
    return ss.str();
}

void InterNodeLoadRequest::Marshal(Buffer &buffer) {
    metadata.Marshal(buffer);
    buffer.Add(only_metadata);
}

void InterNodeLoadRequest::Unmarshal(Buffer &buffer) {
    metadata.Unmarshal(buffer);
    only_metadata = buffer.Get<bool>();
}

std::string InterNodeLoadRequest::String() {
    std::stringstream ss;
    ss << "Metadata" << metadata.String()
       << " OnlyMetadata " << only_metadata;
    return ss.str();
}

void InterNodeLoadResponse::Marshal(Buffer &buffer) {
    metadata.Marshal(buffer);
    data_entry.Marshal(buffer);
    buffer.Add(code);
}

void InterNodeLoadResponse::Unmarshal(Buffer &buffer) {
    metadata.Unmarshal(buffer);
    data_entry.Unmarshal(buffer);
    code = buffer.Get<int>();
}

std::string InterNodeLoadResponse::String() {
    std::stringstream ss;
    ss << "Metadata: " << metadata.String()
       << " DataEntry: " << data_entry.String()
       << " Code " << code;
    return ss.str();
}

void BatchLoadFilter::Marshal(Buffer &buffer) {
    buffer.Add(node_rank);
    buffer.AddString(iteration);
    buffer.Add(state);
}

void BatchLoadFilter::Unmarshal(Buffer &buffer) {
    node_rank = buffer.Get<int>();
    iteration = buffer.GetString();
    state = buffer.Get<CheckpointState>();
}

std::string BatchLoadFilter::String() {
    std::stringstream ss;
    if (node_rank >= 0) {
        ss << " NodeRank: " << std::to_string(node_rank);
    }
    if (iteration.size() > 0) {
        ss << " Iteration " << iteration;
    }
    if (state >= api::CheckpointState::PENDING && state < api::CheckpointState::STATE_NUM) {
        ss << " State " << state;
    }
    return ss.str();
}

void InterNodeBatchLoadRequest::Marshal(Buffer &buffer) {
    filter.Marshal(buffer);
    buffer.Add(only_metadata);
}

void InterNodeBatchLoadRequest::Unmarshal(Buffer &buffer) {
    filter.Unmarshal(buffer);
    only_metadata = buffer.Get<bool>();
}

std::string InterNodeBatchLoadRequest::String() {
    std::stringstream ss;
    ss << "Filter " << filter.String()
       << " onlyMetadata " << only_metadata;
    return ss.str();
}

void InterNodeBatchLoadResponse::Marshal(Buffer &buffer) {
    buffer.Add(responses.size());
    for (auto i = 0; i < responses.size(); i++) {
        responses[i].metadata.Marshal(buffer);
        responses[i].data_entry.Marshal(buffer);
    }
    buffer.Add(code);
}

void InterNodeBatchLoadResponse::Unmarshal(Buffer &buffer) {
    auto size = buffer.Get<size_t>();
    for (auto i = 0; i < size; i++) {
        InterNodeLoadResponse rsp;
        rsp.metadata.Unmarshal(buffer);
        rsp.data_entry.Unmarshal(buffer);
        responses.push_back(rsp);
    }
    code = buffer.Get<int>();
}

std::string InterNodeBatchLoadResponse::String() {
    std::stringstream ss;
    ss << "Size " << responses.size() << ";";
    for (auto i = 0; i < responses.size(); i++) {
        ss << "No." << i << ": " << responses[i].String() << "\n ";
    }
    ss << "Code " << code;
    return ss.str();
}

void InterNodeNotifyBackupResponse::Marshal(Buffer &buffer) {
    buffer.Add(code);
}

void InterNodeNotifyBackupResponse::Unmarshal(Buffer &buffer) {
    code = buffer.Get<int>();
}

std::string InterNodeNotifyBackupResponse::String() {
    return "Code " + std::to_string(code);
}
} // namespace api
