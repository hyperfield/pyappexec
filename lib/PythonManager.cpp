#include "PythonManager.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>


std::string PythonManager::getPythonVersion()
{
    if (cachedPythonVersion.empty()) {
        resolvePythonCommand();
    }

    return cachedPythonVersion;
}


std::string PythonManager::getResolvedPythonCommand()
{
    if (cachedPythonCommand.empty()) {
        resolvePythonCommand();
    }

    return cachedPythonCommand;
}


bool PythonManager::isPythonInstalled()
{
    try {
        return !getPythonVersion().empty();
    } catch (const std::exception&) {
        return false;
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


std::string PythonManager::resolvePythonCommand()
{
    if (!cachedPythonCommand.empty() && !cachedPythonVersion.empty()) {
        return cachedPythonCommand;
    }

    std::vector<std::string> candidates;
#ifdef _WIN32
    candidates = {"py", "python3", "python"};
#else
    candidates = {"python3", "python"};
#endif

    for (const auto& candidate : candidates) {
        try {
            std::string output = Utils::runPythonScriptFromResourceAndCaptureOutput("get_python_version.py", candidate);
            std::istringstream stream(output);
            std::string versionLine;
            std::getline(stream, versionLine);
            std::string executableLine;
            std::getline(stream, executableLine);

            cachedPythonVersion = trimVersionString(versionLine);
            std::string resolvedPath = trimVersionString(executableLine);
            cachedPythonCommand = resolvedPath.empty() ? candidate : resolvedPath;

            if (cachedPythonCommand.empty()) {
                cachedPythonCommand = candidate;
            }

            return cachedPythonCommand;
        } catch (const std::runtime_error& err) {
            std::cerr << "[Python detection] " << candidate << " failed: " << err.what() << std::endl;
        }
    }

    throw std::runtime_error("Unable to locate a working Python interpreter on PATH.");
}


std::string PythonManager::trimVersionString(const std::string& rawVersion)
{
    std::string trimmed = rawVersion;
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), notSpace));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), notSpace).base(), trimmed.end());

    return trimmed;
}
