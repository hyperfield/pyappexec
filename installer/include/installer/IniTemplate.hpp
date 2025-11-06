#ifndef INSTALLER_INITEMPLATE_HPP
#define INSTALLER_INITEMPLATE_HPP

#include "installer/ProjectInspector.hpp"
#include "installer/SettingsModel.hpp"

#include <QString>

namespace installer {

class IniTemplate
{
public:
    QString generate(const SettingsModel& settings, const InspectionResult& inspection) const;
};

} // namespace installer

#endif
