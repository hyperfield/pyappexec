#include "installer/InstallerApp.hpp"
#include "app_version.hpp"
#include "AppMetadata.hpp"
#include <iostream>
#include <string_view>
#if _WIN32
#include <windows.h>
#endif

namespace {
void writeStdout(const std::string& msg)
{
#if _WIN32
    std::string crlf = msg;
    for (size_t pos = 0; (pos = crlf.find('\n', pos)) != std::string::npos; ++pos) {
        crlf.replace(pos, 1, "\r\n");
        ++pos;
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleA(hOut, crlf.c_str(), static_cast<DWORD>(crlf.size()), &written, nullptr);
        return;
    }
#endif
    std::cout << msg;
}

bool ensureConsoleAttached()
{
#if _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        return true;
    }
    return false;
#else
    return true;
#endif
}
} // namespace

int main(int argc, char** argv)
{
    bool forceCli = false;
    bool showVersion = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--no-gui") {
            forceCli = true;
        }
        if (arg == "--version" || arg == "-v") {
            showVersion = true;
        }
    }

    if (showVersion) {
        bool hasConsole = ensureConsoleAttached();
        std::string out = std::string("PyAppExec Installer ") + std::string(AppVersion::kAppVersion) + "\n" +
            "Author: " + std::string(AppMetadata::kAuthor) + "\n" +
            "Years: " + std::string(AppMetadata::kYears) + "\n" +
            "Repo: " + std::string(AppMetadata::kGithub) + "\n";
        writeStdout(out);
#if _WIN32
        if (!hasConsole && !forceCli) {
            std::string msg = std::string("PyAppExec Installer ") + std::string(AppVersion::kAppVersion) +
                "\nAuthor: " + std::string(AppMetadata::kAuthor) +
                "\nYears: " + std::string(AppMetadata::kYears) +
                "\nRepo: " + std::string(AppMetadata::kGithub);
            MessageBoxA(nullptr, msg.c_str(), "PyAppExec Installer version", MB_OK | MB_ICONINFORMATION);
        }
#endif
        return 0;
    }

    if (!forceCli) {
#if _WIN32
        FreeConsole();
#endif
    }

    installer::InstallerApp app(argc, argv);
    return app.run();
}
