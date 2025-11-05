#ifndef PYTHONMANAGER_HPP
#define PYTHONMANAGER_HPP

#include <string>


class PythonManager
{

public:
    bool isPythonInstalled();
    std::string getPythonVersion();
    std::string getResolvedPythonCommand();
    bool isPythonVersionAtLeast(const std::string &installed, const std::string &required);
    bool isPythonInstalledAndMeetsVersion(const std::string &requiredVersion);

private:
    std::string resolvePythonCommand();
    static std::string trimVersionString(const std::string& rawVersion);

    std::string cachedPythonCommand;
    // cppcheck-suppress unusedPrivateField
    std::string cachedPythonVersion;
};

#endif
