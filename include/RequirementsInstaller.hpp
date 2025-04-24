#ifndef REQUIREMENTSINSTALLER_HPP
#define REQUIREMENTSINSTALLER_HPP

#include "SpecConfig.hpp"


class RequirementsInstaller {
public:
    explicit RequirementsInstaller(SpecConfig specConfig);
    bool installDependencies();

private:
    SpecConfig specConfig;
};

#endif