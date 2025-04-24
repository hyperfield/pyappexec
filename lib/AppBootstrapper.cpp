#include "AppBootstrapper.hpp"
#include "SpecConfig.hpp"
#include "Utils.hpp"


AppBootstrapper::AppBootstrapper(const SpecConfig& specConfig) :
    specConfig(specConfig)
{
    parseConfig();
    PythonManager python;
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

    python_download_url = specConfig.get_value(mainSection, "python_download_url", true);
    python_min_ver = specConfig.get_value(mainSection, "python_min_ver", true);
    exec_app_path = specConfig.get_value(mainSection, "exec_app_path", true);
    requirements_file = specConfig.get_value(mainSection, "requirements_file", false);

    requirements.emplace_back(Requirement{"python", python_download_url, Utils::getFileNameFromUrl(python_download_url)});

    int index = 1;
    while (true) {
        std::string key_name = "requirement_" + std::to_string(index);
        std::string key_url = "requirement_" + std::to_string(index) + "_url";
        std::string key_file_name = "requirement_" + std::to_string(index) + "_file_name";

        std::string req_name = specConfig.get_value(reqSection, key_name, false);
        std::string req_url = specConfig.get_value(reqSection, key_url, false);
        std::string req_file_name = specConfig.get_value(reqSection, key_file_name, false);

        if (req_name.empty() && req_url.empty()) {
            break;
        }

        requirements.emplace_back(Requirement{req_name, req_url, req_file_name});
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

    return { PythonSetupStatus::Status::SUCCESS, "Python is installed and up to date.\n" };
}


PythonSetupStatus::Status AppBootstrapper::getPythonSetupStatus()
{
    PythonSetupStatus status = checkPythonSetup();

    switch (status.status) {
        case PythonSetupStatus::Status::NOT_FOUND:
            std::cout << status.message << std::endl;
            break;
        case PythonSetupStatus::Status::UPDATE_REQUIRED:
            std::cerr << status.message << std::endl;
            break;
        case PythonSetupStatus::Status::SUCCESS:
            std::cout << status.message << std::endl;
            break;
    }
    
    return status.status;
}


bool AppBootstrapper::installRequirements()
{
    for (const auto& req : requirements) {
        std::cout << "Installing: " << req.name << " from " << req.url << std::endl;

        std::string command;
        
        if (Utils::isWindows()) {
            command = req.url + " " + req.cmd_params;
        } else if (Utils::isLinux()) {
            command = "wget " + req.url + " -O " + req.file_name;
        } else if (Utils::isMacOS()) {
            command = "curl -L " + req.url + " -o " + req.file_name;
        }
        
        std::cout << "Command: " << command << std::endl;

        std::cout << "Running command: " << command << std::endl;
        int result = system(command.c_str());
        if (result != 0) {
            std::cerr << "Failed to install: " << req.name << std::endl;
            return false;
        }
        
        std::cout << std::endl;
    }
    return true;
}


bool AppBootstrapper::downloadRequirements()
{
    if (!Utils::ensureDirExists("distrib/")) {
        std::cerr << "Failed to create 'distrib/' directory." << std::endl;
        return false;
    }

    bool allDownloadsSuccessful = true;

    for (const auto& req : requirements) {
        std::string fileName = req.file_name.empty() ? Utils::getFileNameFromUrl(req.url) : req.file_name;
        std::string req_path = "distrib/" + fileName;
    
        if (!Utils::isFileComplete(req_path, req.url)) {
            std::cout << "Downloading: " << req.name << " from " << req.url << std::endl;
            if (!Utils::downloadFile(req.url, req_path)) {
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