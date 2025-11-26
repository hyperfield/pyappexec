#ifndef INSTALLER_INSTALLERWINDOW_HPP
#define INSTALLER_INSTALLERWINDOW_HPP

#include "installer/ProjectInspector.hpp"
#include "installer/SettingsModel.hpp"

#include <QMainWindow>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;

namespace installer {

class InstallerWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit InstallerWindow(QWidget* parent = nullptr);

private slots:
    void browseForProject();
    void browseForIcon();
    void handleInstall();
    void showAboutInstaller();
    void showAboutQtDialog();

private:
    void refreshInspection();
    SettingsModel gatherSettings() const;
    void logMessage(const QString& message);

    ProjectInspector inspector_;
    InspectionResult lastInspection_;

    QLineEdit* projectPathEdit_{nullptr};
    QLineEdit* appNameEdit_{nullptr};
    QLineEdit* executableNameEdit_{nullptr};
    QLineEdit* appIdEdit_{nullptr};
    QLineEdit* iconPathEdit_{nullptr};
    QCheckBox* hideGuiCheck_{nullptr};
    QTextEdit* logView_{nullptr};
    QPushButton* installButton_{nullptr};
};

} // namespace installer

#endif
