#include "installer/InstallerApp.hpp"

int main(int argc, char** argv)
{
    installer::InstallerApp app(argc, argv);
    return app.run();
}
