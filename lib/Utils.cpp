#include "Utils.hpp"
#include <array>
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1.hpp>
// cppcheck-suppress missingIncludeSystem
#include <filesystem>
// cppcheck-suppress missingIncludeSystem
#include <gio/gio.h>
#include <memory>
#include "resources.h"
#include <regex>
#include <sstream>
#include <string>
#include <iostream>
// cppcheck-suppress missingIncludeSystem
#include <vector>
// cppcheck-suppress missingIncludeSystem
#include <spdlog/spdlog.h>

#ifdef _WIN32
    // cppcheck-suppress missingIncludeSystem
    #include <windows.h>
    #include <urlmon.h>
    #pragma comment(lib, "urlmon.lib")
#endif

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace fs = std::filesystem;
namespace bp = boost::process::v1;


std::string Utils::runPythonScriptFromResourceAndCaptureOutput(const std::string& script_name, const std::string& python_cmd)
{
    std::vector<char> script = extractResource(script_name);
    if (script.empty()) {
        throw std::runtime_error("Failed to load embedded Python script: " + script_name);
    }

    bp::ipstream outStream;
    bp::opstream inStream;

    bp::child process(
        python_cmd,
        bp::std_out > outStream,
        bp::std_in < inStream,
        bp::std_err > bp::null
    );

    inStream.write(script.data(), script.size());
    inStream.flush();
    inStream.pipe().close();

    std::ostringstream output;
    std::string line;
    while (std::getline(outStream, line)) {
        output << line << '\n';
    }

    process.wait();
    if (process.exit_code() != 0) {
        throw std::runtime_error("Python script execution failed with exit code: " + std::to_string(process.exit_code()));
    }

    return output.str();
}


std::vector<char> Utils::extractResource(const std::string& resource_name)
{
    extern GResource *resources_get_resource(void);
    resources_get_resource();

    std::string resource_path = "/net/quicknode/pyappexec/scripts/" + resource_name;
    GBytes* resource_bytes = g_resources_lookup_data(resource_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);

    if (!resource_bytes) return {};

    gsize size;
    const char* data = (const char*) g_bytes_get_data(resource_bytes, &size);

    std::vector<char> binary_data(data, data + size);
    g_bytes_unref(resource_bytes);

    return binary_data;
}


bool Utils::ensureDirExists(const std::string& directoryPath)
{
    if (!fs::exists(directoryPath)) {
        return fs::create_directories(directoryPath);
    }

    return true; 
}


bool Utils::fileExists(const std::string& fullPath)
{
    return fs::exists(fullPath);
}


std::string Utils::getTempPath()
{
#ifdef _WIN32
    char* temp_path = std::getenv("TEMP");
    return temp_path ? std::string(temp_path) + "\\" : "C:\\Windows\\Temp\\";
#else
    return "/tmp/";
#endif
}


std::string extractFileNameFromURL(const std::string& url)
{
    size_t lastSlash = url.find_last_of("/");
    if (lastSlash != std::string::npos && lastSlash + 1 < url.size()) {
        return url.substr(lastSlash + 1);
    }
    return "no_flie_downloaded";
}


bool Utils::downloadFile(const std::string& url, const std::string& fullOutputPath)
{
    #ifdef _WIN32
        if (fs::exists(fullOutputPath)) {
            spdlog::info("File already exists: {}", fullOutputPath);
            return true;
        }

        HRESULT result = URLDownloadToFileA(NULL, url.c_str(), fullOutputPath.c_str(), 0, NULL);
        if (result == S_OK) {
            spdlog::info("Download succeeded: {}", fullOutputPath);
            return true;
        } else {
            spdlog::error("Download failed with HRESULT: {}", result);
            return false;
        }

    #else
        std::string command = "curl -C - -L -o \"" + fullOutputPath + "\" \"" + url + "\"";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return false;
        int status = pclose(pipe);
        return status == 0;
    #endif
}


bool Utils::isFileComplete(const std::string& filePath, const std::string& fileUrl) {
    if (!fs::exists(filePath)) return false;

    size_t remoteSize = Utils::getRemoteFileSize(fileUrl);
    if (remoteSize == 0) return false;

    return fs::file_size(filePath) == remoteSize;
}


size_t Utils::getRemoteFileSize(const std::string& url) {
    #ifdef _WIN32
        HINTERNET hInternet = InternetOpenA("FileSizeChecker", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            spdlog::error("InternetOpenA failed while checking {}", url);
            return 0;
        }

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
        if (!hUrl) {
            spdlog::error("InternetOpenUrlA failed for {}", url);
            InternetCloseHandle(hInternet);
            return 0;
        }

        DWORD fileSize = 0;
        DWORD fileSizeSize = sizeof(fileSize);

        if (!HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &fileSize, &fileSizeSize, NULL)) {
            spdlog::error("HttpQueryInfoA failed for {}", url);
            fileSize = 0;
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        return fileSize;

    #else
        std::string command = "curl -sI " + url + " | grep -i Content-Length | tr -d '\\r' | awk '{print $2}'";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return 0;

        char buffer[128];
        size_t fileSize = 0;
        
        if (fgets(buffer, sizeof(buffer), pipe)) {
            try {
                fileSize = std::stoull(std::string(buffer));
            } catch (const std::exception&) {
                fileSize = 0;
            }
        }
        pclose(pipe);
        return fileSize;
    #endif
}


std::string Utils::getFileNameFromUrl(const std::string& url) {
    size_t lastSlash = url.find_last_of('/');
    if (lastSlash == std::string::npos) return "";
    return url.substr(lastSlash + 1);
}


std::string Utils::runAndCaptureOutput(const std::string& cmd, bool captureStderr = false) {
    std::ostringstream output;
    bp::ipstream outStream;

    bp::child process = captureStderr
        ? bp::child(cmd, bp::std_out > outStream, bp::std_err > outStream)
        : bp::child(cmd, bp::std_out > outStream, bp::std_err > bp::null);

    std::string line;
    while (std::getline(outStream, line)) {
        output << line << "\n";
    }

    process.wait();
    if (process.exit_code() != 0) {
        throw std::runtime_error("Command failed with exit code: " + std::to_string(process.exit_code()));
    }

    return output.str();
}


std::string Utils::extractVersion(const std::string& output, const std::string& regexPattern)
{
    std::regex versionRegex(regexPattern);
    std::smatch match;
    if (std::regex_search(output, match, versionRegex)) {
        return match.str(1);
    }
    return "";
}


bool Utils::isVersionAtLeast(const std::string& current, const std::string& required)
{
    auto parseVersion = [](const std::string& v) {
        std::istringstream stream(v);
        int major = 0, minor = 0, patch = 0;
        char dot;
        stream >> major >> dot >> minor;
        if (!(stream >> dot >> patch)) patch = 0;
        return std::tuple{major, minor, patch};
    };

    return parseVersion(current) >= parseVersion(required);
}


bool Utils::isWindows() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}


bool Utils::isLinux() {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

   
bool Utils::isMacOS() {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}
