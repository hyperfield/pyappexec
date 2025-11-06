#include "installer/ProjectInspector.hpp"

#include <QDir>
#include <QFileInfo>
#include <QObject>

namespace installer {

InspectionResult ProjectInspector::inspect(const QString& projectRoot) const
{
    InspectionResult result;
    QDir dir(projectRoot);
    if (!dir.exists()) {
        result.notes << QObject::tr("Selected directory does not exist.");
        return result;
    }

    result.success = true;
    result.suggestedAppName = dir.dirName();

    if (dir.exists("src/yt_channel_downloader/__main__.py")) {
        result.suggestedExecPath = QStringLiteral("src/yt_channel_downloader/__main__.py");
    } else if (dir.exists("__main__.py")) {
        result.suggestedExecPath = QStringLiteral("__main__.py");
    } else {
        QFileInfoList mains = dir.entryInfoList({"**/__main__.py"}, QDir::Files | QDir::NoSymLinks, QDir::Name);
        if (!mains.isEmpty()) {
            result.suggestedExecPath = dir.relativeFilePath(mains.first().filePath());
        }
    }

    if (result.suggestedExecPath.isEmpty()) {
        result.notes << QObject::tr("Could not automatically locate an entry point. Please set it manually.");
    }

    if (!dir.exists("requirements.txt")) {
        result.notes << QObject::tr("requirements.txt not found; INI will be generated without dependencies.");
    }

    return result;
}

} // namespace installer
