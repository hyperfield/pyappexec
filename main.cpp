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

        if (pythonStatus == PythonSetupStatus::Status::NOT_FOUND ||
            pythonStatus == PythonSetupStatus::Status::UPDATE_REQUIRED)
        {
            if (!appBootstrapper.downloadRequirements()) {
                return 1;
            }

            if (!appBootstrapper.installRequirements()) {
                return 1;
            }

            std::cout << "Python installed successfully. Checking version again..." << std::endl;
            std::cout << std::endl;

            if (appBootstrapper.getPythonSetupStatus() != PythonSetupStatus::Status::SUCCESS) {
                std::cerr << "Python installation was not successful." << std::endl;
                return 1;
            }
        }

        if (!appBootstrapper.downloadRequirements()) {
            return 1;
        }

        if (!appBootstrapper.installRequirements()) {
            return 1;
        }

    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}