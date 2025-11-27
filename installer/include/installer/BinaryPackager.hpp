#ifndef INSTALLER_BINARYPACKAGER_HPP
#define INSTALLER_BINARYPACKAGER_HPP

#include "installer/SettingsModel.hpp"

#include <QString>

namespace installer {

class BinaryPackager
{
public:
    bool install(const SettingsModel& settings,
                 const QString& iniContents,
                 QString* createdIniPath,
                 QString* errorMessage,
                 bool createBundle,
                 bool writeIni) const;
    bool uninstall(const SettingsModel& settings, QString* errorMessage) const;
};

} // namespace installer

#endif
