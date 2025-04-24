#include "PythonManager.hpp"
#include "Utils.hpp"
#include <iostream>
#include <regex>


std::string PythonManager::getPythonVersion()
{
    try {
        return Utils::runPythonScriptFromResourceAndCaptureOutput("get_python_version.py", "python3");
    } catch (const std::runtime_error& e) {
        std::cerr << "[Fallback] python3 failed: " << e.what() << "\n";
        return Utils::runPythonScriptFromResourceAndCaptureOutput("get_python_version.py", "python");
    }
}


bool PythonManager::isPythonVersionAtLeast(const std::string& installed, const std::string& required)
{
    std::istringstream installedStream(installed);
    std::istringstream requiredStream(required);

    int installedMajor = 0, installedMinor = 0, installedPatch = 0;
    int requiredMajor = 0, requiredMinor = 0, requiredPatch = 0;

    char dot;

    installedStream >> installedMajor >> dot >> installedMinor;
    if (!(installedStream >> installedPatch)) installedPatch = 0;

    requiredStream >> requiredMajor >> dot >> requiredMinor;
    if (!(requiredStream >> dot >> requiredPatch)) requiredPatch = 0;

    if (installedMajor > requiredMajor) return true;
    if (installedMajor < requiredMajor) return false;
    
    if (installedMinor > requiredMinor) return true;
    if (installedMinor < requiredMinor) return false;

    return installedPatch >= requiredPatch;
}


bool PythonManager::isPythonInstalledAndMeetsVersion(const std::string& requiredVersion) {
    std::string installedVersion = getPythonVersion();
    if (installedVersion.empty()) {
        std::cerr << "Python is NOT installed!" << std::endl;
        return false;
    }

    installedVersion.erase(installedVersion.find_last_not_of(" \n\r\t") + 1);
    if (isPythonVersionAtLeast(installedVersion, requiredVersion)) {
        std::cout << "Python " << installedVersion
                  << " is installed and meets the minimum requirement (" 
                  << requiredVersion << ")." << std::endl;
        return true;
    } else {
        std::cerr << "Python version is too old! Installed: " << installedVersion << ", required: >= " << requiredVersion << std::endl;
        return false;
    }
}