#ifndef GUIRUNNER_HPP
#define GUIRUNNER_HPP

#include <string>
// cppcheck-suppress missingIncludeSystem
#include <vector>

int runGuiApplication(int argc, char** argv, const std::vector<std::string>& forwardedArgs, const std::string& guiStatePath, const std::string& appDisplayName);

#endif
