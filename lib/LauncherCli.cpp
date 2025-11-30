#include "LauncherCli.hpp"

#include "AppBootstrapper.hpp"
#include "AppMetadata.hpp"

#include <array>
#include <spdlog/spdlog.h>

namespace {

void ensureMacDefaultPathsInEnv()
{
#ifdef __APPLE__
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
#endif
}

} // namespace

bool LauncherCli::ensurePythonReady(AppBootstrapper& appBootstrapper) const
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

int LauncherCli::run(SpecConfig& specConfig, const ConfigLoader& loader) const
{
#ifdef __APPLE__
    ensureMacDefaultPathsInEnv();
#endif
    AppBootstrapper appBootstrapper(specConfig);
    const std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string hidePref = specConfig.get_value(mainSection, "GUI_CLI_HIDE_AFTER_SUCCESS", false);
    if (hidePref.empty()) {
        hidePref = specConfig.get_value(mainSection, "GUI_HIDE_AFTER_SUCCESS", false); // backward compatibility
    }
    const bool detachAfterLaunch = AppBootstrapper::parseBool(hidePref, false);

    if (!ensurePythonReady(appBootstrapper)) {
        return 1;
    }

    const std::array<std::function<bool()>, 4> steps = {
        [&] { return appBootstrapper.downloadRequirements(); },
        [&] { return appBootstrapper.installRequirements(); },
        [&] { return appBootstrapper.setupVirtualEnv(); },
        [&] { return appBootstrapper.installPythonDependencies(); }
    };

    for (const auto& step : steps) {
        if (!step()) {
            return 1;
        }
    }

    return appBootstrapper.launchPythonApp(detachAfterLaunch) ? 0 : 1;
}
