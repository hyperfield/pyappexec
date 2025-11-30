#ifndef CLIPARSER_HPP
#define CLIPARSER_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct CliOptions
{
    bool forceCli{false};
    bool resetGuiPreference{false};
    bool showVersion{false};
    std::optional<std::string> configOverride;
    std::vector<std::string> forwardedArgs;
};

class CliParser
{
public:
    CliParser(int argc, char** argv);

    CliOptions parse() const;

private:
    void printHelp() const;
    [[noreturn]] void exitWithHelp() const;

    int argc_;
    char** argv_;
};

#endif
