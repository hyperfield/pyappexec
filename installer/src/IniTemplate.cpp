#include "installer/IniTemplate.hpp"

#include <QString>
#include <QStringList>

namespace installer {

QString IniTemplate::generate(const SettingsModel& settings, const InspectionResult& inspection) const
{
    const QString requirements = QStringLiteral("requirements.txt");
    const QString execPath = inspection.suggestedExecPath.isEmpty()
        ? QStringLiteral("__main__.py")
        : inspection.suggestedExecPath;

    auto platformBlock = [&](const QString& os) {
        return QStringLiteral(
            "[%1:main]\n"
            "python_download_url = https://www.python.org/ftp/python/3.13.1/Python-3.13.1.tgz\n"
            "python_min_ver = 3.10\n"
            "python_app_dir = .\n"
            "exec_app_path = %2\n"
            "requirements_file = %3\n"
            "virtual_env_dir = .pyappexec-venv\n"
            "GUI = %4\n"
            "log_console = true\n"
            "log_level = info\n\n")
            .arg(os)
            .arg(execPath)
            .arg(requirements)
            .arg(settings.hideGuiAfterSuccess ? QStringLiteral("false") : QStringLiteral("true"));
    };

    QString ini;
    ini += platformBlock(QStringLiteral("Linux"));
    ini += QStringLiteral("[Linux:requirements]\n\n");
    ini += platformBlock(QStringLiteral("Windows"));
    ini += QStringLiteral("[Windows:requirements]\n\n");
    ini += platformBlock(QStringLiteral("MacOS"));
    ini += QStringLiteral("[MacOS:requirements]\n\n");
    return ini;
}

} // namespace installer
