#ifndef APPMETADATA_HPP
#define APPMETADATA_HPP

#include <string_view>
#include "app_version.hpp"

namespace AppMetadata {

inline constexpr std::string_view kAppName = "PyAppExec";
inline constexpr std::string_view kVersion = AppVersion::kAppVersion;
inline constexpr std::string_view kAuthor = "hyperfield";
inline constexpr std::string_view kLicense = "MIT";
inline constexpr std::string_view kGithub = "https://github.com/hyperfield/pyappexec";
inline constexpr std::string_view kYears = "2025";

}

#endif
