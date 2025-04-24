#ifndef UTILS_HPP
#define UTILS_HPP

#include <Utils.hpp>
#include <string>
#include <vector>


class Utils
{
public:
    static std::string runPythonScriptFromResourceAndCaptureOutput(const std::string &script_name, const std::string &python_cmd);
    static std::vector<char> extractResource(const std::string &resource_name);
    static bool ensureDirExists(const std::string &directoryPath);
    static bool fileExists(const std::string &fullPath);
    static std::string getTempPath();
    static bool downloadFile(const std::string& url, const std::string& outputDirectory);
    static bool isFileComplete(const std::string& filePath, const std::string& fileUrl);
    static size_t getRemoteFileSize(const std::string &url);
    static std::string getFileNameFromUrl(const std::string &url);
    static std::string runAndCaptureOutput(const std::string &cmd, bool captureStderr);
    static std::string extractVersion(const std::string &output, const std::string &regexPattern);
    static bool isVersionAtLeast(const std::string& current, const std::string& required);
    static bool isWindows();
    static bool isLinux();
    static bool isMacOS();
};

#endif