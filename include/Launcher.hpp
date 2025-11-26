#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP

#include "CliParser.hpp"
#include "ConfigLoader.hpp"
#include "SpecConfig.hpp"
#include "AppBootstrapper.hpp"

class Launcher
{
public:
    int run(const CliOptions& options,
            SpecConfig& specConfig,
            const ConfigLoader& loader,
            int argc,
            char** argv) const;

private:
    bool ensurePythonReady(AppBootstrapper& appBootstrapper) const;
    int runCli(AppBootstrapper& appBootstrapper) const;
};

#endif
