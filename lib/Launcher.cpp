#include "Launcher.hpp"

#include "AppBootstrapper.hpp"
#include "AppMetadata.hpp"
#include "gui/GuiRunner.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>

namespace {

#ifdef __APPLE__
void ensureMacDefaultPathsInEnv()
{
    // Prefer the user's shell PATH (login shell) to avoid the stripped PATH Finder provides.
    std::string shellPath;
    if (FILE* pipe = popen("/bin/bash -lc 'echo -n $PATH'", "r")) {
        char buffer[512];
        while (size_t n = fread(buffer, 1, sizeof(buffer), pipe)) {
            shellPath.append(buffer, n);
        }
        pclose(pipe);
    }

    const char* pathEnv = std::getenv("PATH");
    std::string path = pathEnv ? pathEnv : "";
    if (!shellPath.empty()) {
        setenv("PATH", shellPath.c_str(), 1);
        return;
    }

    auto hasPath = [&](const std::string& p) { return path.find(p) != std::string::npos; };
    if (!hasPath("/opt/homebrew/bin")) {
        if (!path.empty() && path.back() != ':') path += ":";
        path += "/opt/homebrew/bin";
    }
    if (!hasPath("/usr/local/bin")) {
        if (!path.empty() && path.back() != ':') path += ":";
        path += "/usr/local/bin";
    }
    setenv("PATH", path.c_str(), 1);
}
#endif

bool shouldLaunchGui(const SpecConfig& specConfig, bool forceCli)
{
    if (forceCli) {
        return false;
    }

    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string guiValue = specConfig.get_value(mainSection, "GUI", false);
    return AppBootstrapper::parseBool(guiValue, false);
}

void writeGuiPreference(const std::filesystem::path& file, bool suppress)
{
    if (file.empty()) {
        return;
    }

    if (suppress) {
        if (!file.parent_path().empty()) {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
        }
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

} // namespace

bool Launcher::ensurePythonReady(AppBootstrapper& appBootstrapper) const
{
    PythonSetupStatus::Status pythonStatus = appBootstrapper.getPythonSetupStatus();

    if (pythonStatus == PythonSetupStatus::Status::SUCCESS) {
        return true;
    }

    if (!appBootstrapper.tryInstallPythonFromCommonPackageManagers()) {
        return false;
    }

    if (appBootstrapper.getPythonSetupStatus() != PythonSetupStatus::Status::SUCCESS) {
        spdlog::error("Python installation/update did not succeed.");
        return false;
    }

    return true;
}

int Launcher::runCli(AppBootstrapper& appBootstrapper, bool detachAfterLaunch) const
{
    if (!ensurePythonReady(appBootstrapper)) {
        return 1;
    }

    const std::array<std::function<bool()>, 5> steps = {
        [&] { return appBootstrapper.downloadRequirements(); },
        [&] { return appBootstrapper.installRequirements(); },
        [&] { return appBootstrapper.setupVirtualEnv(); },
        [&] { return appBootstrapper.installPythonDependencies(); },
        [&] { return appBootstrapper.launchPythonApp(detachAfterLaunch); }
    };

    for (const auto& step : steps) {
        if (!step()) {
            return 1;
        }
    }

    return 0;
}

int Launcher::run(const CliOptions& options,
                  SpecConfig& specConfig,
                  const ConfigLoader& loader,
                  int argc,
                  char** argv) const
{
#ifdef __APPLE__
    ensureMacDefaultPathsInEnv();
#endif
    AppBootstrapper appBootstrapper(specConfig);

    std::string appDisplayName = loader.determineAppDisplayName(specConfig);
    std::filesystem::path guiPreferenceFile = loader.guiPreferenceFile(specConfig);
    if (options.resetGuiPreference) {
        writeGuiPreference(guiPreferenceFile, false);
    }

    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string hidePref = specConfig.get_value(mainSection, "GUI_CLI_HIDE_AFTER_SUCCESS", false);
    if (hidePref.empty()) {
        hidePref = specConfig.get_value(mainSection, "GUI_HIDE_AFTER_SUCCESS", false); // backward compatibility
    }
    bool autoSuppressAfterSuccess = AppBootstrapper::parseBool(hidePref, false);

    bool launchGui = shouldLaunchGui(specConfig, options.forceCli);
    if (launchGui && loadGuiPreference(guiPreferenceFile) && !appBootstrapper.requiresProvisioning()) {
        launchGui = false;
    }
    if (launchGui) {
        return runGuiApplication(argc, argv, options.forwardedArgs, guiPreferenceFile.string(), appDisplayName, autoSuppressAfterSuccess);
    }

    const bool detachAfterLaunch = options.forceCli && autoSuppressAfterSuccess;
    int result = runCli(appBootstrapper, detachAfterLaunch);
    if (result != 0) {
        writeGuiPreference(guiPreferenceFile, false);
    } else {
        std::cout << "Bootstrapped by PyAppExec "
                  << AppMetadata::kVersion << " ("
                  << AppMetadata::kYears << "). Project: "
                  << AppMetadata::kGithub << std::endl;
        if (autoSuppressAfterSuccess) {
            writeGuiPreference(guiPreferenceFile, true);
        }
    }
    return result;
}
