#include "installer/InstallerWindow.hpp"

#include "AppMetadata.hpp"
#include "installer/BinaryPackager.hpp"
#include "installer/IniTemplate.hpp"
#include "installer/UiWidgets.hpp"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QPushButton>
#include <QPixmap>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUrl>

namespace installer {

namespace {

QString generateAppId(int length = 10)
{
    static const QString alphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ0123456789");
    QString id;
    id.reserve(length);
    for (int i = 0; i < length; ++i) {
        const int idx = QRandomGenerator::global()->bounded(alphabet.size());
        id.append(alphabet.at(idx));
    }
    return id;
}

bool isValidAppId(const QString& id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9]{6,20}$"));
    return re.match(id).hasMatch();
}

} // namespace

InstallerWindow::InstallerWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PyAppExec Installer %1").arg(QString::fromUtf8(AppMetadata::kVersion.data())));
    resize(600, 480);

    QMenu* helpMenu = nullptr;
#if defined(Q_OS_MAC)
    helpMenu = menuBar()->addMenu(tr("Help"));
#else
    helpMenu = menuBar()->addMenu(tr("&Help"));
#endif
    auto* aboutAction = helpMenu->addAction(tr("About PyAppExec Installer"));
    aboutAction->setMenuRole(QAction::NoRole);
    connect(aboutAction, &QAction::triggered, this, &InstallerWindow::showAboutInstaller);
    auto* aboutQtAction = helpMenu->addAction(tr("About Qt"));
    aboutQtAction->setMenuRole(QAction::NoRole);
    connect(aboutQtAction, &QAction::triggered, this, &InstallerWindow::showAboutQtDialog);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setSpacing(8);

    BrowseRow projectRow = createBrowseRow(tr("Python project:"), central);
    projectPathEdit_ = projectRow.lineEdit;
    layout->addWidget(projectRow.container);
    connect(projectRow.browseButton, &QPushButton::clicked, this, &InstallerWindow::browseForProject);

    BrowseRow appNameRow = createBrowseRow(tr("App name:"), central);
    appNameEdit_ = appNameRow.lineEdit;
    appNameRow.browseButton->hide();
    layout->addWidget(appNameRow.container);

    BrowseRow exeRow = createBrowseRow(tr("Executable name:"), central);
    executableNameEdit_ = exeRow.lineEdit;
    exeRow.browseButton->hide();
    layout->addWidget(exeRow.container);

    BrowseRow appIdRow = createBrowseRow(tr("App ID (6-20 letters/numbers):"), central);
    appIdEdit_ = appIdRow.lineEdit;
    appIdRow.browseButton->hide();
    appIdEdit_->setText(generateAppId());
    layout->addWidget(appIdRow.container);

#if defined(Q_OS_MAC)
    BrowseRow iconRow = createBrowseRow(tr("App icon (PNG/ICNS):"), central);
    iconPathEdit_ = iconRow.lineEdit;
    layout->addWidget(iconRow.container);
    connect(iconRow.browseButton, &QPushButton::clicked, this, &InstallerWindow::browseForIcon);
#endif

    hideGuiCheck_ = new QCheckBox(tr("Hide GUI/CLI after successful runs"), central);
    layout->addWidget(hideGuiCheck_);

#if defined(Q_OS_WIN)
    copyCliOnlyCheck_ = new QCheckBox(tr("Copy only the CLI version (reduces the size)"), central);
    layout->addWidget(copyCliOnlyCheck_);
#endif

    installButton_ = new QPushButton(tr("Install PyAppExec"), central);
    installButton_->setFixedWidth(220);
    layout->addWidget(installButton_, 0, Qt::AlignHCenter);
    connect(installButton_, &QPushButton::clicked, this, &InstallerWindow::handleInstall);
    installButton_->setEnabled(false);

    uninstallButton_ = new QPushButton(tr("Uninstall PyAppExec"), central);
    uninstallButton_->setFixedWidth(220);
    layout->addWidget(uninstallButton_, 0, Qt::AlignHCenter);
    connect(uninstallButton_, &QPushButton::clicked, this, &InstallerWindow::handleUninstall);
    uninstallButton_->setEnabled(false);

#if defined(Q_OS_MAC)
    createBundleButton_ = new QPushButton(tr("Create macOS .app bundle"), central);
    createBundleButton_->setFixedWidth(220);
    createBundleButton_->setEnabled(false);
    layout->addWidget(createBundleButton_, 0, Qt::AlignHCenter);
    connect(createBundleButton_, &QPushButton::clicked, this, &InstallerWindow::handleCreateBundle);
#endif

    logView_ = new QTextEdit(central);
    logView_->setReadOnly(true);
    layout->addWidget(logView_, 1);

    setCentralWidget(central);
}

void InstallerWindow::browseForProject()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Python project"));
    if (dir.isEmpty()) {
        return;
    }
    projectPathEdit_->setText(dir);
    refreshInspection();
}

void InstallerWindow::browseForIcon()
{
#if defined(Q_OS_MAC)
    if (!iconPathEdit_) {
        return;
    }
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select application icon"),
        QString(),
        tr("Icon files (*.icns *.png *.jpg *.jpeg *.bmp)"));
    if (!file.isEmpty()) {
        iconPathEdit_->setText(file);
    }
#else
    Q_UNUSED(this);
#endif
}

void InstallerWindow::refreshInspection()
{
    lastInspection_ = inspector_.inspect(projectPathEdit_->text());
    if (!lastInspection_.suggestedAppName.isEmpty()) {
        appNameEdit_->setText(lastInspection_.suggestedAppName);
    }
    if (!lastInspection_.suggestedExecPath.isEmpty()) {
        executableNameEdit_->setText(lastInspection_.suggestedAppName.isEmpty()
            ? QStringLiteral("pyappexec")
            : lastInspection_.suggestedAppName.toLower());
    }

    for (const QString& note : lastInspection_.notes) {
        logMessage(note);
    }

    const QString iniPath = QDir(projectPathEdit_->text()).filePath(QStringLiteral("pyappexec.ini"));

#if defined(Q_OS_MAC)
    const bool hasIni = QFileInfo::exists(iniPath);
    if (createBundleButton_) {
        createBundleButton_->setEnabled(hasIni);
    }
#endif

    // If an existing INI is present, prefer its metadata for app name/id and GUI preference
    if (QFileInfo::exists(iniPath)) {
        QSettings ini(iniPath, QSettings::IniFormat);
        QString section =
#if defined(Q_OS_MAC)
            QStringLiteral("MacOS:main");
#elif defined(Q_OS_WIN)
            QStringLiteral("Windows:main");
#else
            QStringLiteral("Linux:main");
#endif
        if (ini.childGroups().contains(section)) {
            ini.beginGroup(section);
            const QString iniAppName = ini.value(QStringLiteral("app_name")).toString().trimmed();
            if (!iniAppName.isEmpty()) {
                appNameEdit_->setText(iniAppName);
            }
            const QString iniAppId = ini.value(QStringLiteral("app_id")).toString().trimmed();
            if (!iniAppId.isEmpty()) {
                appIdEdit_->setText(iniAppId);
            }
            QString guiHide = ini.value(QStringLiteral("GUI_CLI_HIDE_AFTER_SUCCESS")).toString();
            if (guiHide.isEmpty()) {
                guiHide = ini.value(QStringLiteral("GUI_HIDE_AFTER_SUCCESS")).toString(); // backward compatibility
            }
            if (!guiHide.isEmpty()) {
                const QString lowered = guiHide.trimmed().toLower();
                const bool hide = (lowered == QStringLiteral("1") ||
                                   lowered == QStringLiteral("true") ||
                                   lowered == QStringLiteral("yes") ||
                                   lowered == QStringLiteral("on"));
                hideGuiCheck_->setChecked(hide);
            }
            ini.endGroup();
        }
    }

    updateActionButtons();
}

SettingsModel InstallerWindow::gatherSettings() const
{
    SettingsModel settings;
    settings.projectPath = projectPathEdit_->text().trimmed();
    settings.appName = appNameEdit_->text().trimmed();
    settings.executableName = executableNameEdit_->text().trimmed();
    settings.appId = appIdEdit_->text().trimmed();
    if (iconPathEdit_) {
        settings.iconPath = iconPathEdit_->text().trimmed();
    }
    settings.hideGuiAfterSuccess = hideGuiCheck_->isChecked();
#if defined(Q_OS_WIN)
    if (copyCliOnlyCheck_) {
        settings.copyCliOnly = copyCliOnlyCheck_->isChecked();
    }
#endif
    return settings;
}

void InstallerWindow::handleInstall()
{
    SettingsModel settings = gatherSettings();
    if (settings.projectPath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing information"), tr("Select a Python project directory."));
        return;
    }

    if (settings.appId.isEmpty()) {
        settings.appId = generateAppId();
        appIdEdit_->setText(settings.appId);
        logMessage(tr("App ID was empty; generated %1").arg(settings.appId));
    }

    if (!isValidAppId(settings.appId)) {
        QMessageBox::warning(
            this,
            tr("Invalid App ID"),
            tr("App ID must be 6-20 characters of only letters and numbers."));
        return;
    }
#if defined(Q_OS_MAC)
    if (!settings.iconPath.isEmpty() && !QFileInfo::exists(settings.iconPath)) {
        QMessageBox::warning(this,
                             tr("Invalid icon"),
                             tr("The selected icon path does not exist. Please pick a valid file or leave the field blank."));
        return;
    }
#endif

    InspectionResult inspection = inspector_.inspect(settings.projectPath);
    IniTemplate iniTemplate;
    const QString iniContents = iniTemplate.generate(settings, inspection);

    BinaryPackager packager;
    QString createdIni;
    QString error;

    const QString iniPath = QDir(settings.projectPath).filePath(QStringLiteral("pyappexec.ini"));
    if (QFileInfo::exists(iniPath)) {
        const auto response = QMessageBox::question(
            this,
            tr("Overwrite existing configuration?"),
            tr("%1 already exists. Overwrite it with a new pyappexec.ini?").arg(iniPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (response != QMessageBox::Yes) {
            logMessage(tr("Installation cancelled: existing pyappexec.ini left untouched."));
            return;
        }
    }

    if (!packager.install(settings, iniContents, &createdIni, &error, /*createBundle=*/false, /*writeIni=*/true)) {
        QMessageBox::critical(this, tr("Install failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }

    logMessage(tr("Generated %1").arg(createdIni));
    logMessage(tr("Copied launcher as %1").arg(settings.launcherArtifactName()));
#if defined(Q_OS_WIN)
    if (settings.copyCliOnly) {
        logMessage(tr("CLI-only mode: skipped copying Qt and other adjacent DLLs."));
    }
#endif

    QDesktopServices::openUrl(QUrl::fromLocalFile(createdIni));
    QMessageBox::information(this, tr("Success"), tr("PyAppExec was installed for %1").arg(settings.appName));
    updateActionButtons();
}

void InstallerWindow::handleUninstall()
{
    SettingsModel settings = gatherSettings();
    if (settings.projectPath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing information"), tr("Select a Python project directory to uninstall from."));
        return;
    }

    const auto response = QMessageBox::question(
        this,
        tr("Uninstall PyAppExec"),
        tr("This will remove the launcher, copied DLLs, and PyAppExec config/state for this app. Continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (response != QMessageBox::Yes) {
        return;
    }

    BinaryPackager packager;
    QString error;
    if (!packager.uninstall(settings, &error)) {
        QMessageBox::critical(this, tr("Uninstall failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }

    logMessage(tr("Uninstall completed for %1").arg(settings.projectPath));
    QMessageBox::information(this, tr("Success"), tr("PyAppExec was uninstalled."));
    updateActionButtons();
}

void InstallerWindow::handleCreateBundle()
{
#if defined(Q_OS_MAC)
    SettingsModel settings = gatherSettings();
    settings.bundleProject = true;

    if (settings.projectPath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing information"), tr("Select a Python project directory."));
        return;
    }

    const QString iniPath = QDir(settings.projectPath).filePath(QStringLiteral("pyappexec.ini"));
    if (!QFileInfo::exists(iniPath)) {
        QMessageBox::warning(this, tr("Missing configuration"), tr("pyappexec.ini not found in the selected directory."));
        return;
    }

    QFile iniFile(iniPath);
    if (!iniFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Unable to read INI"), tr("Could not read %1").arg(iniPath));
        return;
    }
    const QString iniContents = QString::fromUtf8(iniFile.readAll());
    iniFile.close();

    if (!iniContents.contains(QStringLiteral("[MacOS:main]"))) {
        QMessageBox::warning(this,
                             tr("Incomplete configuration"),
                             tr("pyappexec.ini is missing a [MacOS:main] section. Add it before creating a macOS bundle."));
        return;
    }

    BinaryPackager packager;
    QString createdIni;
    QString error;
    if (!packager.install(settings, iniContents, &createdIni, &error, /*createBundle=*/true, /*writeIni=*/false)) {
        QMessageBox::critical(this, tr("Bundle failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }

    logMessage(tr("Created bundled app for %1").arg(settings.appName));
    QMessageBox::information(this, tr("Success"), tr("Bundled app created."));
#else
    QMessageBox::information(this, tr("Not supported"), tr("Bundle creation is macOS-only."));
#endif
}

void InstallerWindow::logMessage(const QString& message)
{
    if (!message.isEmpty()) {
        logView_->append(message);
    }
}

void InstallerWindow::showAboutInstaller()
{
    const QString body = tr(
        "<b>PyAppExec Installer</b><br><br>"
        "Version: %1<br>"
        "Author: %2<br>"
        "License: %3<br>"
        "Github: <a href=\"%4\">%4</a><br>"
        "Years: %5")
        .arg(QString::fromUtf8(AppMetadata::kVersion.data()),
             QString::fromUtf8(AppMetadata::kAuthor.data()),
             QString::fromUtf8(AppMetadata::kLicense.data()),
             QString::fromUtf8(AppMetadata::kGithub.data()),
             QString::fromUtf8(AppMetadata::kYears.data()));

    QMessageBox box(this);
    box.setWindowTitle(tr("About PyAppExec Installer"));
    box.setText(body);
    box.setTextFormat(Qt::RichText);
    box.setStandardButtons(QMessageBox::Ok);
    if (QPixmap logo(QStringLiteral(":/net/quicknode/pyappexec/logo_transparent.png")); !logo.isNull()) {
        box.setIconPixmap(logo.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    box.exec();
}

void InstallerWindow::showAboutQtDialog()
{
    QMessageBox::aboutQt(this);
}

void InstallerWindow::updateActionButtons()
{
    const bool hasProject = !projectPathEdit_->text().trimmed().isEmpty();
    if (installButton_) {
        installButton_->setEnabled(hasProject);
    }

#if defined(Q_OS_WIN)
    bool hasIni = false;
    if (hasProject) {
        const QString iniPath = QDir(projectPathEdit_->text()).filePath(QStringLiteral("pyappexec.ini"));
        hasIni = QFileInfo::exists(iniPath);
    }
    if (uninstallButton_) {
        uninstallButton_->setEnabled(hasProject && hasIni);
        if (!hasIni) {
            uninstallButton_->setToolTip(tr("pyappexec.ini not found in the selected directory."));
        } else {
            uninstallButton_->setToolTip(QString());
        }
    }
#endif
}

} // namespace installer
