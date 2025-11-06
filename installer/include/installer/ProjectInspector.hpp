#ifndef INSTALLER_PROJECTINSPECTOR_HPP
#define INSTALLER_PROJECTINSPECTOR_HPP

#include <QString>
#include <QStringList>

namespace installer {

struct InspectionResult
{
    bool success{false};
    QString suggestedAppName;
    QString suggestedExecPath;
    QStringList notes;
};

class ProjectInspector
{
public:
    InspectionResult inspect(const QString& projectRoot) const;
};

} // namespace installer

#endif
