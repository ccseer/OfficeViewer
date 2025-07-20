#include <QApplication>
#include <QElapsedTimer>
#include <QString>

#include "officeviewer.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QElapsedTimer et;
    et.start();
    OfficeViewer* viewer = new OfficeViewer;

    auto p      = std::make_unique<ViewOptions>();
    p->d->dpr   = 1;
    p->d->theme = 1;
    p->d->path  = "C:/D/1.pptx";
    // p->d->path  = "D:/1.pptx";
    p->d->type  = viewer->name();
    viewer->setWindowTitle(p->d->path);
    viewer->load(nullptr, std::move(p));
    qDebug() << "load" << et.restart() << "ms";
    viewer->resize(viewer->getContentSize());
    viewer->show();
    qDebug() << "show" << et.restart() << "ms";
    // QTimer::singleShot(1000, [=](){
    //     delete viewer;
    // });

    return app.exec();
}
