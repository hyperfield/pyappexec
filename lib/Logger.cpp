#include "Logger.hpp"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/null_sink.h>
// cppcheck-suppress missingIncludeSystem
#include <spdlog/sinks/rotating_file_sink.h>
// cppcheck-suppress missingIncludeSystem
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Logger {

void initialize()
{
    if (!spdlog::default_logger()) {
        configure(true, spdlog::level::info, std::string{});
    }
}

void configure(bool enableConsole, spdlog::level::level_enum level, const std::string& filePath)
{
    auto distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();

    if (enableConsole) {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // For GUI/console output we don’t need the log level prefix.
        sink->set_pattern("[%H:%M:%S] %v");
        distSink->add_sink(sink);
    }

    if (!filePath.empty()) {
        std::error_code ec;
        std::filesystem::path logPath(filePath);
        std::filesystem::path dir = logPath.parent_path();
        if (!dir.empty()) {
            std::filesystem::create_directories(dir, ec);
        }

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            filePath,
            5 * 1024 * 1024, // 5 MB per file
            3);              // keep 3 files
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        distSink->add_sink(fileSink);
    }

    if (distSink->sinks().empty()) {
        distSink->add_sink(std::make_shared<spdlog::sinks::null_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>("pyappexec", distSink);
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
