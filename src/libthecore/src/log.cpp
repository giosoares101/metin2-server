#define __LIBTHECORE__
#include "stdafx.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

int log_init()
{

    // Replace the default logger with a placeholder in order to avoid a name clash
    spdlog::set_default_logger(spdlog::stderr_logger_mt("placeholder_name"));

    // Create the new logger
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_st>("log/daily", 23, 59));
    auto combined_logger = std::make_shared<spdlog::logger>("", begin(sinks), end(sinks));

    //register it if you need to access it globally
    //spdlog::register_logger(combined_logger);

    // Set the new logger as default
    spdlog::set_default_logger(combined_logger);

    // Set flush period and default log level
    spdlog::flush_every(std::chrono::seconds(5));
    spdlog::set_level(spdlog::level::info);

    return 1;
}

void log_destroy()
{
    spdlog::shutdown();
}

void log_set_level(unsigned int level)
{
    spdlog::level::level_enum spdlog_level;

    switch (level) {
        case SPDLOG_LEVEL_TRACE:
            spdlog_level = spdlog::level::trace;
            break;

        case SPDLOG_LEVEL_DEBUG:
            spdlog_level = spdlog::level::debug;
            break;

        case SPDLOG_LEVEL_INFO:
            spdlog_level = spdlog::level::info;
            break;

        case SPDLOG_LEVEL_WARN:
            spdlog_level = spdlog::level::warn;
            break;

        case SPDLOG_LEVEL_ERROR:
            spdlog_level = spdlog::level::err;
            break;

        case SPDLOG_LEVEL_CRITICAL:
            spdlog_level = spdlog::level::critical;
            break;

        case SPDLOG_LEVEL_OFF:
            spdlog_level = spdlog::level::off;
            break;

        default:
            return;
    }

    spdlog::set_level(spdlog_level);
}