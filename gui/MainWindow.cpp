#include "gui/MainWindow.hpp"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
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


MainWindow::MainWindow(const QStringList& cliArguments, QWidget* parent) :
    QMainWindow(parent),
    cliArguments_(cliArguments)
{
    setWindowTitle(tr("PyAppExec"));
    resize(900, 600);

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
        QMessageBox::information(this, tr("PyAppExec"), tr("Python application finished successfully."));
    } else {
        statusLabel_->setText(tr("Status: failed (exit code %1)").arg(exitCode));
        QMessageBox::critical(this, tr("PyAppExec"), tr("The launcher encountered an error. Review the log output above."));
    }
}


void MainWindow::handleProcessError(QProcess::ProcessError)
{
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    closeButton_->setEnabled(true);
    statusLabel_->setText(tr("Status: failed to start launching process"));
    QMessageBox::critical(this, tr("PyAppExec"), tr("Unable to start the launcher process."));
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
