#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QProcess>
#include <QStringList>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QStringList& cliArguments, QWidget* parent = nullptr);

private slots:
    void handleStdOutput();
    void handleStdError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    void startProcess();
    void appendOutput(const QString& text, bool isError);

    QStringList cliArguments_;
    QPlainTextEdit* terminalView_ {nullptr};
    QProgressBar* progressBar_ {nullptr};
    QLabel* statusLabel_ {nullptr};
    QPushButton* closeButton_ {nullptr};
    QProcess* process_ {nullptr};
    bool completedSuccessfully_ {false};
};

#endif
