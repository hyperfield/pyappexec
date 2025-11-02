#ifndef SPECCONFIG_HPP
#define SPECCONFIG_HPP

#include <filesystem>
#include <iostream>
#include "INIReader.h"


class SpecConfig
{
public:
    explicit SpecConfig(const std::string& filename);
    std::string get_value(const std::string& section, const std::string& key, bool required = false) const;
    std::filesystem::path getConfigDir() const;

private:
    INIReader reader;
    std::filesystem::path configDir;
};

#endif
