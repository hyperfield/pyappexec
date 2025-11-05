#ifndef LOGGERCONFIGURATOR_HPP
#define LOGGERCONFIGURATOR_HPP

#include "SpecConfig.hpp"

class LoggerConfigurator
{
public:
    explicit LoggerConfigurator(const SpecConfig& specConfig);
    void apply() const;

private:
    const SpecConfig& specConfig_;
};

#endif
