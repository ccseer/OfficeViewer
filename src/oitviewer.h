#pragma once

#include <qt_windows.h>

#include <atomic>

#include "seer/viewerbase.h"

namespace Ui {
class OITViewer;
}

class OITViewer : public ViewerBase {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerBase_iid FILE "officeviewer.json")
    Q_INTERFACES(ViewerBase)
public:
    explicit OITViewer(QWidget *parent = nullptr);
    ~OITViewer() override;

    QString name() const override
    {
        return "OfficeViewer";
    }
    QSize getContentSize() const override;

    static QString getDLLPath();

protected:
    void loadImpl(QBoxLayout *layout_content,
                  QHBoxLayout *layout_control_bar) override;

    bool nativeEvent(const QByteArray &ba, void *msg, qintptr *result) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void asyncInit();
    void onDllLoaded(HMODULE lib);
    void doResize();
    bool viewFile(const QString &p);

    QWidget *m_container;

    HWND m_viewer;
    HANDLE m_lib;

    Ui::OITViewer *ui;
};

//////////////////////////////////
class DLLLoaderWorker : public QObject {
    Q_OBJECT
public:
    DLLLoaderWorker(const QString &dir);

    ~DLLLoaderWorker() override;

    void process();
    Q_SIGNAL void sigFinished(HMODULE lib);

    std::atomic<bool> m_stop;

private:
    const QString m_dir;
};
