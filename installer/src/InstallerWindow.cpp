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

    BrowseRow appNameRow = createBrowseRow(tr("Application name:"), central);
    appNameEdit_ = appNameRow.lineEdit;
    appNameRow.browseButton->hide();
    layout->addWidget(appNameRow.container);

    BrowseRow exeRow = createBrowseRow(tr("Executable name:"), central);
    executableNameEdit_ = exeRow.lineEdit;
    exeRow.browseButton->hide();
    layout->addWidget(exeRow.container);

    bundleProjectCheck_ = new QCheckBox(tr("Create self-contained .app (copy project into bundle)"), central);
#if defined(Q_OS_MAC)
    layout->addWidget(bundleProjectCheck_);
#else
    bundleProjectCheck_->hide();
#endif

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

    hideGuiCheck_ = new QCheckBox(tr("Hide GUI after successful runs"), central);
    layout->addWidget(hideGuiCheck_);

    installButton_ = new QPushButton(tr("Install PyAppExec"), central);
    installButton_->setFixedWidth(220);
    layout->addWidget(installButton_, 0, Qt::AlignHCenter);
    connect(installButton_, &QPushButton::clicked, this, &InstallerWindow::handleInstall);

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
#if defined(Q_OS_MAC)
    settings.bundleProject = bundleProjectCheck_ ? bundleProjectCheck_->isChecked() : false;
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

    if (!packager.install(settings, iniContents, &createdIni, &error)) {
        QMessageBox::critical(this, tr("Install failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }

    logMessage(tr("Generated %1").arg(createdIni));
    logMessage(tr("Copied launcher as %1").arg(settings.launcherArtifactName()));

    QDesktopServices::openUrl(QUrl::fromLocalFile(createdIni));
    QMessageBox::information(this, tr("Success"), tr("PyAppExec was installed for %1").arg(settings.appName));
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

} // namespace installer
