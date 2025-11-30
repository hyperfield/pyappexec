#include "installer/IniTemplate.hpp"

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace installer {

namespace {

QString sanitizeAppId(const QString& candidate)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9]{6,20}$"));
    if (re.match(candidate).hasMatch()) {
        return candidate;
    }
    static const QString alphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ0123456789");
    const int length = 10;
    QString id;
    id.reserve(length);
    for (int i = 0; i < length; ++i) {
        const int idx = QRandomGenerator::global()->bounded(alphabet.size());
        id.append(alphabet.at(idx));
    }
    return id;
}

} // namespace

QString IniTemplate::generate(const SettingsModel& settings, const InspectionResult& inspection) const
{
    const QString requirements = QStringLiteral("requirements.txt");
    const QString execPath = inspection.suggestedExecPath.isEmpty()
        ? QStringLiteral("__main__.py")
        : inspection.suggestedExecPath;
    const QString appId = sanitizeAppId(settings.appId.trimmed());
    const QString appName = settings.appName.trimmed().isEmpty()
        ? QFileInfo(settings.projectPath).fileName()
        : settings.appName.trimmed();

    auto platformBlock = [&](const QString& os) {
        return QStringLiteral(
            "[%1:main]\n"
            "python_download_url = https://www.python.org/ftp/python/3.13.1/Python-3.13.1.tgz\n"
            "python_min_ver = 3.10\n"
            "app_id = %2\n"
            "app_name = %3\n"
            "config_root = \n"
            "python_app_dir = .\n"
            "exec_app_path = %4\n"
            "requirements_file = %5\n"
            "virtual_env_dir = \n"
            "GUI = true\n"
            "GUI_CLI_HIDE_AFTER_SUCCESS = %6\n"
            "log_console = true\n"
            "log_level = info\n\n")
            .arg(os)
            .arg(appId)
            .arg(appName)
            .arg(execPath)
            .arg(requirements)
            .arg(settings.hideGuiAfterSuccess ? QStringLiteral("true") : QStringLiteral("false"));
    };

    const QString requirementStub = QStringLiteral(
        "requirement_1 = \n"
        "requirement_1_url = \n"
        "requirement_1_file_name = \n"
        "requirement_1_cmd_params = \n"
        "requirement_1_version_check_command = \n"
        "requirement_1_version_regex = \n"
        "requirement_1_min_version = \n"
        "requirement_1_launch_file = \n"
        "requirement_1_capture_stderr = \n"
        "requirement_1_install_command = \n"
        "requirement_1_append_to_path = \n"
        "requirement_1_standalone = \n"
        "requirement_1_install_dir = \n");

    QString ini;
    ini += platformBlock(QStringLiteral("Linux"));
    ini += QStringLiteral("[Linux:requirements]\n") + requirementStub + QStringLiteral("\n");
    ini += platformBlock(QStringLiteral("Windows"));
    ini += QStringLiteral("[Windows:requirements]\n") + requirementStub + QStringLiteral("\n");
    ini += platformBlock(QStringLiteral("MacOS"));
    ini += QStringLiteral("[MacOS:requirements]\n") + requirementStub + QStringLiteral("\n");
    return ini;
}

} // namespace installer
