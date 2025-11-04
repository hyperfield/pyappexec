#include "Logger.hpp"

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

} // namespace Logger
