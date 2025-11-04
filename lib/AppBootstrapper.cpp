#include "AppBootstrapper.hpp"
#include "SpecConfig.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <boost/process.hpp>
#include <cstdlib>
#include <system_error>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
namespace bp = boost::process;

namespace {
std::string trimCopy(const std::string& input) {
    std::string trimmed = input;
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), notSpace));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), notSpace).base(), trimmed.end());

    return trimmed;
}
}


AppBootstrapper::AppBootstrapper(const SpecConfig& specConfig) :
    specConfig(specConfig)
{
    parseConfig();
    std::string python_ver = python.getPythonVersion();
    if (python_ver.empty()) {
        std::cout << "Python not found." << std::endl;
    }
}


void AppBootstrapper::parseConfig()
{
    std::string osPrefix = getOSPrefix();
    std::string mainSection = osPrefix + ":main";
    std::string reqSection = osPrefix + ":requirements";

    python_download_url = specConfig.get_value(mainSection, "python_download_url", false);
    python_min_ver = specConfig.get_value(mainSection, "python_min_ver", true);
    std::string execPathValue = specConfig.get_value(mainSection, "exec_app_path", true);
    std::string execArgsValue = specConfig.get_value(mainSection, "exec_app_args", false);
    std::string execEnvValue = specConfig.get_value(mainSection, "exec_env", false);
    std::string requirementsValue = specConfig.get_value(mainSection, "requirements_file", false);
    std::string pythonAppDirValue = specConfig.get_value(mainSection, "python_app_dir", false);
    std::string virtualEnvValue = specConfig.get_value(mainSection, "virtual_env_dir", false);

    fs::path configDir = specConfig.getConfigDir();
    if (pythonAppDirValue.empty()) {
        python_app_dir = configDir;
    } else {
        python_app_dir = resolvePath(pythonAppDirValue, configDir);
    }

    if (!fs::exists(python_app_dir)) {
        throw std::runtime_error("Configured python_app_dir does not exist: " + python_app_dir.string());
    }

    exec_app_path = resolvePath(execPathValue, python_app_dir);

    if (!fs::exists(exec_app_path)) {
        throw std::runtime_error("Configured exec_app_path does not exist: " + exec_app_path.string());
    }

    if (!requirementsValue.empty()) {
        requirements_file = resolvePath(requirementsValue, python_app_dir);
    } else {
        requirements_file.clear();
    }

    exec_app_args = execArgsValue.empty()
        ? std::vector<std::string>{}
        : parseCommandArguments(execArgsValue);

    exec_app_env = execEnvValue.empty()
        ? std::vector<std::pair<std::string, std::string>>{}
        : parseEnvironmentAssignments(execEnvValue);

    if (virtualEnvValue.empty()) {
        virtualEnvValue = ".venv";
    }
    virtual_env_path = resolvePath(virtualEnvValue, python_app_dir);

    requirements.clear();

    int index = 1;
    while (true) {
        std::string key_name = "requirement_" + std::to_string(index);
        std::string key_url = "requirement_" + std::to_string(index) + "_url";
        std::string key_file_name = "requirement_" + std::to_string(index) + "_file_name";
        std::string key_cmd_params = "requirement_" + std::to_string(index) + "_cmd_params";
        std::string key_version_cmd = "requirement_" + std::to_string(index) + "_version_check_command";
        std::string key_version_regex = "requirement_" + std::to_string(index) + "_version_regex";
        std::string key_min_version = "requirement_" + std::to_string(index) + "_min_version";
        std::string key_launch_file = "requirement_" + std::to_string(index) + "_launch_file";
        std::string key_capture_stderr = "requirement_" + std::to_string(index) + "_capture_stderr";
        std::string key_install_command = "requirement_" + std::to_string(index) + "_install_command";

        std::string req_name = specConfig.get_value(reqSection, key_name, false);
        std::string req_url = specConfig.get_value(reqSection, key_url, false);
        std::string req_file_name = specConfig.get_value(reqSection, key_file_name, false);

        if (req_name.empty() && req_url.empty()) {
            break;
        }

        Requirement req;
        req.name = req_name;
        req.url = req_url;
        req.file_name = req_file_name;
        req.cmd_params = specConfig.get_value(reqSection, key_cmd_params, false);
        req.version_check_command = specConfig.get_value(reqSection, key_version_cmd, false);
        req.version_regex = specConfig.get_value(reqSection, key_version_regex, false);
        req.min_version = specConfig.get_value(reqSection, key_min_version, false);
        req.launch_file = specConfig.get_value(reqSection, key_launch_file, false);
        req.capture_stderr = parseBool(specConfig.get_value(reqSection, key_capture_stderr, false), false);
        req.install_command = specConfig.get_value(reqSection, key_install_command, false);

        requirements.emplace_back(req);
        index++;
    }
}


PythonSetupStatus AppBootstrapper::checkPythonSetup()
{
    std::string python_ver = python.getPythonVersion();
    
    if (python_ver.empty()) {
        return {PythonSetupStatus::Status::NOT_FOUND};
    }

    if (!python.isPythonInstalledAndMeetsVersion(python_min_ver)) {
        return { PythonSetupStatus::Status::UPDATE_REQUIRED, "Python update required.\n" };
    }

    return { PythonSetupStatus::Status::SUCCESS, "" };
}


PythonSetupStatus::Status AppBootstrapper::getPythonSetupStatus()
{
    PythonSetupStatus status = checkPythonSetup();

    switch (status.status) {
        case PythonSetupStatus::Status::NOT_FOUND:
            if (!status.message.empty()) {
                std::cout << status.message << std::endl;
            }
            break;
        case PythonSetupStatus::Status::UPDATE_REQUIRED:
            if (!status.message.empty()) {
                std::cerr << status.message << std::endl;
            }
            break;
        case PythonSetupStatus::Status::SUCCESS:
            if (!status.message.empty()) {
                std::cout << status.message << std::endl;
            }
            break;
    }
    
    return status.status;
}


bool AppBootstrapper::setupVirtualEnv()
{
    try {
        if (virtual_env_path.empty()) {
            std::cerr << "Virtual environment path is not configured." << std::endl;
            return false;
        }

        fs::path marker = virtual_env_path / "pyvenv.cfg";
        if (fs::exists(marker)) {
            if (!virtual_env_reported_) {
                std::cout << "Virtual environment already available at " << virtual_env_path << std::endl;
                virtual_env_reported_ = true;
            }
            return true;
        }

        virtual_env_reported_ = false;

        if (!virtual_env_path.parent_path().empty() &&
            !Utils::ensureDirExists(virtual_env_path.parent_path().string())) {
            std::cerr << "Failed to create virtual environment directory: "
                      << virtual_env_path.parent_path() << std::endl;
            return false;
        }

        std::string pythonCmd = python.getResolvedPythonCommand();
        std::cout << "Creating virtual environment with '" << pythonCmd
                  << "' at " << virtual_env_path << std::endl;

        std::vector<std::string> args{"-m", "venv", virtual_env_path.string()};
        bp::child process(
            bp::exe = pythonCmd,
            bp::args = args,
            bp::start_dir = python_app_dir.string()
        );
        process.wait();
        if (process.exit_code() != 0) {
            std::cerr << "Virtual environment creation failed with exit code "
                      << process.exit_code() << std::endl;
            return false;
        }

        return true;
    } catch (const std::exception& err) {
        std::cerr << "Virtual environment setup failed: " << err.what() << std::endl;
        return false;
    }
}


bool AppBootstrapper::installPythonDependencies()
{
    if (requirements_file.empty()) {
        std::cout << "No Python requirements specified; skipping dependency installation." << std::endl;
        return true;
    }

    if (!fs::exists(requirements_file)) {
        std::cerr << "Requirements file not found: " << requirements_file << std::endl;
        return false;
    }

    fs::path pythonExecutable = getVirtualEnvPythonExecutable();
    if (!fs::exists(pythonExecutable)) {
        std::cerr << "Virtual environment Python executable not found at "
                  << pythonExecutable << std::endl;
        return false;
    }

    fs::path stateFile = getRequirementsStateFile();
    std::string signature = computeRequirementsSignature();
    std::string existingSignature;

    if (!signature.empty() && fs::exists(stateFile)) {
        std::ifstream stateIn(stateFile);
        if (stateIn) {
            std::getline(stateIn, existingSignature);
        }
    }

    if (!signature.empty() && signature == existingSignature) {
        std::cout << "Python dependencies already installed; requirements file unchanged." << std::endl;
        return true;
    }

    auto runPython = [&](const std::vector<std::string>& args, const std::string& action) -> bool {
        try {
            bp::child process(
                bp::exe = pythonExecutable.string(),
                bp::args = args,
                bp::start_dir = python_app_dir.string()
            );
            process.wait();
            if (process.exit_code() != 0) {
                std::cerr << "Failed to " << action << " (exit code "
                          << process.exit_code() << ")." << std::endl;
                return false;
            }
            return true;
        } catch (const std::exception& err) {
            std::cerr << "Failed to " << action << ": " << err.what() << std::endl;
            return false;
        }
    };

    if (!runPython({"-m", "pip", "install", "--upgrade", "pip"}, "upgrade pip")) {
        return false;
    }

    std::vector<std::string> installArgs{
        "-m", "pip", "install", "-r", requirements_file.string()
    };

    if (!runPython(installArgs, "install Python dependencies")) {
        return false;
    }

    if (!signature.empty()) {
        persistRequirementsSignature(signature);
    }

    return true;
}


bool AppBootstrapper::launchPythonApp()
{
    fs::path pythonExecutable = getVirtualEnvPythonExecutable();
    if (!fs::exists(pythonExecutable)) {
        std::cerr << "Cannot launch application; virtual environment Python missing at "
                  << pythonExecutable << std::endl;
        return false;
    }

    if (exec_app_path.empty() || !fs::exists(exec_app_path)) {
        std::cerr << "Application entry point not found: " << exec_app_path << std::endl;
        return false;
    }

    std::string scriptArgument;
    try {
        scriptArgument = fs::relative(exec_app_path, python_app_dir).string();
    } catch (const std::exception&) {
        scriptArgument = exec_app_path.string();
    }

    std::cout << "Launching Python application (" << scriptArgument << ")..." << std::endl;
    try {
        std::vector<std::string> args;
        args.emplace_back(scriptArgument);
        args.insert(args.end(), exec_app_args.begin(), exec_app_args.end());

        bp::environment env = boost::this_process::environment();
        for (const auto& entry : exec_app_env) {
            env[entry.first] = entry.second;
        }

        bp::child process(
            bp::exe = pythonExecutable.string(),
            bp::args = args,
            bp::start_dir = python_app_dir.string(),
            bp::env = env
        );
        process.wait();
        if (process.exit_code() != 0) {
            std::cerr << "Python application exited with code "
                      << process.exit_code() << std::endl;
            return false;
        }
    } catch (const std::exception& err) {
        std::cerr << "Failed to start Python application: " << err.what() << std::endl;
        return false;
    }

    return true;
}


bool AppBootstrapper::installRequirements()
{
    if (requirements.empty()) {
        return true;
    }

    bool success = true;
    fs::path distribDir("distrib");

    for (auto& req : requirements) {
        if (req.name.empty()) {
            continue;
        }

        if (isRequirementAlreadyInstalled(req)) {
            continue;
        }

        if (!req.install_command.empty()) {
            std::cout << "Installing " << req.name << " using command: " << req.install_command << std::endl;
            int result = std::system(req.install_command.c_str());
            if (result != 0) {
                std::cerr << "Failed to install " << req.name << " (exit code " << result << ")." << std::endl;
                success = false;
                break;
            }
            continue;
        }

        if (req.url.empty()) {
            std::cout << "No installer URL configured for " << req.name << ". Skipping." << std::endl;
            continue;
        }

        std::string fileName = req.file_name.empty() ? Utils::getFileNameFromUrl(req.url) : req.file_name;
        fs::path installerPath = distribDir / fileName;

        if (!fs::exists(installerPath)) {
            std::cout << "Installer for " << req.name << " is not available at "
                      << installerPath << ". Skipping." << std::endl;
            continue;
        }

        if (!Utils::isWindows()) {
            std::cout << req.name << " package downloaded to " << installerPath
                      << ". Manual installation is required on this platform." << std::endl;
            continue;
        }

        std::string command = "\"" + installerPath.string() + "\"";
        if (!req.cmd_params.empty()) {
            command += " " + req.cmd_params;
        }

        std::cout << "Running command: " << command << std::endl;
        int result = std::system(command.c_str());
        if (result != 0) {
            std::cerr << "Failed to install: " << req.name << std::endl;
            success = false;
            break;
        }
        
        std::cout << std::endl;
    }
    return success;
}


bool AppBootstrapper::downloadRequirements()
{
    if (requirements.empty()) {
        std::cout << "No external requirements configured." << std::endl;
        return true;
    }

    if (!Utils::ensureDirExists("distrib/")) {
        std::cerr << "Failed to create 'distrib/' directory." << std::endl;
        return false;
    }

    bool allDownloadsSuccessful = true;

    for (auto& req : requirements) {
        if (req.name.empty()) {
            continue;
        }

        if (isRequirementAlreadyInstalled(req)) {
            continue;
        }

        if (!req.install_command.empty()) {
            std::cout << req.name << " will be installed via command; no download required." << std::endl;
            continue;
        }

        if (req.url.empty()) {
            std::cout << "No download URL configured for " << req.name << ". Skipping." << std::endl;
            continue;
        }

        std::string fileName = req.file_name.empty() ? Utils::getFileNameFromUrl(req.url) : req.file_name;
        fs::path req_path = fs::path("distrib") / fileName;
    
        if (!Utils::isFileComplete(req_path.string(), req.url)) {
            std::cout << "Downloading: " << req.name << " from " << req.url << std::endl;
            if (!Utils::downloadFile(req.url, req_path.string())) {
                std::cout << std::endl;
                std::cerr << "Failed to download: " << req.name << " from " << req.url << std::endl;
                allDownloadsSuccessful = false;
                break;
            }
        } else {
            std::cout << req_path << " is already downloaded" << std::endl;
        }
        std::cout << std::endl;
    }    

    return allDownloadsSuccessful;
}


std::string AppBootstrapper::getPythonDownloadUrl()
{
    return python_download_url;
}


std::vector<Requirement> AppBootstrapper::getRequirements()
{
    return requirements;
}


fs::path AppBootstrapper::resolvePath(const std::string& value, const fs::path& baseDir) const
{
    if (value.empty()) {
        return {};
    }

    fs::path candidate(value);
    if (!candidate.is_absolute()) {
        candidate = baseDir / candidate;
    }

    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(candidate, ec);
    if (ec) {
        return candidate.lexically_normal();
    }

    return normalized;
}


fs::path AppBootstrapper::getVirtualEnvPythonExecutable() const
{
#ifdef _WIN32
    return virtual_env_path / "Scripts" / "python.exe";
#else
    return virtual_env_path / "bin" / "python";
#endif
}


bool AppBootstrapper::parseBool(const std::string& value, bool fallback)
{
    if (value.empty()) {
        return fallback;
    }

    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }

    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }

    return fallback;
}


std::filesystem::path AppBootstrapper::getRequirementsStateFile() const
{
    return virtual_env_path / ".pyappexec_requirements_state";
}


std::string AppBootstrapper::computeRequirementsSignature() const
{
    if (requirements_file.empty() || !fs::exists(requirements_file)) {
        return {};
    }

    std::ostringstream oss;
    oss << requirements_file.string() << '|'
        << fs::file_size(requirements_file) << '|'
        << fs::last_write_time(requirements_file).time_since_epoch().count();
    return oss.str();
}


void AppBootstrapper::persistRequirementsSignature(const std::string& signature) const
{
    if (signature.empty()) {
        return;
    }

    std::ofstream out(getRequirementsStateFile(), std::ios::trunc);
    if (out) {
        out << signature;
    }
}


std::vector<std::string> AppBootstrapper::parseCommandArguments(const std::string& args)
{
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;
    char quoteChar = '\0';
    bool escaping = false;

    for (char ch : args) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }

        if (inQuotes) {
            if (ch == '\\') {
                escaping = true;
                continue;
            }

            if (ch == quoteChar) {
                inQuotes = false;
                continue;
            }

            current.push_back(ch);
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"' || ch == '\'') {
            inQuotes = true;
            quoteChar = ch;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                result.emplace_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (escaping) {
        current.push_back('\\');
    }

    if (!current.empty()) {
        result.emplace_back(current);
    }

    return result;
}


std::vector<std::pair<std::string, std::string>> AppBootstrapper::parseEnvironmentAssignments(const std::string& envSpec)
{
    std::vector<std::pair<std::string, std::string>> assignments;
    std::stringstream stream(envSpec);
    std::string token;

    while (std::getline(stream, token, ';')) {
        std::string trimmed = trimCopy(token);
        if (trimmed.empty()) {
            continue;
        }

        auto equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            std::cerr << "Ignoring malformed environment assignment: " << trimmed << std::endl;
            continue;
        }

        std::string key = trimCopy(trimmed.substr(0, equalsPos));
        std::string value = trimCopy(trimmed.substr(equalsPos + 1));

        if (key.empty()) {
            std::cerr << "Ignoring environment assignment with empty key." << std::endl;
            continue;
        }

        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        assignments.emplace_back(key, value);
    }

    return assignments;
}


bool AppBootstrapper::isRequirementAlreadyInstalled(Requirement& req)
{
    if (req.version_check_command.empty()) {
        return false;
    }

    try {
        std::string output = Utils::runAndCaptureOutput(req.version_check_command, req.capture_stderr);
        std::string version = req.version_regex.empty()
            ? trimCopy(output)
            : Utils::extractVersion(output, req.version_regex);
        version = trimCopy(version);

        if (version.empty()) {
            return false;
        }

        if (req.min_version.empty()) {
            if (!req.status_reported) {
                std::cout << req.name << " detected (version " << version << ")." << std::endl;
            }
            req.last_version_check_result = true;
            req.status_reported = true;
            return true;
        }

        bool meets = Utils::isVersionAtLeast(version, req.min_version);
        bool shouldLog = !req.status_reported || !req.last_version_check_result.has_value() || req.last_version_check_result.value() != meets;
        if (shouldLog) {
            if (meets) {
                std::cout << req.name << " " << version
                          << " meets minimum version " << req.min_version << std::endl;
            } else {
                std::cout << req.name << " " << version
                          << " is below required version " << req.min_version << std::endl;
            }
        }
        req.last_version_check_result = meets;
        req.status_reported = true;

        return meets;
    } catch (const std::exception& err) {
        std::cerr << "Requirement check for " << req.name
                  << " failed: " << err.what() << std::endl;
        return false;
    }
}


std::string AppBootstrapper::getOSPrefix()
{
#ifdef _WIN32
    return "Windows";
#elif __APPLE__
    return "MacOS";
#elif __linux__
    return "Linux";
#else
    throw std::runtime_error("Unsupported OS");
#endif
}


bool AppBootstrapper::tryInstallPythonFromCommonPackageManagers()
{
#if defined(__linux__)
    struct PackageManagerInfo {
        std::string name;
        std::string availabilityCheck;
        std::string versionQuery;
        std::string versionRegex;
        std::string installCommand;
    };

    const std::vector<PackageManagerInfo> managers = {
        {
            "apt",
            "command -v apt-get",
            "apt-cache policy python3 | grep Candidate",
            R"(Candidate:\s*([0-9.]+))",
            "sudo apt-get update && sudo apt-get install -y python3"
        },
        {
            "dnf",
            "command -v dnf",
            "dnf --showduplicates list python3 2>/dev/null | tail -n 1",
            R"(python3\s+([0-9.]+))",
            "sudo dnf install -y python3"
        },
        {
            "pacman",
            "command -v pacman",
            "pacman -Si python | grep Version",
            R"(Version\s*:\s*([0-9.]+))",
            "sudo pacman -S --noconfirm python"
        }
    };

    for (const auto& manager : managers) {
        int available = std::system((manager.availabilityCheck + " >/dev/null 2>&1").c_str());
        if (available != 0) {
            continue;
        }

        try {
            std::string output = Utils::runAndCaptureOutput("/bin/bash -lc \"" + manager.versionQuery + "\"", true);
            std::string offeredVersion = Utils::extractVersion(output, manager.versionRegex);
            offeredVersion = trimCopy(offeredVersion);

            if (offeredVersion.empty()) {
                std::cout << "Unable to determine Python version offered by " << manager.name << "." << std::endl;
                continue;
            }

            if (!python.isPythonVersionAtLeast(offeredVersion, python_min_ver)) {
                std::cerr << manager.name << " offers Python " << offeredVersion
                          << ", which is below required version " << python_min_ver << std::endl;
                continue;
            }

            std::cout << "Attempting to install/upgrade Python via " << manager.name << "..." << std::endl;
            int installResult = std::system(manager.installCommand.c_str());
            if (installResult == 0) {
                std::cout << "Python installation via " << manager.name << " completed." << std::endl;
                return true;
            } else {
                std::cerr << manager.name << " installation command failed with exit code "
                          << installResult << std::endl;
                return false;
            }
        } catch (const std::exception& err) {
            std::cerr << "Failed to query " << manager.name << " for Python versions: "
                      << err.what() << std::endl;
        }
    }

    std::cerr << "No supported package manager could provide the required Python version." << std::endl;
    return false;
#else
    std::cerr << "Automatic Python installation is not supported on this platform." << std::endl;
    return false;
#endif
}
