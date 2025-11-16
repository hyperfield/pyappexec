#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QProcess>
#include <QStringList>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QCloseEvent;
class QString;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QStringList& cliArguments,
                        const QString& stateFilePath,
                        const QString& appDisplayName,
                        bool autoSuppressAfterSuccess,
                        QWidget* parent = nullptr);

private slots:
    void handleStdOutput();
    void handleStdError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);
    void showAboutPyAppExec();
    void showAboutQt();

private:
    void startProcess();
    void appendOutput(const QString& text, bool isError);
    void persistGuiPreference(bool suppress);
    void closeEvent(QCloseEvent* event) override;

    QStringList cliArguments_;
    QString stateFilePath_;
    QString appDisplayName_;
    QPlainTextEdit* terminalView_ {nullptr};
    QProgressBar* progressBar_ {nullptr};
    QLabel* statusLabel_ {nullptr};
    QPushButton* closeButton_ {nullptr};
    QCheckBox* suppressCheckBox_ {nullptr};
    QProcess* process_ {nullptr};
    bool completedSuccessfully_ {false};
    bool autoSuppressAfterSuccess_ {false};
};

#endif
