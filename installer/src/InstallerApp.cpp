#include "installer/InstallerApp.hpp"

#include "installer/InstallerWindow.hpp"

namespace installer {

InstallerApp::InstallerApp(int& argc, char** argv)
    : app_(argc, argv)
{
    app_.setApplicationName(QStringLiteral("PyAppExec Installer"));
}

int InstallerApp::run()
{
    InstallerWindow window;
    window.show();
    return app_.exec();
}

} // namespace installer
