/**
 * @file nic_helper.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-09-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "util/nic_helper.h"

#include "infiniband/verbs.h"

#include "logger/logger.h"

namespace util {
MultiNicHelper::MultiNicHelper() {
    LOG_INFO("searching for IB devices in host");
    /* get device names in the system */
    int num_devices = 0;
    auto dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        LOG_FATAL("failed to get IB devices list");
    }
    /* if there isn't any IB device in host */
    if (num_devices <= 0) {
        LOG_ERROR("cannot find devices");
        return;
    }

    for (int i = 0; i < num_devices; i++) {
        auto name = std::string(ibv_get_device_name(dev_list[i]));

        /* open device */
        auto ib_ctx = ibv_open_device(dev_list[i]);
        if (!ib_ctx) {
            LOG_FATAL("ibv_open_device: {}", strerror(errno));
        }

        /* query device attribute */
        ibv_device_attr device_attr;
        if (ibv_query_device(ib_ctx, &device_attr)) {
            LOG_FATAL("ibv_query_device: {}", strerror(errno));
        }

        /* get each port info */
        for (int port_num = 1; port_num <= device_attr.phys_port_cnt; ++port_num) {
            struct ibv_port_attr port_attr;
            if (ibv_query_port(ib_ctx, port_num, &port_attr)) {
                LOG_FATAL("ibv_query_port: {}", strerror(errno));
            }

            if (port_attr.state != IBV_PORT_ACTIVE) {
                LOG_WARN("device {} inactive, skip...", name);
                continue;
            }
            if (port_attr.link_layer != 1) {
                LOG_WARN("device {} link layer not infiniband, skip...", name);
                continue;
            }

            nics_.push_back(name);
            busy_[name] = 0;
        }

        /* close context */
        ibv_close_device(ib_ctx);
    }
    auto names = [this]() -> std::string {
        std::string all_dev_name;
        for (auto &ele : nics_) {
            all_dev_name += std::string(ele) + " ";
        }
        return all_dev_name;
    }();
    LOG_INFO("found active IB devices: {}", names);

    ibv_free_device_list(dev_list);
}

std::string MultiNicHelper::ChooseNic() {
    mu_.lock();

    std::string res;
    uint min = 1024; /* should be enough */
    for (auto iter = busy_.begin(); iter != busy_.end(); iter++) {
        if (iter->second == 0) {
            res = iter->first;
            break;
        }
        if (min > iter->second) {
            res = iter->first;
            min = iter->second;
        }
    }

    busy_[res] += 1;

    mu_.unlock();
    return res;
}

void MultiNicHelper::ReleaseNic(std::string name) {
    mu_.lock();
    busy_[name] -= 1;
    mu_.unlock();
}
} // namespace util
