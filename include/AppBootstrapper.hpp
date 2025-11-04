#ifndef APPBOOTSTRAPPER_HPP
#define APPBOOTSTRAPPER_HPP

#include "PythonManager.hpp"
#include "SpecConfig.hpp"
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>


struct Requirement {
    std::string name;
    std::string url;
    std::string file_name;
    std::string cmd_params;
    std::string version_check_command;
    std::string version_regex;
    std::string min_version;
    std::string launch_file;
    bool capture_stderr{false};
    std::string install_command;
    std::optional<bool> last_version_check_result;
    bool status_reported{false};
};


struct PythonSetupStatus {
    enum class Status {
        SUCCESS,
        NOT_FOUND,
        UPDATE_REQUIRED
    };

    Status status;
    std::string message;
};


class AppBootstrapper
{
public:
    explicit AppBootstrapper(const SpecConfig& specConfig);
    bool setupVirtualEnv();
    bool installPythonDependencies();
    PythonSetupStatus checkPythonSetup();
    PythonSetupStatus::Status getPythonSetupStatus();
    bool installRequirements();
    bool downloadRequirements();
    bool launchPythonApp();
    bool tryInstallPythonFromCommonPackageManagers();

    std::string getPythonDownloadUrl();
    std::vector<Requirement> getRequirements();
    bool isRequirementAlreadyInstalled(Requirement& req);

private:
    PythonManager python;
    SpecConfig specConfig;
    bool virtual_env_reported_{false};

    std::string python_download_url;
    std::string python_min_ver;
    std::filesystem::path python_app_dir;
    std::filesystem::path exec_app_path;
    std::filesystem::path requirements_file;
    std::filesystem::path virtual_env_path;
    std::vector<std::string> exec_app_args;
    std::vector<std::pair<std::string, std::string>> exec_app_env;

    std::vector<Requirement> requirements;
    
    void parseConfig();
    std::string getOSPrefix();
    std::filesystem::path resolvePath(const std::string& value, const std::filesystem::path& baseDir) const;
    std::filesystem::path getVirtualEnvPythonExecutable() const;
    static bool parseBool(const std::string& value, bool fallback);
    std::filesystem::path getRequirementsStateFile() const;
    std::string computeRequirementsSignature() const;
    void persistRequirementsSignature(const std::string& signature) const;
    static std::vector<std::string> parseCommandArguments(const std::string& args);
    static std::vector<std::pair<std::string, std::string>> parseEnvironmentAssignments(const std::string& envSpec);
};

#endif
