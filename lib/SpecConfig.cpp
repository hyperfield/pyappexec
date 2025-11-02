#include "INIReader.h"
#include "SpecConfig.hpp"
#include <filesystem>
#include <string>


SpecConfig::SpecConfig(const std::string& filename) : reader(filename)
{
    if (reader.ParseError() != 0) {
        throw std::runtime_error("Failed to load config file: " + filename);
    }

    try {
        configDir = std::filesystem::absolute(filename).parent_path();
    } catch (const std::exception&) {
        configDir.clear();
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


std::filesystem::path SpecConfig::getConfigDir() const
{
    if (!configDir.empty()) {
        return configDir;
    }

    return std::filesystem::current_path();
}
