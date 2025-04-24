#ifndef APPBOOTSTRAPPER_HPP
#define APPBOOTSTRAPPER_HPP

#include "PythonManager.hpp"
#include "SpecConfig.hpp"
#include <vector>


struct Requirement {
    std::string name;
    std::string url;
    std::string file_name;
    std::string cmd_params;
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

    std::string getPythonDownloadUrl();
    std::vector<Requirement> getRequirements();
    bool isRequirementAlreadyInstalled(const Requirement& req);

private:
    PythonManager python;
    SpecConfig specConfig;

    std::string python_download_url;
    std::string python_min_ver;
    std::string exec_app_path;
    std::string requirements_file;

    std::vector<Requirement> requirements;
    
    void parseConfig();
    std::string getOSPrefix();
};

#endif