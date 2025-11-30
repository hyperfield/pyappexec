#ifndef LAUNCHERCLI_HPP
#define LAUNCHERCLI_HPP

#include "SpecConfig.hpp"
#include "ConfigLoader.hpp"
#include "AppBootstrapper.hpp"

class LauncherCli
{
public:
    int run(SpecConfig& specConfig, const ConfigLoader& loader) const;

private:
    bool ensurePythonReady(AppBootstrapper& appBootstrapper) const;
};

#endif
