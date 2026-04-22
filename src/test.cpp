#include <QApplication>
#include <QElapsedTimer>
#include <QString>

#include "officeviewer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QElapsedTimer et;
    et.start();

    QString path = "C:\\Users\\corey\\Dev\\1.pptx";
    if (argc > 1) {
        path = QString::fromLocal8Bit(argv[1]);
    }

    ViewOptionsPrivate d;
    d.dpr         = 1;
    d.theme       = 1;
    d.path        = path;
    d.viewer_type = "officeviewer";

    ViewOptions opts;
    opts.d_ptr = &d;

    OfficeViewer viewer;
    viewer.setWindowTitle(path);
    viewer.load(nullptr, &opts);
    qDebug() << "load" << et.restart() << "ms";
    viewer.resize(viewer.getContentSize());
    viewer.show();
    qDebug() << "show" << et.restart() << "ms";

    return app.exec();
}
