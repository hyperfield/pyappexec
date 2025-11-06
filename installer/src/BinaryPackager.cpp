#include "installer/BinaryPackager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QObject>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace installer {

namespace {
QString launcherSourcePath()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
    const QString exeCandidate = appDir.filePath(QStringLiteral("pyappexec.exe"));
    if (QFileInfo::exists(exeCandidate)) {
        return exeCandidate;
    }
#endif
    const QString generic = appDir.filePath(QStringLiteral("pyappexec"));
    if (QFileInfo::exists(generic)) {
        return generic;
    }
    return {};
}
}

bool BinaryPackager::install(const SettingsModel& settings,
                             const QString& iniContents,
                             QString* createdIniPath,
                             QString* errorMessage) const
{
    QDir targetDir(settings.projectPath);
    if (!targetDir.exists()) {
        if (!targetDir.mkpath(QStringLiteral("."))) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Unable to create target directory.");
            }
            return false;
        }
    }

    const QString iniPath = targetDir.filePath(QStringLiteral("pyappexec.ini"));
    QFile iniFile(iniPath);
    if (!iniFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to write %1").arg(iniPath);
        }
        return false;
    }
    QTextStream stream(&iniFile);
    stream << iniContents;
    iniFile.close();

    if (createdIniPath) {
        *createdIniPath = iniPath;
    }

    const QString sourceLauncher = launcherSourcePath();
    if (sourceLauncher.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not locate pyappexec binary near the installer.");
        }
        return false;
    }

    const QString destLauncher = targetDir.filePath(settings.executableFileName());
    if (QFile::exists(destLauncher)) {
        QFile::remove(destLauncher);
    }
    if (!QFile::copy(sourceLauncher, destLauncher)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to copy launcher binary to %1").arg(destLauncher);
        }
        return false;
    }

#ifdef Q_OS_UNIX
    QFile::setPermissions(destLauncher, QFile::permissions(destLauncher) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    return true;
}

} // namespace installer
