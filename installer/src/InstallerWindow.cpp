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
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUrl>

namespace installer {

InstallerWindow::InstallerWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PyAppExec Installer %1").arg(QString::fromUtf8(AppMetadata::kVersion.data())));
    resize(600, 480);

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
    layout->addWidget(installButton_);
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

} // namespace installer
