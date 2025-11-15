#include "PythonManager.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <spdlog/spdlog.h>


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
        spdlog::warn("Python is NOT installed!");
        return false;
    }

    installedVersion.erase(installedVersion.find_last_not_of(" \n\r\t") + 1);
    if (isPythonVersionAtLeast(installedVersion, requiredVersion)) {
        spdlog::info("Python {} is installed and meets the minimum requirement ({})",
                     installedVersion, requiredVersion);
        return true;
    } else {
        spdlog::error("Python version is too old! Installed: {}, required: >= {}",
                      installedVersion, requiredVersion);
        return false;
    }
}


std::string PythonManager::resolvePythonCommand()
{
    if (!cachedPythonCommand.empty() && !cachedPythonVersion.empty()) {
        return cachedPythonCommand;
    }

    std::vector<std::string> candidates;
    std::unordered_set<std::string> seen;
    const auto addCandidate = [&](std::string path) {
        if (path.empty()) {
            return;
        }
        std::replace(path.begin(), path.end(), '\\', '/');
        if (seen.insert(path).second) {
            candidates.emplace_back(std::move(path));
        }
    };

    if (const char* overrideCmd = std::getenv("PYAPPEXEC_PYTHON")) {
        if (overrideCmd[0] != '\0') {
            addCandidate(overrideCmd);
        }
    }

#if defined(__APPLE__)
    const std::vector<std::string> macPaths = {
        "/opt/homebrew/bin/python3",
        "/usr/local/bin/python3",
        "/Library/Frameworks/Python.framework/Versions/Current/bin/python3"
    };
    for (const auto& p : macPaths) {
        addCandidate(p);
    }
#endif

#ifdef _WIN32
    const std::vector<std::string> defaultCandidates = {"py", "python3", "python"};
#else
    const std::vector<std::string> defaultCandidates = {"python3", "python"};
#endif
    for (const auto& p : defaultCandidates) {
        addCandidate(p);
    }

#ifdef _WIN32
    namespace fs = std::filesystem;

    const auto scanPythonDirPath = [&addCandidate](const fs::path& rootPath) {
        std::error_code ec;
        if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) {
            return;
        }
        for (const auto& entry : fs::directory_iterator(rootPath, ec)) {
            if (!entry.is_directory(ec)) {
                continue;
            }
            const auto name = entry.path().filename().string();
            if (name.rfind("Python", 0) == 0) { // starts with "Python"
                addCandidate((entry.path() / "python.exe").string());
                addCandidate((entry.path() / "python3.exe").string());
                addCandidate((entry.path() / "python3.13.exe").string());
            }
        }
    };

    const auto scanPythonDirsFromEnv = [&scanPythonDirPath](const char* rootEnv) {
        if (!rootEnv || std::strlen(rootEnv) == 0) {
            return;
        }
        scanPythonDirPath(fs::path(rootEnv));
    };
#endif

#ifdef _WIN32
    auto scanLocalBase = [&](const fs::path& base) {
        if (base.empty()) {
            return;
        }
        // Microsoft Store shims live here when installed from the Store.
        addCandidate((base / "Microsoft/WindowsApps/python3.13.exe").string());
        addCandidate((base / "Microsoft/WindowsApps/python3.exe").string());
        addCandidate((base / "Microsoft/WindowsApps/python.exe").string());
        addCandidate((base / "Microsoft/WindowsApps/py.exe").string());
        // Traditional installer under the user's profile.
        scanPythonDirPath(base / "Programs/Python");
        // Some Store layouts place the real python.exe under a package folder.
        scanPythonDirPath(base / "Microsoft/WindowsApps");
    };

    fs::path localBase;
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        localBase = fs::path(localAppData);
    } else if (const char* userProfile = std::getenv("USERPROFILE")) {
        localBase = fs::path(userProfile) / "AppData/Local";
    }
    scanLocalBase(localBase);

    scanPythonDirsFromEnv(std::getenv("ProgramFiles"));
    scanPythonDirsFromEnv(std::getenv("ProgramFiles(x86)"));
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
            spdlog::debug("Python detection candidate '{}' failed: {}", candidate, err.what());
        }
    }

    std::ostringstream candidatesList;
    for (size_t i = 0; i < candidates.size(); ++i) {
        candidatesList << (i == 0 ? "" : ", ") << candidates[i];
    }
    spdlog::error("Unable to locate a working Python interpreter. Tried: {}", candidatesList.str());
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
