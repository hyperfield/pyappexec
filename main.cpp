#include "AppBootstrapper.hpp"
#include "SpecConfig.hpp"
#include "gui/GuiRunner.hpp"
#include "Logger.hpp"
#include <gio/gio.h>
#include <spdlog/spdlog.h>
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

}


int main(int argc, char** argv)
{
    bool forceCli = false;
    std::vector<std::string> forwardedArgs;
    forwardedArgs.reserve(argc > 1 ? argc - 1 : 0);

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--no-gui") {
            forceCli = true;
            continue;
        }
        forwardedArgs.emplace_back(argv[i]);
    }

    Logger::initialize();

    try {
        SpecConfig specConfig("pyappexec.ini");

        if (shouldLaunchGui(specConfig, forceCli)) {
            return runGuiApplication(argc, argv, forwardedArgs);
        }

        return runCli(specConfig);

    } catch (const std::runtime_error& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
