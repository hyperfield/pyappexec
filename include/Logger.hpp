#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/common.h>

namespace Logger {

void initialize();
void configure(bool enableConsole, spdlog::level::level_enum level);
spdlog::level::level_enum levelFromString(const std::string& value);

}

#endif
