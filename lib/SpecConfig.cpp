#include "INIReader.h"
#include "SpecConfig.hpp"
#include <string>


SpecConfig::SpecConfig(const std::string& filename) : reader(filename)
{
    if (reader.ParseError() != 0) {
        throw std::runtime_error("Failed to load config file: " + filename);
    }
}


std::string SpecConfig::get_value(const std::string& section, const std::string& key, bool required) const
{
    std::string value = reader.Get(section, key, "");

    if (required && value.empty()) {
        throw std::runtime_error("Missing required config key: [" + section + "] " + key);
    }

    return value;
}