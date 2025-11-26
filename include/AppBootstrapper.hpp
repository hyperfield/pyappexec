#ifndef APPBOOTSTRAPPER_HPP
#define APPBOOTSTRAPPER_HPP

#include "PythonManager.hpp"
#include "SpecConfig.hpp"
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>


struct Requirement {
    // cppcheck-suppress unusedStructMember
    std::string name;
    // cppcheck-suppress unusedStructMember
    std::string url;
    // cppcheck-suppress unusedStructMember
    std::string file_name;
    // cppcheck-suppress unusedStructMember
    std::string cmd_params;
    std::string version_check_command;
    // cppcheck-suppress unusedStructMember
    std::string version_regex;
    std::string min_version;
    std::string launch_file;
    // cppcheck-suppress unusedStructMember
    bool capture_stderr{false};
    bool append_to_path{false};
    bool standalone{false};
    std::string install_dir;
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
    bool requiresProvisioning() const;
    static std::string getOSPrefix();
    static bool parseBool(const std::string& value, bool fallback);

    std::string getPythonDownloadUrl();
    std::vector<Requirement> getRequirements();
    bool isRequirementAlreadyInstalled(Requirement& req);
    std::filesystem::path getDistribDir() const;

private:
    static std::filesystem::path defaultConfigRoot(const std::string& appId);
    static std::string sanitizeIdForPath(const std::string& appId);

    PythonManager python;
    SpecConfig specConfig;
    bool virtual_env_reported_{false};

    std::string app_id_;
    std::filesystem::path config_root_;
    std::string python_download_url;
    // cppcheck-suppress unusedPrivateField
    std::string python_min_ver;
    std::filesystem::path python_app_dir;
    std::filesystem::path exec_app_path;
    std::filesystem::path requirements_file;
    std::filesystem::path virtual_env_path;
    std::vector<std::string> exec_app_args;
    std::vector<std::pair<std::string, std::string>> exec_app_env;
    std::vector<std::filesystem::path> requirement_bin_paths_;

    // cppcheck-suppress unusedPrivateField
    std::vector<Requirement> requirements;
    
    void parseConfig();
    std::filesystem::path resolvePath(const std::string& value, const std::filesystem::path& baseDir) const;
    std::filesystem::path getVirtualEnvPythonExecutable() const;
    std::filesystem::path getRequirementsStateFile() const;
    std::string computeRequirementsSignature() const;
    void persistRequirementsSignature(const std::string& signature) const;
    void parseMainSection(const std::string& mainSection);
    void parseRequirementsSection(const std::string& reqSection);
    bool ensureRequirementsInputs(std::filesystem::path& pythonExecutable) const;
    bool isRequirementsStateCurrent(const std::string& signature, const std::filesystem::path& stateFile) const;
    bool runPythonInVirtualEnv(
        const std::filesystem::path& pythonExecutable,
        const std::vector<std::string>& args,
        const std::string& action) const;
    void addRequirementPath(const std::filesystem::path& path);
    void maybeAddRequirementPathFromVersionCheck(const Requirement& req);
#if defined(__linux__)
    struct PackageManagerInfo {
        std::string name;
        std::string availabilityCheck;
        std::string versionQuery;
        std::string versionRegex;
        std::string installCommand;
    };
    static const std::vector<PackageManagerInfo>& getPackageManagers();
    bool isManagerAvailable(const PackageManagerInfo& manager) const;
    std::optional<std::string> queryManagerVersion(const PackageManagerInfo& manager) const;
    bool installViaManager(const PackageManagerInfo& manager) const;
#endif
    static std::vector<std::string> parseCommandArguments(const std::string& args);
    static std::vector<std::pair<std::string, std::string>> parseEnvironmentAssignments(const std::string& envSpec);
};

#endif
