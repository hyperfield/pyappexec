#ifndef CONFIGLOADER_HPP
#define CONFIGLOADER_HPP

#include "CliParser.hpp"
#include "SpecConfig.hpp"
#include <filesystem>

class ConfigLoader
{
public:
    explicit ConfigLoader(const CliOptions& options);

    SpecConfig load();
    const std::filesystem::path& configPath() const { return configPath_; }
    std::filesystem::path guiPreferenceFile() const;
    std::string determineAppDisplayName(const SpecConfig& specConfig) const;

private:
    std::filesystem::path resolveConfigPath(const std::optional<std::string>& overridePath) const;

    CliOptions options_;
    std::filesystem::path configPath_;
};

#endif
