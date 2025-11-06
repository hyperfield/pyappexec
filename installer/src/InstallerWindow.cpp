#include "installer/InstallerWindow.hpp"

#include "AppMetadata.hpp"
#include "installer/BinaryPackager.hpp"
#include "installer/IniTemplate.hpp"
#include "installer/UiWidgets.hpp"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUrl>

namespace installer {

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
    settings.hideGuiAfterSuccess = hideGuiCheck_->isChecked();
    return settings;
}

void InstallerWindow::handleInstall()
{
    SettingsModel settings = gatherSettings();
    if (settings.projectPath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing information"), tr("Select a Python project directory."));
        return;
    }

    InspectionResult inspection = inspector_.inspect(settings.projectPath);
    IniTemplate iniTemplate;
    const QString iniContents = iniTemplate.generate(settings, inspection);

    BinaryPackager packager;
    QString createdIni;
    QString error;
    if (!packager.install(settings, iniContents, &createdIni, &error)) {
        QMessageBox::critical(this, tr("Install failed"), error.isEmpty() ? tr("Unknown error.") : error);
        return;
    }

    logMessage(tr("Generated %1").arg(createdIni));
    logMessage(tr("Copied launcher as %1").arg(settings.executableFileName()));

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
    box.exec();
}

void InstallerWindow::showAboutQtDialog()
{
    QMessageBox::aboutQt(this);
}

} // namespace installer
