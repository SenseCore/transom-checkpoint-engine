/**
 * @file operator_test.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <iostream>

#include "coordinator/coordinator.h"
#include "operator/operator.h"

using operators::Operator;

int main(int argc, char **argv) {
    logger::Logger::InitLogger();
    auto op = Operator();
    auto handler = [](std::string key) -> bool {
        LOG_INFO("handle key {}", key);
        return true;
    };
    op.SetHandler(handler);

    std::thread([](Operator &op) {
        for (auto i = 0;; i++) {
            auto key = "a" + std::to_string(i);
            LOG_INFO("enqueue key {}", key);
            op.AddRateLimited(key);
        }
    },
                std::ref(op))
        .detach();

    op.Run();
    return 0;
}
