#include "Logger.hpp"

#include <algorithm>
#include <unordered_map>
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
    static const std::unordered_map<std::string, spdlog::level::level_enum> kLevelMap = {
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"warning", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"fatal", spdlog::level::critical},
        {"off", spdlog::level::off},
        {"none", spdlog::level::off}
    };

    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    auto it = kLevelMap.find(lowered);
    return it != kLevelMap.end() ? it->second : spdlog::level::info;
}

} // namespace Logger
