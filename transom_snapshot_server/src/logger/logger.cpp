/**
 * @file logger.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "logger/logger.h"

#include "config/config.h"
#include "util/util.h"

using logger::Logger;

void Logger::InitLogger() {
    auto log_level = std::atoi(util::Util::GetEnv(config::ENV_KEY_LOG_LEVEL,
                                                  std::to_string(config::SPDLOG_DEFAULT_LOG_LEVEL).c_str())
                                   .c_str());
    SetLogLevel(log_level);

    auto log_pattern = util::Util::GetEnv(config::ENV_KEY_LOG_PATTERN, config::SPDLOG_LOG_DEFAULT_PATTERN);
    spdlog::set_pattern(log_pattern);
}

void Logger::SetLogLevel(int level) {
    switch (level) {
    case LOG_LEVEL_TRACE:
        spdlog::set_level(spdlog::level::trace);
        break;
    case LOG_LEVEL_DEBUG:
        spdlog::set_level(spdlog::level::debug);
        break;
    case LOG_LEVEL_INFO:
        spdlog::set_level(spdlog::level::info);
        break;
    case LOG_LEVEL_WARN:
        spdlog::set_level(spdlog::level::warn);
        break;
    case LOG_LEVEL_ERROR:
        spdlog::set_level(spdlog::level::err);
        break;
    case LOG_LEVEL_FATAL:
        spdlog::set_level(spdlog::level::critical);
        break;
    default:
        LOG_FATAL("log level {} unsupported", level);
    }
}
