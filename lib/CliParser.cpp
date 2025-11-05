#include "CliParser.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

CliParser::CliParser(int argc, char** argv) : argc_(argc), argv_(argv) {}

CliOptions CliParser::parse() const
{
    CliOptions options;
    options.forwardedArgs.reserve(argc_ > 1 ? argc_ - 1 : 0);

    for (int i = 1; i < argc_; ++i) {
        std::string_view arg(argv_[i]);
        if (arg == "--no-gui") {
            options.forceCli = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            exitWithHelp();
        }
        if (arg == "--reset-gui") {
            options.resetGuiPreference = true;
            continue;
        }
        if (arg.rfind("--config", 0) == 0) {
            std::string value;
            if (arg == "--config") {
                if (i + 1 >= argc_) {
                    throw std::runtime_error("--config requires a path argument.");
                }
                value = argv_[++i];
            } else {
                auto pos = arg.find('=');
                if (pos == std::string_view::npos || pos + 1 == arg.size()) {
                    throw std::runtime_error("Use --config /path/to/pyappexec.ini or --config=/path/to/pyappexec.ini");
                }
                value = std::string(arg.substr(pos + 1));
            }
            options.configOverride = value;
            continue;
        }
        options.forwardedArgs.emplace_back(argv_[i]);
    }

    return options;
}

void CliParser::printHelp() const
{
    std::cout << "PyAppExec - Cross-platform Python bootstrapper\n"
              << "Usage: " << argv_[0] << " [options]\n\n"
              << "Options:\n"
              << "  --config <path>     Use a specific pyappexec.ini\n"
              << "  --no-gui            Force CLI mode even if GUI is enabled in the INI\n"
              << "  --reset-gui         Clear the saved \"hide GUI\" preference for this app\n"
              << "  --help              Show this message and exit\n\n"
              << "PyAppExec prepares Python interpreters, virtual environments, and external tools\n"
                 "so end users can run your packaged application without manual setup.\n"
                 "Project: https://github.com/quicknode-labs/PyAppExec\n";
}

void CliParser::exitWithHelp() const
{
    printHelp();
    std::exit(0);
}
