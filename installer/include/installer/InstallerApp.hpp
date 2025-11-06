#ifndef INSTALLER_APP_HPP
#define INSTALLER_APP_HPP

#include <QApplication>

namespace installer {

class InstallerApp
{
public:
    InstallerApp(int& argc, char** argv);
    int run();

private:
    QApplication app_;
};

} // namespace installer

#endif
