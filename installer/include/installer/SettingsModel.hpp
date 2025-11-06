#ifndef INSTALLER_SETTINGSMODEL_HPP
#define INSTALLER_SETTINGSMODEL_HPP

#include <QString>

namespace installer {

struct SettingsModel
{
    QString projectPath;
    QString appName;
    QString executableName;
    bool hideGuiAfterSuccess{false};

    QString executableFileName() const;
};

} // namespace installer

#endif
