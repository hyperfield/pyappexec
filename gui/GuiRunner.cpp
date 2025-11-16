#include "gui/GuiRunner.hpp"

#include "gui/MainWindow.hpp"

#include <QApplication>
#include <QStringList>

int runGuiApplication(int argc,
                      char** argv,
                      const std::vector<std::string>& forwardedArgs,
                      const std::string& guiStatePath,
                      const std::string& appDisplayName,
                      bool autoSuppressAfterSuccess)
{
    QApplication app(argc, argv);

    QStringList args;
    for (const auto& entry : forwardedArgs) {
        args << QString::fromStdString(entry);
    }

    MainWindow window(args,
                      QString::fromStdString(guiStatePath),
                      QString::fromStdString(appDisplayName),
                      autoSuppressAfterSuccess);
    window.show();

    return app.exec();
}
