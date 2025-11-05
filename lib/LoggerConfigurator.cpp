#include "LoggerConfigurator.hpp"

#include "AppBootstrapper.hpp"
#include "Logger.hpp"

LoggerConfigurator::LoggerConfigurator(const SpecConfig& specConfig)
    : specConfig_(specConfig)
{}

void LoggerConfigurator::apply() const
{
    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    bool logConsole = AppBootstrapper::parseBool(
        specConfig_.get_value(mainSection, "log_console", false), true);
    auto level = Logger::levelFromString(
        specConfig_.get_value(mainSection, "log_level", false));
    Logger::configure(logConsole, level);
}
