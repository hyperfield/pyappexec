#include "CliParser.hpp"
#include "ConfigLoader.hpp"
#include "Launcher.hpp"
#include "Logger.hpp"
#include "LoggerConfigurator.hpp"
#include "SpecConfig.hpp"
#include "AppMetadata.hpp"

#include <spdlog/spdlog.h>
#if _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <string>

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

void printVersion(bool allowDialog)
{
    bool hasConsole = ensureConsoleAttached();
    std::string out = std::string(AppMetadata::kAppName) + " " + std::string(AppMetadata::kVersion) + "\n" +
        "Author: " + std::string(AppMetadata::kAuthor) + "\n" +
        "Years: " + std::string(AppMetadata::kYears) + "\n" +
        "Repo: " + std::string(AppMetadata::kGithub) + "\n";
    writeStdout(out);
#if _WIN32
    if (allowDialog && !hasConsole) {
        std::string msg = std::string(AppMetadata::kAppName) + " " + std::string(AppMetadata::kVersion) +
            "\nAuthor: " + std::string(AppMetadata::kAuthor) +
            "\nYears: " + std::string(AppMetadata::kYears) +
            "\nRepo: " + std::string(AppMetadata::kGithub);
        MessageBoxA(nullptr, msg.c_str(), "PyAppExec version", MB_OK | MB_ICONINFORMATION);
    }
#endif
}

} // namespace

int main(int argc, char** argv)
{
    CliParser parser(argc, argv);
    CliOptions options = parser.parse();
    if (options.showVersion) {
        printVersion(!options.forceCli);
        return 0;
    }
    std::filesystem::path binaryDir;
    try {
        binaryDir = std::filesystem::canonical(argv[0]).parent_path();
    } catch (const std::exception&) {
        binaryDir = std::filesystem::current_path();
    }

    Logger::initialize();

    try {
        ConfigLoader loader(options, binaryDir);
        SpecConfig specConfig = loader.load();
        LoggerConfigurator(specConfig).apply();

        Launcher launcher;
        return launcher.run(options, specConfig, loader, argc, argv);

    } catch (const std::runtime_error& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
