#include "CliParser.hpp"
#include "ConfigLoader.hpp"
#include "LauncherCli.hpp"
#include "Logger.hpp"
#include "LoggerConfigurator.hpp"
#include "SpecConfig.hpp"
#include "AppMetadata.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    CliParser parser(argc, argv);
    CliOptions options = parser.parse();
    options.forceCli = true;
    if (options.showVersion) {
        std::cout << AppMetadata::kAppName << " " << AppMetadata::kVersion << "\n"
                  << "Author: " << AppMetadata::kAuthor << "\n"
                  << "Years: " << AppMetadata::kYears << "\n"
                  << "Repo: " << AppMetadata::kGithub << "\n";
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

        LauncherCli launcher;
        return launcher.run(specConfig, loader);

    } catch (const std::runtime_error& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
