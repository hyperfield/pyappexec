#include "installer/ProjectInspector.hpp"

#include <QDir>
#include <QDirIterator>
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

    // Derive a likely package name so we can probe common locations first.
    QString packageName = result.suggestedAppName;
    packageName.replace('-', '_');

    QStringList preferredCandidates;
    if (!packageName.isEmpty()) {
        preferredCandidates << QStringLiteral("src/%1/__main__.py").arg(packageName);
        preferredCandidates << QStringLiteral("%1/__main__.py").arg(packageName);
        preferredCandidates << QStringLiteral("src/%1/main.py").arg(packageName);
        preferredCandidates << QStringLiteral("%1/main.py").arg(packageName);
    }
    preferredCandidates << QStringLiteral("__main__.py");
    preferredCandidates << QStringLiteral("main.py");

    for (const QString& candidate : preferredCandidates) {
        if (dir.exists(candidate)) {
            result.suggestedExecPath = candidate;
            break;
        }
    }

    if (result.suggestedExecPath.isEmpty()) {
        // As a fallback, search recursively for an entry point.
        QDirIterator it(projectRoot, QStringList() << "__main__.py", QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            const QString path = it.next();
            result.suggestedExecPath = dir.relativeFilePath(path);
        } else {
            QDirIterator mainIt(projectRoot, QStringList() << "main.py", QDir::Files, QDirIterator::Subdirectories);
            if (mainIt.hasNext()) {
                const QString path = mainIt.next();
                result.suggestedExecPath = dir.relativeFilePath(path);
            }
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
