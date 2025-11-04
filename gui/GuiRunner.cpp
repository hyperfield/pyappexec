#include "gui/GuiRunner.hpp"

#include "gui/MainWindow.hpp"

#include <QApplication>
#include <QStringList>

int runGuiApplication(int argc, char** argv, const std::vector<std::string>& forwardedArgs)
{
    QApplication app(argc, argv);

    QStringList args;
    for (const auto& entry : forwardedArgs) {
        args << QString::fromStdString(entry);
    }

    MainWindow window(args);
    window.show();

    return app.exec();
}
