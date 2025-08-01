#pragma once

#include <qt_windows.h>

#include <QTimer>
#include <atomic>

#include "seer/viewerbase.h"

class OfficeViewer : public ViewerBase {
    Q_OBJECT
public:
    explicit OfficeViewer(QWidget *parent = nullptr);
    ~OfficeViewer() override;

    QString name() const override
    {
        return "OfficeViewer";
    }
    QSize getContentSize() const override;
    void updateDPR(qreal) override;

protected:
    void loadImpl(QBoxLayout *layout_content,
                  QHBoxLayout *layout_control_bar) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    void asyncInit();
    void onDllLoaded(HMODULE lib);
    void doResize();
    bool viewFile(const QString &p);

    QWidget *m_container;
    QBoxLayout *m_layout;

    HWND m_viewer;
    HANDLE m_lib;

    QTimer m_timer_resize;
};

/////////////////////////////////
class DllLoader : public QObject {
    Q_OBJECT
public:
    DllLoader(const QString &dir);
    ~DllLoader() override;

    void process();
    Q_SIGNAL void sigFinished(HMODULE lib);

    std::atomic<bool> m_stop;

private:
    const QString m_dir;
};

/////////////////////////////////////////////////////////////////
class OfficePlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "../bin/plugin.json")
    Q_INTERFACES(ViewerPluginInterface)
public:
    ViewerBase *createViewer(QWidget *parent = nullptr) override
    {
        return new OfficeViewer(parent);
    }
};
