#include "installer/SettingsModel.hpp"

namespace installer {

QString SettingsModel::normalizedExecutableStem() const
{
    QString stem = executableName.trimmed();
    if (stem.isEmpty()) {
        stem = QStringLiteral("pyappexec");
    }
#if defined(Q_OS_WIN)
    if (stem.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        stem.chop(4);
    }
#endif
#if defined(Q_OS_MAC)
    if (stem.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
        stem.chop(4);
    }
#endif
    return stem;
}

QString SettingsModel::executableFileName() const
{
    QString base = normalizedExecutableStem();
#if defined(Q_OS_WIN)
    return base + QStringLiteral(".exe");
#else
    return base;
#endif
}

QString SettingsModel::launcherArtifactName() const
{
#if defined(Q_OS_MAC)
    return bundleName();
#else
    return executableFileName();
#endif
}

#if defined(Q_OS_MAC)
QString SettingsModel::bundleName() const
{
    QString base = normalizedExecutableStem();
    return base + QStringLiteral(".app");
}
#endif

} // namespace installer
