/**
 * @file world.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-07-10
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "util/util.h"

namespace config {
using util::Util;

/**
 * @brief training job world state, including hostnames, rank, world size, etc.
 */
class WorldState {
private:
    WorldState() {
        hostname_ = Util::GetEnv("HOSTNAME");
        job_name_ = Util::GetEnv(ENV_KEY_TRANSOM_JOB_KEY, DEFAULT_TRANSOM_JOB_KEY);
        node_rank_ = std::atoi(Util::GetEnv(ENV_KEY_TRANSOM_RANK, DEFAULT_TRANSOM_RANK).c_str());
        world_size_ = std::atoi(Util::GetEnv(ENV_KEY_TRANSOM_WORLD_SIZE, DEFAULT_TRANSOM_WORLD_SIZE).c_str());
        hosts_ = Util::Split(Util::GetEnv(ENV_KEY_TRANSOM_HOSTS, hostname_.c_str()).c_str(), ',');
    }

public:
    WorldState(const WorldState &) = delete;
    WorldState(WorldState &&) = delete;
    WorldState &operator=(const WorldState &) = delete;
    WorldState &operator=(WorldState &&) = delete;

    /**
     * @brief return single instance
     * @return reference of instance, cannot be copied or deleted
     */
    static WorldState &Instance() {
        static std::unique_ptr<WorldState> instance_ptr_(new WorldState());
        return *instance_ptr_;
    }

    /**
     * @brief return hostname
     * @return string
     */
    std::string Hostname() {
        return hostname_;
    }

    /**
     * @brief return job name(actually job key)
     * @return string
     */
    std::string JobName() {
        return job_name_;
    }

    /**
     * @brief return node rank
     */
    int NodeRank() {
        return node_rank_;
    }

    /**
     * @brief return world size, aka number of nodes
     */
    int WorldSize() {
        return world_size_;
    }

    /**
     * @brief return transom hosts list
     * @return vector, each element is hostname of a node
     */
    std::vector<std::string> Hosts() {
        return hosts_;
    }

private:
    std::string hostname_;
    std::string job_name_;
    std::vector<std::string> hosts_;
    int world_size_;
    int node_rank_;
};
} // namespace config