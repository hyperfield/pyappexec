#include "installer/BinaryPackager.hpp"

#include "AppMetadata.hpp"
#include <array>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QObject>
#include <QIODevice>
#include <QProcess>
#ifdef Q_OS_MAC
#include <QChar>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace installer {

namespace {
bool copyFileOverwrite(const QString& source,
                       const QString& destination,
                       const QString& failureContext,
                       QString* errorMessage)
{
    if (QFile::exists(destination) && !QFile::remove(destination)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to replace %1").arg(destination);
        }
        return false;
    }
    if (!QFile::copy(source, destination)) {
        if (errorMessage) {
            *errorMessage = failureContext.arg(source, destination);
        }
        return false;
    }
    return true;
}

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

#ifdef Q_OS_WIN
QString sanitizeAppId(const QString& appId)
{
    QString cleaned;
    cleaned.reserve(appId.size());
    for (QChar ch : appId) {
        if (ch.isLetterOrNumber()) {
            cleaned.append(ch);
        }
    }
    if (cleaned.isEmpty()) {
        cleaned = QStringLiteral("pyappexec");
    }
    if (cleaned.size() > 40) {
        cleaned.truncate(40);
    }
    return cleaned;
}
#endif

#if !defined(Q_OS_WIN)
bool writeUninstallScript(const QString& scriptPath, const QString& sectionName, QString* errorMessage)
{
    const QString script = QStringLiteral(
        "#!/bin/bash\n"
        "set -euo pipefail\n"
        "SCRIPT_DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n"
        "INI=\"$SCRIPT_DIR/pyappexec.ini\"\n"
        "if [[ ! -f \"$INI\" ]]; then\n"
        "  echo \"pyappexec.ini not found next to this script.\" >&2\n"
        "  exit 1\n"
        "fi\n"
        "python3 - \"$INI\" <<'PY'\n"
        "import configparser\n"
        "import os\n"
        "import pathlib\n"
        "import shutil\n"
        "import sys\n"
        "\n"
        "ini = pathlib.Path(sys.argv[1]).resolve()\n"
        "cfg = configparser.ConfigParser()\n"
        "if not cfg.read(ini):\n"
        "    print(f\"Unable to read {ini}\", file=sys.stderr)\n"
        "    sys.exit(1)\n"
        "section = \"%1\"\n"
        "if section not in cfg:\n"
        "    print(f\"Missing section {section}\", file=sys.stderr)\n"
        "    sys.exit(1)\n"
        "\n"
        "config_dir = ini.parent\n"
        "python_app_dir = cfg.get(section, \"python_app_dir\", fallback=\".\") or \".\"\n"
        "virtual_env_dir = cfg.get(section, \"virtual_env_dir\", fallback=\".venv\") or \".venv\"\n"
        "config_root_val = cfg.get(section, \"config_root\", fallback=\"\")\n"
        "app_id_val = cfg.get(section, \"app_id\", fallback=\"pyappexec\")\n"
        "\n"
        "python_app_path = (config_dir / pathlib.Path(python_app_dir)).resolve()\n"
        "\n"
        "def sanitize(app_id: str) -> str:\n"
        "    cleaned = \"\".join(ch for ch in app_id if ch.isalnum())\n"
        "    cleaned = cleaned[:40]\n"
        "    return cleaned or \"pyappexec\"\n"
        "\n"
        "def default_config_root(app_id: str) -> pathlib.Path:\n"
        "    aid = sanitize(app_id)\n"
        "    home = pathlib.Path(os.environ.get(\"HOME\", \"~\")).expanduser()\n"
        "    if sys.platform == \"darwin\":\n"
        "        return home / \"Library\" / \"Application Support\" / \"PyAppExec\" / aid\n"
        "    if sys.platform.startswith(\"linux\"):\n"
        "        xdg = os.environ.get(\"XDG_DATA_HOME\")\n"
        "        base = pathlib.Path(xdg) if xdg else home / \".local\" / \"share\"\n"
        "        return base / \"pyappexec\" / aid\n"
        "    return home / \".pyappexec\" / aid\n"
        "\n"
        "if config_root_val:\n"
        "    config_root = pathlib.Path(config_root_val)\n"
        "    if not config_root.is_absolute():\n"
        "        config_root = (config_dir / config_root).resolve()\n"
        "else:\n"
        "    config_root = default_config_root(app_id_val)\n"
        "if not virtual_env_dir:\n"
        "    venv_path = (config_root / \"venv\").resolve()\n"
        "else:\n"
        "    venv_path = pathlib.Path(virtual_env_dir)\n"
        "    if not venv_path.is_absolute():\n"
        "        venv_path = (python_app_path / venv_path).resolve()\n"
        "\n"
        "if not venv_path.exists():\n"
        "    print(f\"No virtual environment found at {venv_path}\")\n"
        "    sys.exit(0)\n"
        "\n"
        "if not venv_path.is_dir():\n"
        "    print(f\"{venv_path} exists but is not a directory; refusing to remove.\", file=sys.stderr)\n"
        "    sys.exit(1)\n"
        "\n"
        "print(f\"Removing virtual environment at {venv_path}\")\n"
        "shutil.rmtree(venv_path)\n"
        "print(\"Done.\")\n"
        "PY\n");

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to write uninstaller script at %1").arg(scriptPath);
        }
        return false;
    }
    QTextStream ts(&scriptFile);
    ts << script.arg(sectionName);
    scriptFile.close();
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
    return true;
}
#endif

#if defined(Q_OS_MAC)
QString rewriteIniForBundledMac(const QString& iniContents)
{
    QStringList lines = iniContents.split(QLatin1Char('\n'));
    bool inMacSection = false;
    for (QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            inMacSection = trimmed.compare(QStringLiteral("[MacOS:main]"), Qt::CaseInsensitive) == 0;
        }
        if (inMacSection && trimmed.startsWith(QStringLiteral("python_app_dir"))) {
            line = QStringLiteral("python_app_dir = ../Resources/app");
        }
    }
    return lines.join(QLatin1Char('\n'));
}

bool copyDirectoryRecursive(const QDir& sourceDir,
                            const QString& destPath,
                            const QString& skipPath,
                            QString* errorMessage)
{
    if (!sourceDir.exists()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Source directory %1 does not exist.").arg(sourceDir.absolutePath());
        }
        return false;
    }

    QDir destDir(destPath);
    if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Unable to create destination directory %1").arg(destPath);
        }
        return false;
    }

    const auto entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& info : entries) {
        const QString srcPath = info.absoluteFilePath();
        if (srcPath.startsWith(skipPath)) {
            continue;
        }
        if (info.isDir() && info.fileName().endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
            // Skip existing app bundles to avoid recursive copies/hangs.
            continue;
        }

        const QString relPath = sourceDir.relativeFilePath(srcPath);
        const QString destEntry = destDir.filePath(relPath);

        if (info.isDir()) {
            QDir childDir(info.absoluteFilePath());
            if (!copyDirectoryRecursive(childDir, destEntry, skipPath, errorMessage)) {
                return false;
            }
        } else if (info.isFile()) {
            if (!copyFileOverwrite(srcPath, destEntry,
                                   QObject::tr("Failed to copy %1 to %2"),
                                   errorMessage)) {
                return false;
            }
        }
    }

    return true;
}
#endif

#ifdef Q_OS_MAC
QString sanitizeIdentifierFragment(const QString& value)
{
    QString fragment;
    fragment.reserve(value.size());
    for (QChar ch : value) {
        if (ch.isLetterOrNumber()) {
            fragment.append(ch.toLower());
        }
    }
    if (fragment.isEmpty()) {
        fragment = QStringLiteral("pyappexecapp");
    } else if (!fragment.at(0).isLetter()) {
        fragment.prepend(QStringLiteral("a"));
    }
    return fragment;
}

bool writeInfoPlist(const QString& contentsPath,
                    const SettingsModel& settings,
                    const QString& executableName,
                    const QString& iconFileName,
                    QString* errorMessage)
{
    QString displayName = settings.appName.trimmed();
    if (displayName.isEmpty()) {
        displayName = settings.executableName.trimmed();
    }
    if (displayName.isEmpty()) {
        displayName = QStringLiteral("PyAppExec");
    }

    const QString identifier = QStringLiteral("com.pyappexec.%1")
        .arg(sanitizeIdentifierFragment(displayName));
    const QString version = QString::fromUtf8(AppMetadata::kVersion.data());
    QString iconBlock;
    if (!iconFileName.isEmpty()) {
        iconBlock = QStringLiteral(
            "  <key>CFBundleIconFile</key>\n"
            "  <string>%1</string>\n").arg(iconFileName);
    }

    const QString plist = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "  <key>CFBundleName</key>\n"
        "  <string>%1</string>\n"
        "  <key>CFBundleDisplayName</key>\n"
        "  <string>%1</string>\n"
        "  <key>CFBundleIdentifier</key>\n"
        "  <string>%2</string>\n"
        "  <key>CFBundleExecutable</key>\n"
        "  <string>%3</string>\n"
        "  <key>CFBundlePackageType</key>\n"
        "  <string>APPL</string>\n"
        "  <key>CFBundleVersion</key>\n"
        "  <string>%4</string>\n"
        "  <key>CFBundleShortVersionString</key>\n"
        "  <string>%4</string>\n"
        "  <key>LSMinimumSystemVersion</key>\n"
        "  <string>11.0</string>\n"
        "  <key>NSHighResolutionCapable</key>\n"
        "  <true/>\n"
        "%5"
        "</dict>\n"
        "</plist>\n")
        .arg(displayName, identifier, executableName, version, iconBlock);

    QFile plistFile(contentsPath + QStringLiteral("/Info.plist"));
    if (!plistFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to write Info.plist inside bundle.");
        }
        return false;
    }
    plistFile.write(plist.toUtf8());
    plistFile.close();

    QFile pkgInfo(contentsPath + QStringLiteral("/PkgInfo"));
    if (pkgInfo.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        pkgInfo.write("APPL????");
        pkgInfo.close();
    }

    return true;
}

QString iconutilExecutable()
{
    const QString path = QStandardPaths::findExecutable(QStringLiteral("iconutil"));
    return path;
}

bool generateIcnsFromImage(const QString& sourceImage,
                           const QString& destinationIcns,
                           QString* errorMessage)
{
    QImage image(sourceImage);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Unable to load icon image: %1").arg(sourceImage);
        }
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to create a temporary directory for icon conversion.");
        }
        return false;
    }

    const QString iconsetPath = tempDir.path() + QStringLiteral("/icon.iconset");
    QDir().mkpath(iconsetPath);

    struct IconSpec {
        QString name;
        int size;
    };
    const std::array<IconSpec, 10> specs{{
        {QStringLiteral("icon_16x16.png"), 16},
        {QStringLiteral("icon_16x16@2x.png"), 32},
        {QStringLiteral("icon_32x32.png"), 32},
        {QStringLiteral("icon_32x32@2x.png"), 64},
        {QStringLiteral("icon_128x128.png"), 128},
        {QStringLiteral("icon_128x128@2x.png"), 256},
        {QStringLiteral("icon_256x256.png"), 256},
        {QStringLiteral("icon_256x256@2x.png"), 512},
        {QStringLiteral("icon_512x512.png"), 512},
        {QStringLiteral("icon_512x512@2x.png"), 1024},
    }};

    for (const auto& spec : specs) {
        QImage scaled = image.scaled(spec.size, spec.size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!scaled.save(iconsetPath + QLatin1Char('/') + spec.name, "PNG")) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to write %1 inside temporary iconset.").arg(spec.name);
            }
            return false;
        }
    }

    const QString iconutilPath = iconutilExecutable();
    if (iconutilPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not locate 'iconutil'. Install Xcode command-line tools to enable icon conversion.");
        }
        return false;
    }

    QProcess iconutil;
    iconutil.start(iconutilPath, {
        QStringLiteral("-c"),
        QStringLiteral("icns"),
        QStringLiteral("-o"),
        destinationIcns,
        iconsetPath
    });
    if (!iconutil.waitForFinished() || iconutil.exitStatus() != QProcess::NormalExit || iconutil.exitCode() != 0) {
        if (errorMessage) {
            QString stderrOut = QString::fromUtf8(iconutil.readAllStandardError());
            if (stderrOut.isEmpty()) {
                stderrOut = QString::fromUtf8(iconutil.readAllStandardOutput());
            }
            if (stderrOut.isEmpty()) {
                stderrOut = QObject::tr("Unknown error.");
            }
            *errorMessage = QObject::tr("iconutil failed: %1").arg(stderrOut.trimmed());
        }
        return false;
    }

    return true;
}

bool maybeAttachIcon(const SettingsModel& settings,
                     const QString& resourcesPath,
                     QString* outIconFileName,
                     QString* errorMessage)
{
    const QString iconSource = settings.iconPath.trimmed();

    QString iconTargetName = settings.executableFileName().trimmed();
    if (iconTargetName.isEmpty()) {
        iconTargetName = QStringLiteral("PyAppExec");
    }
    iconTargetName += QStringLiteral(".icns");
    const QString destination = resourcesPath + QLatin1Char('/') + iconTargetName;

    if (QFile::exists(destination)) {
        QFile::remove(destination);
    }

    if (iconSource.isEmpty()) {
        const QString resourceIcon = QStringLiteral(":/net/quicknode/pyappexec/icons/PyAppExec.icns");
        if (!QFile::exists(resourceIcon)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Built-in icon resource is missing.");
            }
            return false;
        }
        if (!QFile::copy(resourceIcon, destination)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to copy the default PyAppExec icon into the bundle.");
            }
            return false;
        }
    } else {
        QFileInfo info(iconSource);
        if (!info.exists() || !info.isFile()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Icon file %1 does not exist.").arg(iconSource);
            }
            return false;
        }

        if (info.suffix().compare(QStringLiteral("icns"), Qt::CaseInsensitive) == 0) {
            if (!QFile::copy(iconSource, destination)) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("Failed to copy icon into bundle.");
                }
                return false;
            }
        } else {
            if (!generateIcnsFromImage(iconSource, destination, errorMessage)) {
                return false;
            }
        }
    }

    if (outIconFileName) {
        *outIconFileName = iconTargetName;
    }
    return true;
}
#endif
}

bool BinaryPackager::install(const SettingsModel& settings,
                             const QString& iniContents,
                             QString* createdIniPath,
                             QString* errorMessage,
                             bool createBundle,
                             bool writeIni) const
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

    QString iniPath;
    if (writeIni) {
        iniPath = targetDir.filePath(QStringLiteral("pyappexec.ini"));
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
    } else if (createdIniPath) {
        *createdIniPath = targetDir.filePath(QStringLiteral("pyappexec.ini"));
    }

#if !defined(Q_OS_WIN)
    const QString uninstallScriptName =
#  if defined(Q_OS_MAC)
        QStringLiteral("reset_pyappexec.command");
#  else
        QStringLiteral("reset_pyappexec.sh");
#  endif
    const QString uninstallScriptPath = targetDir.filePath(uninstallScriptName);
    const QString uninstallSection =
#  if defined(Q_OS_MAC)
        QStringLiteral("MacOS:main");
#  else
        QStringLiteral("Linux:main");
#  endif
    if (!writeUninstallScript(uninstallScriptPath, uninstallSection, errorMessage)) {
        return false;
    }
#endif

    const QString sourceLauncher = launcherSourcePath();
    if (sourceLauncher.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not locate pyappexec binary near the installer.");
        }
        return false;
    }

#if defined(Q_OS_MAC)
    if (createBundle) {
        const QString bundlePath = targetDir.filePath(settings.bundleName());
        QDir bundleDir(bundlePath);
        if (bundleDir.exists()) {
            if (!bundleDir.removeRecursively()) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("Unable to replace existing bundle at %1").arg(bundlePath);
                }
                return false;
            }
        }
        const QString contentsPath = bundlePath + QStringLiteral("/Contents");
        const QString macosPath = contentsPath + QStringLiteral("/MacOS");
        const QString resourcesPath = contentsPath + QStringLiteral("/Resources");
        if (!QDir().mkpath(macosPath) || !QDir().mkpath(resourcesPath)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to create bundle structure at %1").arg(bundlePath);
            }
            return false;
        }

        const QString destLauncher = macosPath + QLatin1Char('/') + settings.executableFileName();
        if (QFile::exists(destLauncher)) {
            QFile::remove(destLauncher);
        }
        if (!QFile::copy(sourceLauncher, destLauncher)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to copy launcher binary into bundle.");
            }
            return false;
        }
        QFile::setPermissions(destLauncher, QFile::permissions(destLauncher) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);

        QString iconFileName;
        if (!maybeAttachIcon(settings, resourcesPath, &iconFileName, errorMessage)) {
            return false;
        }

        if (!writeInfoPlist(contentsPath, settings, settings.executableFileName(), iconFileName, errorMessage)) {
            return false;
        }

        if (settings.bundleProject) {
            const QString resourcesAppPath = resourcesPath + QStringLiteral("/app");
            const QString skipPath = QDir(bundlePath).absolutePath();
            if (!copyDirectoryRecursive(QDir(settings.projectPath), resourcesAppPath, skipPath, errorMessage)) {
                return false;
            }

            const QString bundledIniPath = macosPath + QStringLiteral("/pyappexec.ini");
            const QString bundledResetPath = macosPath + QStringLiteral("/reset_pyappexec.command");
            const QString adjustedIni = rewriteIniForBundledMac(iniContents);

            QFile bundledIni(bundledIniPath);
            if (!bundledIni.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("Failed to write bundled INI at %1").arg(bundledIniPath);
                }
                return false;
            }
            QTextStream bundledStream(&bundledIni);
            bundledStream << adjustedIni;
            bundledIni.close();

            if (!writeUninstallScript(bundledResetPath, QStringLiteral("MacOS:main"), errorMessage)) {
                return false;
            }

            const QString identity = qEnvironmentVariable("CODESIGN_IDENTITY", QStringLiteral("-"));
            if (!identity.isEmpty()) {
                QProcess signer;
                signer.start(QStringLiteral("codesign"),
                             QStringList() << QStringLiteral("--force")
                                           << QStringLiteral("--deep")
                                           << QStringLiteral("--timestamp=none")
                                           << QStringLiteral("--sign") << identity
                                           << bundlePath);
                if (!signer.waitForFinished(120000) || signer.exitStatus() != QProcess::NormalExit || signer.exitCode() != 0) {
                    if (errorMessage) {
                        *errorMessage = QObject::tr("codesign failed for %1: %2").arg(bundlePath, QString::fromLocal8Bit(signer.readAllStandardError()));
                    }
                }
            }
        }
        return true;
    }
#endif
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

#ifdef Q_OS_WIN
    // Bundle the Qt platform plugin and Qt/third-party DLLs that sit next to the launcher
    const QString sourceDir = QFileInfo(sourceLauncher).absolutePath();
    const QString platformDir = sourceDir + QStringLiteral("/platforms");
    if (QDir(platformDir).exists()) {
        const QString destPlatformDir = targetDir.filePath(QStringLiteral("platforms"));
        if (!QDir().mkpath(destPlatformDir)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to create platforms directory at %1").arg(destPlatformDir);
            }
            return false;
        }
        QDirIterator it(platformDir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString sourcePath = it.next();
            const QString relativePath = QDir(platformDir).relativeFilePath(sourcePath);
            const QString destPath = destPlatformDir + QLatin1Char('/') + relativePath;
            const QString parentDir = QFileInfo(destPath).absolutePath();
            QDir().mkpath(parentDir);
            if (!copyFileOverwrite(sourcePath, destPath,
                                   QObject::tr("Failed to copy Qt platform plugin from %1 to %2"),
                                   errorMessage)) {
                return false;
            }
        }
    }

    QDirIterator dllIt(sourceDir, QStringList{QStringLiteral("*.dll")}, QDir::Files | QDir::NoSymLinks);
    while (dllIt.hasNext()) {
        const QString dllPath = dllIt.next();
        const QString fileName = QFileInfo(dllPath).fileName();
        const QString destPath = targetDir.filePath(fileName);
        if (!copyFileOverwrite(dllPath, destPath,
                               QObject::tr("Failed to copy runtime library from %1 to %2"),
                               errorMessage)) {
            return false;
        }
    }
#endif

#ifdef Q_OS_UNIX
    QFile::setPermissions(destLauncher, QFile::permissions(destLauncher) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    return true;
}

} // namespace installer

#if defined(Q_OS_WIN)
bool installer::BinaryPackager::uninstall(const SettingsModel& settings, QString* errorMessage) const
{
    QDir targetDir(settings.projectPath);
    if (!targetDir.exists()) {
        return true; // nothing to do
    }

    const QString launcherPath = targetDir.filePath(settings.executableFileName());
    if (QFile::exists(launcherPath) && !QFile::remove(launcherPath)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to remove launcher at %1").arg(launcherPath);
        }
        return false;
    }

    const QString iniPath = targetDir.filePath(QStringLiteral("pyappexec.ini"));
    if (QFile::exists(iniPath) && !QFile::remove(iniPath)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to remove %1").arg(iniPath);
        }
        return false;
    }

    const QString platformsDir = targetDir.filePath(QStringLiteral("platforms"));
    if (QDir(platformsDir).exists()) {
        if (!QDir(platformsDir).removeRecursively()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to remove Qt platforms directory at %1").arg(platformsDir);
            }
            return false;
        }
    }

    // Remove DLLs that we would have copied alongside the launcher
    const QString sourceLauncher = launcherSourcePath();
    if (!sourceLauncher.isEmpty()) {
        const QString sourceDir = QFileInfo(sourceLauncher).absolutePath();
        QSet<QString> dllNames;
        QDirIterator dllIt(sourceDir, QStringList{QStringLiteral("*.dll")}, QDir::Files | QDir::NoSymLinks);
        while (dllIt.hasNext()) {
            dllIt.next();
            dllNames.insert(dllIt.fileName().toLower());
        }
        for (const QString& dllName : dllNames) {
            const QString destPath = targetDir.filePath(dllName);
            if (QFile::exists(destPath) && !QFile::remove(destPath)) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("Failed to remove %1").arg(destPath);
                }
                return false;
            }
        }
    }

    // Remove the config directory associated with this app id
    QString appId = settings.appId.trimmed();
    if (appId.isEmpty()) {
        // Try to read from the installed INI if present
        if (QFile::exists(iniPath)) {
            QSettings ini(iniPath, QSettings::IniFormat);
            const QString section = QStringLiteral("Windows:main");
            if (ini.childGroups().contains(section)) {
                ini.beginGroup(section);
                const QString iniAppId = ini.value(QStringLiteral("app_id")).toString().trimmed();
                if (!iniAppId.isEmpty()) {
                    appId = iniAppId;
                }
                ini.endGroup();
            }
        }
    }
    const QString sanitizedId = sanitizeAppId(appId);

    QString configRoot;
    if (QFile::exists(iniPath)) {
        QSettings ini(iniPath, QSettings::IniFormat);
        const QString section = QStringLiteral("Windows:main");
        if (ini.childGroups().contains(section)) {
            ini.beginGroup(section);
            configRoot = ini.value(QStringLiteral("config_root")).toString().trimmed();
            ini.endGroup();
        }
    }

    QDir configDir;
    if (!configRoot.isEmpty()) {
        QFileInfo info(configRoot);
        if (info.isRelative()) {
            configDir = QDir(targetDir.absoluteFilePath(configRoot));
        } else {
            configDir = QDir(configRoot);
        }
    } else {
        QString base = qEnvironmentVariable("LOCALAPPDATA");
        if (base.isEmpty()) {
            base = qEnvironmentVariable("APPDATA");
        }
        if (base.isEmpty()) {
            base = QDir::currentPath();
        }
        configDir = QDir(QDir(base).filePath(QStringLiteral("PyAppExec/%1").arg(sanitizedId)));
    }

    if (configDir.exists()) {
        if (!configDir.removeRecursively()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Failed to remove config directory at %1").arg(configDir.absolutePath());
            }
            return false;
        }
    }

    return true;
}
#endif
