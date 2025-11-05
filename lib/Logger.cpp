#include "Logger.hpp"

#include <algorithm>
#include <spdlog/sinks/null_sink.h>
// cppcheck-suppress missingIncludeSystem
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Logger {

void initialize()
{
    if (spdlog::default_logger()) {
        return;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%H:%M:%S] [%^%l%$] %v");

    auto logger = std::make_shared<spdlog::logger>("pyappexec", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}

void configure(bool enableConsole, spdlog::level::level_enum level)
{
    std::shared_ptr<spdlog::logger> logger;

    if (enableConsole) {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        logger = std::make_shared<spdlog::logger>("pyappexec", sink);
    } else {
        auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        logger = std::make_shared<spdlog::logger>("pyappexec", sink);
    }

    spdlog::set_default_logger(logger);
    spdlog::set_level(level);
    spdlog::flush_on(level);
}

spdlog::level::level_enum levelFromString(const std::string& value)
{
    if (value.empty()) {
        return spdlog::level::info;
    }

    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lowered == "trace") return spdlog::level::trace;
    if (lowered == "debug") return spdlog::level::debug;
    if (lowered == "info") return spdlog::level::info;
    if (lowered == "warn" || lowered == "warning") return spdlog::level::warn;
    if (lowered == "error") return spdlog::level::err;
    if (lowered == "critical" || lowered == "fatal") return spdlog::level::critical;
    if (lowered == "off" || lowered == "none") return spdlog::level::off;

    return spdlog::level::info;
}

} // namespace Logger
