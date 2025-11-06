#include "ConfigLoader.hpp"
#include "AppBootstrapper.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

ConfigLoader::ConfigLoader(const CliOptions& options, const std::filesystem::path& binaryDir)
    : options_(options), binaryDir_(binaryDir) {}

SpecConfig ConfigLoader::load()
{
    configPath_ = resolveConfigPath(options_.configOverride);
    spdlog::info("Using configuration file: {}", configPath_.string());
    return SpecConfig(configPath_.string());
}

std::filesystem::path ConfigLoader::resolveConfigPath(const std::optional<std::string>& overridePath) const
{
    namespace fs = std::filesystem;

    if (overridePath) {
        fs::path explicitPath = fs::path(*overridePath);
        if (!explicitPath.is_absolute()) {
            explicitPath = fs::current_path() / explicitPath;
        }
        if (!fs::exists(explicitPath)) {
            throw std::runtime_error("Specified config file not found: " + explicitPath.string());
        }
        return explicitPath;
    }

    auto searchDir = [](const fs::path& dir) -> std::optional<fs::path> {
        fs::path primary = dir / "pyappexec.ini";
        if (fs::exists(primary)) {
            return primary;
        }
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_directory()) {
                continue;
            }
            fs::path candidate = entry.path() / "pyappexec.ini";
            if (fs::exists(candidate)) {
                return candidate;
            }
        }
        return std::nullopt;
    };

    if (auto fromCwd = searchDir(fs::current_path())) {
        return *fromCwd;
    }

    if (!binaryDir_.empty()) {
        if (auto fromBinary = searchDir(binaryDir_)) {
            return *fromBinary;
        }
    }

    throw std::runtime_error(
        "Unable to locate pyappexec.ini in the current directory or its immediate subdirectories.\n"
        "Pass --config /path/to/pyappexec.ini to specify the configuration explicitly.");
}

std::filesystem::path ConfigLoader::guiPreferenceFile() const
{
    return configPath_.parent_path() / ".pyappexec_gui_pref";
}

std::string ConfigLoader::determineAppDisplayName(const SpecConfig& specConfig) const
{
    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string appDirValue = specConfig.get_value(mainSection, "python_app_dir", false);
    std::filesystem::path baseDir = configPath_.parent_path();

    auto fallbackName = [&]() {
        std::string name = baseDir.filename().string();
        if (name.empty()) {
            name = baseDir.parent_path().filename().string();
        }
        if (name.empty()) {
            name = "PyAppExec";
        }
        return name;
    };

    if (!appDirValue.empty()) {
        std::filesystem::path resolved(appDirValue);
        if (resolved.is_relative()) {
            resolved = baseDir / resolved;
        }
        std::string candidate = resolved.filename().string();
        if (!candidate.empty() && candidate != "." && candidate != "..") {
            return candidate;
        }
    }

    return fallbackName();
}

std::filesystem::path ConfigLoader::resolveAppRelativePath(const std::filesystem::path& pythonAppDir,
                                                           const std::string& entry) const
{
    if (entry.empty()) {
        return {};
    }

    std::filesystem::path path(entry);
    if (path.is_absolute()) {
        return path;
    }

    return pythonAppDir / path;
}
