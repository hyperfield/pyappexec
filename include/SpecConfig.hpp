#ifndef SPECCONFIG_HPP
#define SPECCONFIG_HPP

#include <iostream>
#include "INIReader.h"


class SpecConfig
{
public:
    explicit SpecConfig(const std::string& filename);
    std::string get_value(const std::string& section, const std::string& key, bool required = false) const;

private:
    INIReader reader;
};

#endif