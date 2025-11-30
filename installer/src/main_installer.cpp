#include "installer/InstallerApp.hpp"
#include "app_version.hpp"
#include "AppMetadata.hpp"

#include <iostream>
#include <string_view>
#if _WIN32
#include <windows.h>
#endif

namespace {
void ensureConsole()
{
#if _WIN32
    AttachConsole(ATTACH_PARENT_PROCESS);
#endif
}

void printHelp()
{
    ensureConsole();
    std::cout << "PyAppExec Installer\n"
              << "Usage: pyappexec_installer [--help] [--version]\n";
}

void printVersion()
{
    ensureConsole();
    std::cout << "PyAppExec Installer " << AppVersion::kAppVersion << "\n"
              << "Author: " << AppMetadata::kAuthor << "\n"
              << "Years: " << AppMetadata::kYears << "\n"
              << "Repo: " << AppMetadata::kGithub << "\n";
}
} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            printVersion();
            return 0;
        }
    }

    installer::InstallerApp app(argc, argv);
    return app.run();
}
