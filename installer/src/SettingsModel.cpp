#include "installer/SettingsModel.hpp"

#ifdef Q_OS_WIN
static constexpr char kExecutableSuffix[] = ".exe";
#else
static constexpr char kExecutableSuffix[] = "";
#endif

namespace installer {

QString SettingsModel::executableFileName() const
{
    QString base = executableName.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("pyappexec");
    }
    if (!base.endsWith(QString::fromLatin1(kExecutableSuffix), Qt::CaseInsensitive)) {
        base += QString::fromLatin1(kExecutableSuffix);
    }
    return base;
}

} // namespace installer
