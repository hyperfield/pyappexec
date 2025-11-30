#include "ConfigLoader.hpp"
#include "AppBootstrapper.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>
#include <system_error>

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

    fs::path primary = fs::current_path() / "pyappexec.ini";
    std::error_code ec;
    if (fs::exists(primary, ec)) {
        spdlog::info("Found pyappexec.ini at {}", primary.string());
        return primary;
    }

    throw std::runtime_error(
        "Unable to locate pyappexec.ini in the current directory.\n"
        "Pass --config /path/to/pyappexec.ini to specify the configuration explicitly.");
}

namespace {

std::filesystem::path resolveConfigRoot(const SpecConfig& specConfig,
                                        const std::filesystem::path& configDir)
{
    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string appId = specConfig.get_value(mainSection, "app_id", false);
    if (appId.empty()) {
        appId = "pyappexec";
    }

    std::string configRootValue = specConfig.get_value(mainSection, "config_root", false);
    if (!configRootValue.empty()) {
        std::filesystem::path candidate(configRootValue);
        if (!candidate.is_absolute()) {
            candidate = configDir / candidate;
        }
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(candidate, ec);
        return ec ? candidate.lexically_normal() : normalized;
    }

    return AppBootstrapper::defaultConfigRoot(appId);
}

} // namespace

std::filesystem::path ConfigLoader::guiPreferenceFile(const SpecConfig& specConfig) const
{
    const std::filesystem::path configDir = configPath_.parent_path();
    std::filesystem::path root = resolveConfigRoot(specConfig, configDir);
    if (root.empty()) {
        return configDir / ".pyappexec_gui_pref";
    }
    return root / ".pyappexec_gui_pref";
}

std::string ConfigLoader::determineAppDisplayName(const SpecConfig& specConfig) const
{
    std::string mainSection = AppBootstrapper::getOSPrefix() + ":main";
    std::string appNameValue = specConfig.get_value(mainSection, "app_name", false);
    std::string appDirValue = specConfig.get_value(mainSection, "python_app_dir", false);
    std::filesystem::path baseDir = configPath_.parent_path();

    auto trim = [](std::string s) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    };

    if (!appNameValue.empty()) {
        appNameValue = trim(appNameValue);
        if (!appNameValue.empty()) {
            return appNameValue;
        }
    }

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
