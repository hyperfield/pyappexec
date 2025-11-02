#include "AppBootstrapper.hpp"
#include "PythonManager.hpp"
#include "SpecConfig.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <gio/gio.h>


int main()
{
    try {
        SpecConfig specConfig("pyappexec.ini");
        AppBootstrapper appBootstrapper(specConfig);

        PythonSetupStatus::Status pythonStatus = appBootstrapper.getPythonSetupStatus();

        if (pythonStatus != PythonSetupStatus::Status::SUCCESS) {
            if (!appBootstrapper.tryInstallPythonFromCommonPackageManagers()) {
                return 1;
            }

            if (appBootstrapper.getPythonSetupStatus() != PythonSetupStatus::Status::SUCCESS) {
                std::cerr << "Python installation/update did not succeed." << std::endl;
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

    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
