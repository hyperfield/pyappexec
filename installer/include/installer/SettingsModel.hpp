#ifndef INSTALLER_SETTINGSMODEL_HPP
#define INSTALLER_SETTINGSMODEL_HPP

#include <QString>

namespace installer {

struct SettingsModel
{
    QString projectPath;
    QString appName;
    QString executableName;
    QString iconPath;
    QString appId;
    bool hideGuiAfterSuccess{false};

    QString executableFileName() const;
    QString launcherArtifactName() const;
#ifdef Q_OS_MAC
    QString bundleName() const;
#endif

private:
    QString normalizedExecutableStem() const;
};

} // namespace installer

#endif
