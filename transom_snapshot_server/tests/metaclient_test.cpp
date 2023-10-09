/**
 * @file metaclient_test.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "monitor/monitor.h"
#include "storage/metadata.h"

int main(int argc, char **argv) {
    setenv(config::ENV_KEY_META_CLIENT, config::META_CLIENT_MYSQL, 1);
    setenv("CKPT_ENGINE_MYSQL_ADDR", "10.198.32.49", 1);
    setenv("CKPT_ENGINE_MYSQL_PORT", "3306", 1);
    setenv("CKPT_ENGINE_MYSQL_USER", "root", 1);
    setenv(config::ENV_KEY_MYSQL_PASSWORD, "12345678", 1);
    setenv(config::ENV_KEY_MYSQL_FLUSH_TABLE, "true", 1);
    auto client = storage::MetadataClientFactory::GetClient();
    int rc = -1;
    api::Metadata metadata1("test", "test1", 0, "iter0", api::BROKEN);
    api::Metadata metadata2("test", "test2", 1, "iter0", api::PENDING);

    if (!client->Save(std::ref(metadata1))) {
        return 1;
    }
    if (!client->Save(std::ref(metadata1))) {
        return 1;
    }
    if (!client->Save(std::ref(metadata2))) {
        return 1;
    }

    api::Metadata query("test", "test1");
    rc = client->Load(std::ref(query));
    if (!api::IsSuccess(rc)) {
        return 1;
    }
    LOG_INFO("loaded metadata: {}", query.String());

    api::BatchLoadFilter filter1(0);
    std::vector<api::Metadata> vec1;
    rc = client->BatchLoad(std::ref(filter1), std::ref(vec1));
    if (!api::IsSuccess(rc)) {
        return 1;
    }
    LOG_INFO("size {}", vec1.size());
    for (auto &item : vec1) {
        LOG_INFO(item.String());
    }

    api::BatchLoadFilter filter2(-1, "iter0");
    std::vector<api::Metadata> vec2;
    rc = client->BatchLoad(std::ref(filter2), std::ref(vec2));
    if (!api::IsSuccess(rc)) {
        return 1;
    }
    LOG_INFO("size {}", vec2.size());
    for (auto &item : vec2) {
        LOG_INFO(item.String());
    }

    /* test update state */
    metadata1.state = api::PERSISTENT;
    rc = client->UpdateState(metadata1.file_name, metadata1.state);
    if (!api::IsSuccess(rc)) {
        return 1;
    }

    /* test memory monitor */
    monitor::MemoryMonitor::Instance().Start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto res = monitor::MemoryMonitor::Instance().GetMemoryStat();
    LOG_INFO("usage {} idle {}", res.total_usage, res.total_idle);

    return 0;
}
