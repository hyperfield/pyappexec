#ifndef PYTHONMANAGER_HPP
#define PYTHONMANAGER_HPP

#include <string>


class PythonManager
{

public:
    bool isPythonInstalled();
    std::string getPythonVersion();
    bool isPythonVersionAtLeast(const std::string &installed, const std::string &required);
    bool isPythonInstalledAndMeetsVersion(const std::string &requiredVersion);
};

#endif