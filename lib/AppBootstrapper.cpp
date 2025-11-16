#include "AppBootstrapper.hpp"
#include "SpecConfig.hpp"
#include "Utils.hpp"
#include <algorithm>
#if defined(__has_include)
#  if __has_include(<boost/process/v1.hpp>)
#    define PYAPPEXEC_USE_BOOST_PROCESS_V1 1
#  endif
#endif

#ifdef PYAPPEXEC_USE_BOOST_PROCESS_V1
#  define BOOST_PROCESS_VERSION 1
#  include <boost/process/v1.hpp>
#else
#  include <boost/process.hpp>
#endif
#include <cstdlib>
#include <system_error>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
#ifdef PYAPPEXEC_USE_BOOST_PROCESS_V1
namespace bp = boost::process::v1;
#else
namespace bp = boost::process;
#endif
#undef PYAPPEXEC_USE_BOOST_PROCESS_V1

namespace {
const char kPathSeparator =
#ifdef _WIN32
    ';';
#else
    ':';
#endif

bool isProtectedInstallPath(const std::filesystem::path& path)
{
#ifdef _WIN32
    std::string lower = path.string();
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower.find("program files") != std::string::npos;
#else
    (void)path;
    return false;
#endif
}

bool isProcessElevated()
{
#ifdef _WIN32
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = nullptr;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &administratorsGroup)) {
        CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }
    return isAdmin == TRUE;
#else
    return true;
#endif
}

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
        spdlog::warn("Python not found.");
    }
}


void AppBootstrapper::parseConfig()
{
    std::string osPrefix = getOSPrefix();
    std::string mainSection = osPrefix + ":main";
    std::string reqSection = osPrefix + ":requirements";

    parseMainSection(mainSection);
    parseRequirementsSection(reqSection);
}


void AppBootstrapper::parseMainSection(const std::string& mainSection)
{
    std::string pythonAppDirValue = specConfig.get_value(mainSection, "python_app_dir", false);
    std::string execPathValue = specConfig.get_value(mainSection, "exec_app_path", true);
    std::string execArgsValue = specConfig.get_value(mainSection, "exec_app_args", false);
    std::string execEnvValue = specConfig.get_value(mainSection, "exec_env", false);
    std::string requirementsValue = specConfig.get_value(mainSection, "requirements_file", false);
    std::string virtualEnvValue = specConfig.get_value(mainSection, "virtual_env_dir", false);

    python_download_url = specConfig.get_value(mainSection, "python_download_url", false);
    python_min_ver = specConfig.get_value(mainSection, "python_min_ver", true);

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
}


void AppBootstrapper::parseRequirementsSection(const std::string& reqSection)
{
    int index = 1;
    requirements.clear();

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
    std::string key_append_path = "requirement_" + std::to_string(index) + "_append_to_path";
    std::string key_standalone = "requirement_" + std::to_string(index) + "_standalone";
    std::string key_install_dir = "requirement_" + std::to_string(index) + "_install_dir";
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
    req.append_to_path = parseBool(specConfig.get_value(reqSection, key_append_path, false), false);
    req.standalone = parseBool(specConfig.get_value(reqSection, key_standalone, false), false);
    req.install_dir = specConfig.get_value(reqSection, key_install_dir, false);
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
                spdlog::warn(status.message);
            }
            break;
        case PythonSetupStatus::Status::UPDATE_REQUIRED:
            if (!status.message.empty()) {
                spdlog::error(status.message);
            }
            break;
        case PythonSetupStatus::Status::SUCCESS:
            if (!status.message.empty()) {
                spdlog::info(status.message);
            }
            break;
    }
    
    return status.status;
}


bool AppBootstrapper::setupVirtualEnv()
{
    try {
        if (virtual_env_path.empty()) {
            spdlog::error("Virtual environment path is not configured.");
            return false;
        }

        fs::path marker = virtual_env_path / "pyvenv.cfg";
        if (fs::exists(marker)) {
            if (!virtual_env_reported_) {
                spdlog::info("Virtual environment already available at {}", virtual_env_path.string());
                virtual_env_reported_ = true;
            }
            return true;
        }

        virtual_env_reported_ = false;

        if (!virtual_env_path.parent_path().empty() &&
            !Utils::ensureDirExists(virtual_env_path.parent_path().string())) {
            spdlog::error("Failed to create virtual environment directory: {}",
                          virtual_env_path.parent_path().string());
            return false;
        }

        std::string pythonCmd = python.getResolvedPythonCommand();
        spdlog::info("Creating virtual environment with '{}' at {}",
                     pythonCmd, virtual_env_path.string());

        std::vector<std::string> args{"-m", "venv", virtual_env_path.string()};
        bp::child process(
            bp::exe = pythonCmd,
            bp::args = args,
            bp::start_dir = python_app_dir.string()
        );
        process.wait();
        if (process.exit_code() != 0) {
            spdlog::error("Virtual environment creation failed with exit code {}",
                          process.exit_code());
            return false;
        }

        return true;
    } catch (const std::exception& err) {
        spdlog::error("Virtual environment setup failed: {}", err.what());
        return false;
    }
}


bool AppBootstrapper::installPythonDependencies()
{
    if (requirements_file.empty()) {
        spdlog::info("No Python requirements specified; skipping dependency installation.");
        return true;
    }

    fs::path pythonExecutable;
    if (!ensureRequirementsInputs(pythonExecutable)) {
        return false;
    }

    fs::path stateFile = getRequirementsStateFile();
    std::string signature = computeRequirementsSignature();
    
    if (isRequirementsStateCurrent(signature, stateFile)) {
        spdlog::info("Python dependencies already installed; requirements file unchanged.");
        return true;
    }

    if (!runPythonInVirtualEnv(
            pythonExecutable,
            {"-m", "pip", "install", "--upgrade", "pip"},
            "upgrade pip")) {
        return false;
    }

    std::vector<std::string> installArgs{
        "-m", "pip", "install", "-r", requirements_file.string()
    };

    if (!runPythonInVirtualEnv(pythonExecutable, installArgs, "install Python dependencies")) {
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
        spdlog::error("Cannot launch application; virtual environment Python missing at {}",
                       pythonExecutable.string());
        return false;
    }

    if (exec_app_path.empty() || !fs::exists(exec_app_path)) {
        spdlog::error("Application entry point not found: {}", exec_app_path.string());
        return false;
    }

    std::string scriptArgument;
    try {
        scriptArgument = fs::relative(exec_app_path, python_app_dir).string();
    } catch (const std::exception&) {
        scriptArgument = exec_app_path.string();
    }

    spdlog::info("Launching Python application ({})...", scriptArgument);
    try {
        std::vector<std::string> args;
        args.emplace_back(scriptArgument);
        args.insert(args.end(), exec_app_args.begin(), exec_app_args.end());

        bp::environment env = boost::this_process::environment();
        for (const auto& entry : exec_app_env) {
            env[entry.first] = entry.second;
        }
        if (!requirement_bin_paths_.empty()) {
            std::string pathEnv = env["PATH"].to_string();
            for (const auto& p : requirement_bin_paths_) {
                pathEnv.push_back(kPathSeparator);
                pathEnv.append(p.string());
            }
            env["PATH"] = pathEnv;
        }

        bp::child process(
            bp::exe = pythonExecutable.string(),
            bp::args = args,
            bp::start_dir = python_app_dir.string(),
            bp::env = env
        );
        process.wait();
        if (process.exit_code() != 0) {
            spdlog::error("Python application exited with code {}", process.exit_code());
            return false;
        }
    } catch (const std::exception& err) {
        spdlog::error("Failed to start Python application: {}", err.what());
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

        if (req.url.empty()) {
            spdlog::info("No installer URL configured for {}. Skipping.", req.name);
            continue;
        }

        std::string fileName = req.file_name.empty() ? Utils::getFileNameFromUrl(req.url) : req.file_name;
        fs::path installerPath = distribDir / fileName;

        if (!fs::exists(installerPath)) {
            spdlog::warn("Installer for {} is not available at {}. Skipping.",
                         req.name, installerPath.string());
            continue;
        }

        if (!req.install_command.empty()) {
            spdlog::info("Installing {} using command: {}", req.name, req.install_command);
            int result = std::system(req.install_command.c_str());
            if (result != 0) {
                spdlog::error("Failed to install {} (exit code {}).", req.name, result);
                success = false;
                break;
            }
            continue;
        }

        bool handled = false;
        if (req.install_command.empty() && req.cmd_params.empty()) {
            fs::path destDir;
            if (req.standalone && !req.install_dir.empty()) {
                destDir = fs::path(req.install_dir);
            } else if (req.standalone) {
                #ifdef _WIN32
                std::string programFiles = std::getenv("ProgramFiles") ? std::getenv("ProgramFiles") : "C:/Program Files";
                #else
                std::string programFiles = "/usr/local";
                #endif
                destDir = fs::path(programFiles) / (req.name.empty() ? installerPath.stem() : fs::path(req.name));
            } else {
                destDir = distribDir / installerPath.stem();
            }

            std::error_code dirEc;
            fs::create_directories(destDir, dirEc);
            if (dirEc && !fs::exists(destDir)) {
                spdlog::error("Cannot create install directory {}.", destDir.string());
#ifdef _WIN32
                if (!isProcessElevated()) {
                    spdlog::error("Please relaunch PyAppExec as Administrator or pick a user-writable install_dir.");
                }
#endif
                success = false;
                break;
            }

            auto runCommand = [&](const std::string& command) -> bool {
                spdlog::info("Installing {} by extracting archive: {}", req.name, command);
                int result = std::system(command.c_str());
                if (result != 0) {
                    spdlog::error("Failed to extract {} (exit code {}).", req.name, result);
#ifdef _WIN32
                    if (!isProcessElevated() && (isProtectedInstallPath(destDir) || result == 5 || result == 740)) {
                        spdlog::error("Extraction may require Administrator rights. Please relaunch PyAppExec as Administrator or choose a user-writable install_dir.");
                    } else if (isProtectedInstallPath(destDir)) {
                        spdlog::error("Extraction failed in protected target {}. Please verify installer integrity or permissions.", destDir.string());
                    }
#endif
                    return false;
                }
                if (req.append_to_path) {
                    addRequirementPath(destDir);
                }
                return true;
            };

            const std::string ext = installerPath.extension().string();
#ifdef _WIN32
            if (ext == ".zip") {
                std::ostringstream cmd;
                cmd << "powershell -ExecutionPolicy Bypass -Command \""
                    << "$ErrorActionPreference='Stop';"
                    << "$ProgressPreference='SilentlyContinue';"
                    << "$dl='" << installerPath.string() << "';"
                    << "$dest='" << destDir.string() << "';"
                    << "if(Test-Path $dest){Remove-Item $dest -Recurse -Force};"
                    << "Expand-Archive -LiteralPath $dl -DestinationPath $dest -Force;"
                    << "$ff=Get-ChildItem -Path $dest -Filter 'ffmpeg.exe' -Recurse -File | Select-Object -First 1;"
                    << "if($ff){Copy-Item $ff.FullName -Destination (Join-Path $dest 'ffmpeg.exe') -Force};"
                    << "$fp=Get-ChildItem -Path $dest -Filter 'ffprobe.exe' -Recurse -File | Select-Object -First 1;"
                    << "if($fp){Copy-Item $fp.FullName -Destination (Join-Path $dest 'ffprobe.exe') -Force};"
                    << "\"";
                handled = runCommand(cmd.str());
                if (!handled) {
                    success = false;
                    break;
                }
                continue;
            }
#else
            auto toolAvailable = [](const std::string& tool) {
                std::string probe = "command -v " + tool + " >/dev/null 2>&1";
                return std::system(probe.c_str()) == 0;
            };

            std::string command;
            std::string lowerName = installerPath.filename().string();
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.ends_with(".tar.gz") || lowerName.ends_with(".tgz") ||
                lowerName.ends_with(".tar.bz2") || lowerName.ends_with(".tbz2") ||
                lowerName.ends_with(".tar.xz") || lowerName.ends_with(".txz") ||
                lowerName.ends_with(".tar")) {
                if (toolAvailable("tar")) {
                    command = "tar -xf \"" + installerPath.string() + "\" -C \"" + destDir.string() + "\"";
                }
            } else if (lowerName.ends_with(".zip")) {
                if (toolAvailable("unzip")) {
                    command = "unzip -o \"" + installerPath.string() + "\" -d \"" + destDir.string() + "\"";
                }
            } else if (lowerName.ends_with(".7z") || lowerName.ends_with(".rar")) {
                if (toolAvailable("7z")) {
                    command = "7z x -y \"" + installerPath.string() + "\" -o\"" + destDir.string() + "\"";
                } else if (toolAvailable("7za")) {
                    command = "7za x -y \"" + installerPath.string() + "\" -o\"" + destDir.string() + "\"";
                }
            }

            if (!command.empty()) {
                handled = runCommand(command);
                if (!handled) {
                    success = false;
                    break;
                }
                continue;
            } else {
                spdlog::warn("No suitable extractor found for {}. Provide an explicit install_command for this requirement.",
                             installerPath.string());
            }
#endif
        }

        if (!Utils::isWindows()) {
            spdlog::info("{} package downloaded to {}. Manual installation required on this platform.",
                         req.name, installerPath.string());
            continue;
        }

        std::string command = "\"" + installerPath.string() + "\"";
        if (!req.cmd_params.empty()) {
            command += " " + req.cmd_params;
        }

        spdlog::info("Running command: {}", command);
        int result = std::system(command.c_str());
        if (result != 0) {
            spdlog::error("Failed to install: {}", req.name);
#ifdef _WIN32
            if (!isProcessElevated() && (isProtectedInstallPath(installerPath.parent_path()) || result == 5 || result == 740)) {
                spdlog::error("Install target may require Administrator rights. Please relaunch PyAppExec as Administrator or choose a user-writable install_dir.");
            } else if (isProtectedInstallPath(installerPath.parent_path())) {
                spdlog::error("Install failed in protected location {}. Please verify installer integrity or permissions.", installerPath.parent_path().string());
            }
#endif
            success = false;
            break;
        }
    }
    return success;
}


bool AppBootstrapper::downloadRequirements()
{
    if (requirements.empty()) {
        spdlog::info("No external requirements configured.");
        return true;
    }

    if (!Utils::ensureDirExists("distrib/")) {
        spdlog::error("Failed to create 'distrib/' directory.");
        // if the directory now exists, we assume another process created it between calls
        if (!fs::exists("distrib/")) {
            return false;
        }
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
            spdlog::info("{} will be installed via command; no download required.", req.name);
            continue;
        }

        if (req.url.empty()) {
            spdlog::info("No download URL configured for {}. Skipping.", req.name);
            continue;
        }

        std::string fileName = req.file_name.empty() ? Utils::getFileNameFromUrl(req.url) : req.file_name;
        fs::path req_path = fs::path("distrib") / fileName;
    
        if (!Utils::isFileComplete(req_path.string(), req.url)) {
            spdlog::info("Downloading: {} from {}", req.name, req.url);
            if (!Utils::downloadFile(req.url, req_path.string())) {
                spdlog::error("Failed to download: {} from {}", req.name, req.url);
                allDownloadsSuccessful = false;
                break;
            }
        } else {
            spdlog::info("{} is already downloaded", req_path.string());
        }
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
        << static_cast<long long>(fs::file_size(requirements_file)) << '|'
        << static_cast<long long>(fs::last_write_time(requirements_file).time_since_epoch().count());
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

bool AppBootstrapper::ensureRequirementsInputs(std::filesystem::path& pythonExecutable) const
{
    if (!fs::exists(requirements_file)) {
        spdlog::error("Requirements file not found: {}", requirements_file.string());
        return false;
    }

    pythonExecutable = getVirtualEnvPythonExecutable();
    if (!fs::exists(pythonExecutable)) {
        spdlog::error(
            "Virtual environment Python executable not found at {}",
            pythonExecutable.string());
        return false;
    }

    return true;
}


bool AppBootstrapper::isRequirementsStateCurrent(
    const std::string& signature,
    const std::filesystem::path& stateFile) const
{
    if (signature.empty() || !fs::exists(stateFile)) {
        return false;
    }

    std::ifstream stateIn(stateFile);
    if (!stateIn) {
        return false;
    }

    std::string existingSignature;
    std::getline(stateIn, existingSignature);
    return signature == existingSignature;
}


bool AppBootstrapper::runPythonInVirtualEnv(
    const std::filesystem::path& pythonExecutable,
    const std::vector<std::string>& args,
    const std::string& action) const
{
    try {
        bp::child process(
            bp::exe = pythonExecutable.string(),
            bp::args = args,
            bp::start_dir = python_app_dir.string());
        process.wait();
        if (process.exit_code() != 0) {
            spdlog::error("Failed to {} (exit code {}).", action, process.exit_code());
            return false;
        }
        return true;
    } catch (const std::exception& err) {
        spdlog::error("Failed to {}: {}", action, err.what());
        return false;
    }
}

#if defined(__linux__)

const std::vector<AppBootstrapper::PackageManagerInfo>& AppBootstrapper::getPackageManagers()
{
    static const std::vector<PackageManagerInfo> managers = {
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

    return managers;
}


bool AppBootstrapper::isManagerAvailable(const PackageManagerInfo& manager) const
{
    int available = std::system((manager.availabilityCheck + " >/dev/null 2>&1").c_str());
    return available == 0;
}


std::optional<std::string> AppBootstrapper::queryManagerVersion(const PackageManagerInfo& manager) const
{
    try {
        std::string output = Utils::runAndCaptureOutput(
            std::string("/bin/bash -lc \"") + manager.versionQuery + "\"",
            true);
        std::string offeredVersion = Utils::extractVersion(output, manager.versionRegex);
        offeredVersion = trimCopy(offeredVersion);

        if (offeredVersion.empty()) {
            spdlog::info("Unable to determine Python version offered by {}.", manager.name);
            return std::nullopt;
        }

        return offeredVersion;
    } catch (const std::exception& err) {
        spdlog::error("Failed to query {} for Python versions: {}", manager.name, err.what());
        return std::nullopt;
    }
}


bool AppBootstrapper::installViaManager(const PackageManagerInfo& manager) const
{
    spdlog::info("Attempting to install/upgrade Python via {}...", manager.name);
    int installResult = std::system(manager.installCommand.c_str());
    if (installResult == 0) {
        spdlog::info("Python installation via {} completed.", manager.name);
        return true;
    }

    spdlog::error(
        "{} installation command failed with exit code {}",
        manager.name,
        installResult);
    return false;
}

#endif


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
            spdlog::warn("Ignoring malformed environment assignment: {}", trimmed);
            continue;
        }

        std::string key = trimCopy(trimmed.substr(0, equalsPos));
        std::string value = trimCopy(trimmed.substr(equalsPos + 1));

        if (key.empty()) {
            spdlog::warn("Ignoring environment assignment with empty key.");
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


namespace {

std::optional<std::string> extractRequirementVersion(const Requirement& req)
{
    try {
        std::string output = Utils::runAndCaptureOutput(req.version_check_command, req.capture_stderr);
        std::string version = req.version_regex.empty()
            ? trimCopy(output)
            : Utils::extractVersion(output, req.version_regex);
        version = trimCopy(version);
        if (version.empty()) {
            return std::nullopt;
        }
        return version;
    } catch (const std::exception& err) {
        spdlog::error("Requirement check for {} failed: {}", req.name, err.what());
        return std::nullopt;
    }
}

void logRequirementStatus(const Requirement& req, const std::string& version, bool meets)
{
    const bool shouldLog = !req.status_reported || !req.last_version_check_result.has_value() ||
        req.last_version_check_result.value() != meets;
    if (!shouldLog) {
        return;
    }

    if (req.min_version.empty() || meets) {
        spdlog::info("{} {} meets minimum version {}", req.name, version,
                     req.min_version.empty() ? version : req.min_version);
    } else {
        spdlog::warn("{} {} is below required version {}", req.name, version, req.min_version);
    }
}

} // namespace

bool AppBootstrapper::isRequirementAlreadyInstalled(Requirement& req)
{
    if (req.version_check_command.empty()) {
        return false;
    }

    auto versionOpt = extractRequirementVersion(req);
    if (!versionOpt) {
        return false;
    }
    const std::string& version = *versionOpt;

    bool meets = req.min_version.empty() || Utils::isVersionAtLeast(version, req.min_version);
    logRequirementStatus(req, version, meets);
    req.last_version_check_result = meets;
    req.status_reported = true;
    if (meets && req.append_to_path) {
        maybeAddRequirementPathFromVersionCheck(req);
    }
    return meets;
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

void AppBootstrapper::addRequirementPath(const fs::path& path)
{
    if (path.empty()) {
        return;
    }
    fs::path normalized = path;
    std::error_code ec;
    normalized = fs::weakly_canonical(normalized, ec);
    if (ec) {
        normalized = path.lexically_normal();
    }
    for (const auto& existing : requirement_bin_paths_) {
        if (existing == normalized) {
            return;
        }
    }
    requirement_bin_paths_.push_back(normalized);
}

void AppBootstrapper::maybeAddRequirementPathFromVersionCheck(const Requirement& req)
{
    std::string cmd = req.version_check_command;
    auto ltrim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    };
    ltrim(cmd);
    std::string program;
    if (cmd.empty()) {
        return;
    }
    if (cmd.front() == '"') {
        const auto endQuote = cmd.find('"', 1);
        if (endQuote != std::string::npos && endQuote > 1) {
            program = cmd.substr(1, endQuote - 1);
        }
    }
    if (program.empty()) {
        std::istringstream ss(cmd);
        ss >> program;
    }
    if (program.empty()) {
        return;
    }
    fs::path candidate(program);
    if (!candidate.has_parent_path()) {
        return;
    }
    fs::path dir = candidate.parent_path();
    if (!dir.is_absolute()) {
        dir = python_app_dir / dir;
    }
    addRequirementPath(dir);
}


bool AppBootstrapper::tryInstallPythonFromCommonPackageManagers()
{
#if defined(__linux__)
    for (const auto& manager : getPackageManagers()) {
        if (!isManagerAvailable(manager)) {
            continue;
        }

        auto offeredVersion = queryManagerVersion(manager);
        if (!offeredVersion.has_value()) {
            continue;
        }

        if (!python.isPythonVersionAtLeast(*offeredVersion, python_min_ver)) {
            spdlog::warn("{} offers Python {}, which is below required version {}",
                         manager.name, *offeredVersion, python_min_ver);
            continue;
        }

        if (installViaManager(manager)) {
            return true;
        }

        // installation attempt failed; stop early since installViaManager already logged why
        return false;
    }

    spdlog::error("No supported package manager could provide the required Python version.");
    return false;
#else
    spdlog::warn("Automatic Python installation is not supported on this platform.");
    return false;
#endif
}
