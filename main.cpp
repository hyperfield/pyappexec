#include "CliParser.hpp"
#include "ConfigLoader.hpp"
#include "Launcher.hpp"
#include "Logger.hpp"
#include "LoggerConfigurator.hpp"
#include "SpecConfig.hpp"

#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
    CliParser parser(argc, argv);
    CliOptions options = parser.parse();

    Logger::initialize();

    try {
        ConfigLoader loader(options);
        SpecConfig specConfig = loader.load();
        LoggerConfigurator(specConfig).apply();

        Launcher launcher;
        return launcher.run(options, specConfig, loader, argc, argv);

    } catch (const std::runtime_error& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}
