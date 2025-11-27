#include "LoggerConfigurator.hpp"

#include "AppBootstrapper.hpp"
#include "Logger.hpp"

#include <cstdlib>
#include <filesystem>

namespace {

std::filesystem::path userLogDirectory()
{
#ifdef _WIN32
    const char* localAppData = std::getenv("LOCALAPPDATA");
    const char* appData = std::getenv("APPDATA");
    if (localAppData && *localAppData) {
        return std::filesystem::path(localAppData) / "PyAppExec" / "logs";
    }
    if (appData && *appData) {
        return std::filesystem::path(appData) / "PyAppExec" / "logs";
    }
    return std::filesystem::path("logs");
#elif __APPLE__
    const char* home = std::getenv("HOME");
    std::filesystem::path base = (home && *home) ? std::filesystem::path(home) : std::filesystem::current_path();
    // Use the conventional macOS logs directory under the user Library.
    return base / "Library" / "Logs" / "PyAppExec";
#else
    const char* xdgState = std::getenv("XDG_STATE_HOME");
    if (xdgState && *xdgState) {
        return std::filesystem::path(xdgState) / "pyappexec";
    }
    const char* home = std::getenv("HOME");
    std::filesystem::path base = (home && *home) ? std::filesystem::path(home) / ".local" / "state"
                                                 : std::filesystem::current_path();
    return base / "pyappexec";
#endif
}

} // namespace

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

    const std::filesystem::path logFile = userLogDirectory() / "pyappexec.log";
    Logger::configure(logConsole, level, logFile.string());
}
