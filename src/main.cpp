#include <QApplication>
#include <QFile>
#include <QStandardPaths>

#include "oitviewer.h"
#include "seer/embedplugin.h"

int main(int argc, char* argv[])
{
    // no attribute if added to another window
    //    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    //    QApplication::setHighDpiScaleFactorRoundingPolicy(
    //        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);
    if (argc != 3) {
        return ERR_BAD_ARG;
    }
    const auto args = a.arguments();
    const auto& p   = args[2];
    if (!QFile::exists(p)) {
        return ERR_FILE_NOT_FOUND;
    }


    OITViewer w(args[1].toInt(), p);
    // argv[1] -> only english characters can be read
    // auto ret = w.init("H:/1.ppt");
    if (!w.init()) {
        return ERR_PROCESS;
    }
    return a.exec();
}
