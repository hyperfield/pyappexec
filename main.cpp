#include "AppBootstrapper.hpp"
#include "AppMetadata.hpp"
#include "Logger.hpp"
#include "SpecConfig.hpp"
#include "gui/GuiRunner.hpp"
// cppcheck-suppress missingIncludeSystem
#include <gio/gio.h>
#include <spdlog/spdlog.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

int runCli(SpecConfig& specConfig)
{
    AppBootstrapper appBootstrapper(specConfig);

    PythonSetupStatus::Status pythonStatus = appBootstrapper.getPythonSetupStatus();

    if (pythonStatus != PythonSetupStatus::Status::SUCCESS) {
        if (!appBootstrapper.tryInstallPythonFromCommonPackageManagers()) {
            return 1;
        }

        if (appBootstrapper.getPythonSetupStatus() != PythonSetupStatus::Status::SUCCESS) {
            spdlog::error("Python installation/update did not succeed.");
            return 1;
        }
    }

    if (!appBootstrapper.downloadRequirements()) {
        return 1;
    }

    if (!appBootstrapper.installRequirements()) {
        return 1;
    }

    if (!appBootstrapper.setupVirtualEnv()) {
        return 1;
    }

    if (!appBootstrapper.installPythonDependencies()) {
        return 1;
    }

    if (!appBootstrapper.launchPythonApp()) {
        return 1;
    }

    return 0;
}

bool shouldLaunchGui(const SpecConfig& specConfig, bool forceCli)
{
    if (forceCli) {
        return false;
    }

    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string guiValue = specConfig.get_value(mainSection, "GUI", false);
    return AppBootstrapper::parseBool(guiValue, false);
}

std::filesystem::path resolveConfigPath(const std::optional<std::string>& overridePath)
{
    namespace fs = std::filesystem;

    if (overridePath) {
        fs::path explicitPath = fs::path(*overridePath);
        if (!explicitPath.is_absolute()) {
            explicitPath = fs::current_path() / explicitPath;
        }
        if (!fs::exists(explicitPath)) {
            throw std::runtime_error("Specified config file not found: " + explicitPath.string());
        }
        return explicitPath;
    }

    fs::path cwd = fs::current_path();
    fs::path primary = cwd / "pyappexec.ini";
    if (fs::exists(primary)) {
        return primary;
    }

    for (const auto& entry : fs::directory_iterator(cwd)) {
        if (!entry.is_directory()) {
            continue;
        }
        fs::path candidate = entry.path() / "pyappexec.ini";
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error(
        "Unable to locate pyappexec.ini in the current directory or its immediate subdirectories.\n"
        "Pass --config /path/to/pyappexec.ini to specify the configuration explicitly.");
}


void writeGuiPreference(const std::filesystem::path& file, bool suppress)
{
    if (file.empty()) {
        return;
    }

    if (suppress) {
        std::ofstream out(file, std::ios::trunc);
        if (out) {
            out << "suppress_gui=1\n";
        }
    } else {
        std::error_code ec;
        std::filesystem::remove(file, ec);
    }
}


bool loadGuiPreference(const std::filesystem::path& file)
{
    std::ifstream in(file);
    if (!in) {
        return false;
    }

    std::string content;
    std::getline(in, content);
    for (char& ch : content) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    return content.find('1') != std::string::npos || content.find("true") != std::string::npos;
}


std::string determineAppDisplayName(const SpecConfig& specConfig, const std::filesystem::path& configPath)
{
    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string appDirValue = specConfig.get_value(mainSection, "python_app_dir", false);
    std::filesystem::path baseDir = configPath.parent_path();

    auto fallbackName = [&]() {
        std::string name = baseDir.filename().string();
        if (name.empty()) {
            name = baseDir.parent_path().filename().string();
        }
        if (name.empty()) {
            name = "PyAppExec";
        }
        return name;
    };

    if (!appDirValue.empty()) {
        std::filesystem::path resolved(appDirValue);
        if (resolved.is_relative()) {
            resolved = baseDir / resolved;
        }
        std::string candidate = resolved.filename().string();
        if (!candidate.empty() && candidate != "." && candidate != "..") {
            return candidate;
        }
    }

    return fallbackName();
}


void printHelp(const char* programName)
{
    std::cout << "PyAppExec - Cross-platform Python bootstrapper\n"
              << "Usage: " << programName << " [options]\n\n"
              << "Options:\n"
              << "  --config <path>     Use a specific pyappexec.ini\n"
              << "  --no-gui            Force CLI mode even if GUI is enabled in the INI\n"
              << "  --reset-gui         Clear the saved \"hide GUI\" preference for this app\n"
              << "  --help              Show this message and exit\n\n"
              << "PyAppExec prepares Python interpreters, virtual environments, and external tools\n"
                 "so end users can run your packaged application without manual setup.\n"
                 "Project: https://github.com/quicknode-labs/PyAppExec\n";
}

}


int main(int argc, char** argv)
{
    bool forceCli = false;
    std::vector<std::string> forwardedArgs;
    forwardedArgs.reserve(argc > 1 ? argc - 1 : 0);
    std::optional<std::string> configOverride;
    bool resetGuiPreference = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--no-gui") {
            forceCli = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        }
        if (arg == "--reset-gui") {
            resetGuiPreference = true;
            continue;
        }
        if (arg.rfind("--config", 0) == 0) {
            std::string value;
            if (arg == "--config") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--config requires a path argument.");
                }
                value = argv[++i];
            } else {
                auto pos = arg.find('=');
                if (pos == std::string_view::npos || pos + 1 == arg.size()) {
                    throw std::runtime_error("Use --config /path/to/pyappexec.ini or --config=/path/to/pyappexec.ini");
                }
                value = std::string(arg.substr(pos + 1));
            }
            configOverride = value;
            continue;
        }
        forwardedArgs.emplace_back(argv[i]);
    }

    Logger::initialize();

    try {
        std::filesystem::path configPath = resolveConfigPath(configOverride);
        spdlog::info("Using configuration file: {}", configPath.string());
        SpecConfig specConfig(configPath.string());
        std::string appDisplayName = determineAppDisplayName(specConfig, configPath);
        std::filesystem::path guiPreferenceFile = configPath.parent_path() / ".pyappexec_gui_pref";
        if (resetGuiPreference) {
            writeGuiPreference(guiPreferenceFile, false);
        }

        std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
        bool logConsole = AppBootstrapper::parseBool(
            specConfig.get_value(mainSection, "log_console", false), true);
        auto level = Logger::levelFromString(
            specConfig.get_value(mainSection, "log_level", false));
        Logger::configure(logConsole, level);

        bool launchGui = shouldLaunchGui(specConfig, forceCli);
        if (launchGui && loadGuiPreference(guiPreferenceFile)) {
            launchGui = false;
        }
        if (launchGui) {
            return runGuiApplication(argc, argv, forwardedArgs, guiPreferenceFile.string(), appDisplayName);
        }

        int result = runCli(specConfig);
        if (result != 0) {
            writeGuiPreference(guiPreferenceFile, false);
        } else {
            spdlog::info("Bootstrapped by PyAppExec {} ({}). Project: {}",
                         AppMetadata::kVersion,
                         AppMetadata::kYears,
                         AppMetadata::kGithub);
        }
        return result;

    } catch (const std::runtime_error& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
