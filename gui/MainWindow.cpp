#include "gui/MainWindow.hpp"

#include <QAction>
#include "AppMetadata.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
// cppcheck-suppress missingIncludeSystem
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
// cppcheck-suppress missingIncludeSystem
#include <QMessageBox>
#include <QPlainTextEdit>
// cppcheck-suppress missingIncludeSystem
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
// cppcheck-suppress missingIncludeSystem
#include <QVBoxLayout>
#include <QObject>

namespace {

QString terminalTitle()
{
#if defined(_WIN32)
    return QObject::tr("Embedded PowerShell output");
#elif defined(__APPLE__)
    return QObject::tr("Embedded Terminal output (macOS)");
#else
    return QObject::tr("Embedded Terminal output (Linux)");
#endif
}

}


MainWindow::MainWindow(const QStringList& cliArguments, const QString& stateFilePath, const QString& appDisplayName, QWidget* parent) :
    QMainWindow(parent),
    cliArguments_(cliArguments),
    stateFilePath_(stateFilePath),
    appDisplayName_(appDisplayName.isEmpty() ? tr("PyAppExec") : appDisplayName)
{
    setWindowTitle(QStringLiteral("%1 (via PyAppExec)").arg(appDisplayName_));
    resize(600, 600);

    QMenu* helpMenu = nullptr;
#if defined(Q_OS_MAC)
    helpMenu = menuBar()->addMenu(tr("Help"));
#else
    helpMenu = menuBar()->addMenu(tr("&Help"));
#endif
    auto* aboutAction = helpMenu->addAction(tr("About PyAppExec"));
    aboutAction->setMenuRole(QAction::NoRole);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutPyAppExec);
    auto* aboutQtAction = helpMenu->addAction(tr("About Qt"));
    aboutQtAction->setMenuRole(QAction::NoRole);
    connect(aboutQtAction, &QAction::triggered, this, &MainWindow::showAboutQt);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    statusLabel_ = new QLabel(tr("Status: waiting to start"), central);
    layout->addWidget(statusLabel_);

    auto* terminalHeader = new QLabel(terminalTitle(), central);
    layout->addWidget(terminalHeader);

    terminalView_ = new QPlainTextEdit(central);
    terminalView_->setReadOnly(true);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    terminalView_->setFont(font);
    terminalView_->setMaximumBlockCount(5000);
    layout->addWidget(terminalView_);

    progressBar_ = new QProgressBar(central);
    progressBar_->setRange(0, 0);
    layout->addWidget(progressBar_);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    suppressCheckBox_ = new QCheckBox(tr("Hide GUI after successful runs"), central);
    suppressCheckBox_->setEnabled(false);
    suppressCheckBox_->setToolTip(tr("If checked, the GUI stays hidden on future successful runs (it reappears automatically if a run fails)."));
    buttons->addWidget(suppressCheckBox_);

    closeButton_ = new QPushButton(tr("Close"), central);
    closeButton_->setEnabled(false);
    buttons->addWidget(closeButton_);
    layout->addLayout(buttons);

    setCentralWidget(central);

    connect(closeButton_, &QPushButton::clicked, this, &QWidget::close);

    startProcess();
}


void MainWindow::startProcess()
{
    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process_, &QProcess::readyReadStandardOutput, this, &MainWindow::handleStdOutput);
    connect(process_, &QProcess::readyReadStandardError, this, &MainWindow::handleStdError);
    connect(process_, &QProcess::finished, this, &MainWindow::handleProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &MainWindow::handleProcessError);

    QString program = QCoreApplication::applicationFilePath();
    QStringList args = cliArguments_;
    if (!args.contains(QStringLiteral("--no-gui"))) {
        args << QStringLiteral("--no-gui");
    }

    statusLabel_->setText(tr("Status: running setup..."));
    process_->start(program, args);
}


void MainWindow::handleStdOutput()
{
    appendOutput(QString::fromUtf8(process_->readAllStandardOutput()), false);
}


void MainWindow::handleStdError()
{
    appendOutput(QString::fromUtf8(process_->readAllStandardError()), true);
}


void MainWindow::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    completedSuccessfully_ = (status == QProcess::NormalExit && exitCode == 0);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(completedSuccessfully_ ? 1 : 0);
    closeButton_->setEnabled(true);

    if (completedSuccessfully_) {
        statusLabel_->setText(tr("Status: completed successfully"));
        appendOutput(tr("[info] %1 finished successfully.\n").arg(appDisplayName_), false);
        appendOutput(tr("[info] Bootstrapped by %1 %2 (%3). Project: %4\n")
                         .arg(QString::fromUtf8(AppMetadata::kAppName.data()),
                              QString::fromUtf8(AppMetadata::kVersion.data()),
                              QString::fromUtf8(AppMetadata::kYears.data()),
                              QString::fromUtf8(AppMetadata::kGithub.data())),
                     false);
        suppressCheckBox_->setEnabled(true);
    } else {
        statusLabel_->setText(tr("Status: failed (exit code %1)").arg(exitCode));
        QMessageBox::critical(this, appDisplayName_, tr("The launcher encountered an error. Review the log output above."));
        suppressCheckBox_->setChecked(false);
        suppressCheckBox_->setEnabled(false);
        persistGuiPreference(false);
    }
}


void MainWindow::handleProcessError(QProcess::ProcessError)
{
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    closeButton_->setEnabled(true);
    statusLabel_->setText(tr("Status: failed to start launching process"));
    QMessageBox::critical(this, appDisplayName_, tr("Unable to start the launcher process."));
    suppressCheckBox_->setChecked(false);
    suppressCheckBox_->setEnabled(false);
    persistGuiPreference(false);
}


void MainWindow::appendOutput(const QString& text, bool isError)
{
    if (text.isEmpty()) {
        return;
    }

    QString payload = text;
    if (isError) {
        payload = QStringLiteral("[stderr] ") + payload;
    }

    terminalView_->moveCursor(QTextCursor::End);
    terminalView_->insertPlainText(payload);
    terminalView_->verticalScrollBar()->setValue(terminalView_->verticalScrollBar()->maximum());
}


void MainWindow::persistGuiPreference(bool suppress)
{
    if (stateFilePath_.isEmpty()) {
        return;
    }

    QFile file(stateFilePath_);
    if (suppress) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            file.write("suppress_gui=1\n");
            file.close();
        }
    } else {
        if (file.exists()) {
            file.remove();
        }
    }
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    if (completedSuccessfully_) {
        persistGuiPreference(suppressCheckBox_->isChecked());
    }
    QMainWindow::closeEvent(event);
}


void MainWindow::showAboutPyAppExec()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About PyAppExec"));
    dialog.resize(420, 320);

    auto* layout = new QVBoxLayout(&dialog);

    auto* textLabel = new QLabel(QStringLiteral(
        "<b>%1</b> is bootstrapped by %2.<br><br>"
        "PyAppExec prepares Python interpreters, virtual environments, and external tools so end users can run your "
        "packaged application without manual setup.<br><br>"
        "<b>Version:</b> %3<br>"
        "<b>Author:</b> %4<br>"
        "<b>License:</b> %5<br>"
        "<b>Github:</b> <a href=\"%6\">%6</a><br>"
        "<b>Years:</b> %7")
        .arg(appDisplayName_,
             QString::fromUtf8(AppMetadata::kAppName.data()),
             QString::fromUtf8(AppMetadata::kVersion.data()),
             QString::fromUtf8(AppMetadata::kAuthor.data()),
             QString::fromUtf8(AppMetadata::kLicense.data()),
             QString::fromUtf8(AppMetadata::kGithub.data()),
             QString::fromUtf8(AppMetadata::kYears.data())));
    textLabel->setWordWrap(true);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setOpenExternalLinks(true);
    layout->addWidget(textLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}


void MainWindow::showAboutQt()
{
    QMessageBox::aboutQt(this);
}
