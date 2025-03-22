#pragma once

#include <qt_windows.h>

#include <QWidget>

namespace Ui {
class OITViewer;
}

class OITViewer : public QWidget {
    Q_OBJECT

public:
    static int translateKey(int key);

    explicit OITViewer(int wnd_index, QString p, QWidget *parent = nullptr);
    ~OITViewer() override;

    bool init();

protected:
    bool nativeEvent(const QByteArray &ba, void *msg, qintptr *result) override;

private:
    bool createViewer();
    void doResize(const QSize &sz) const;
    bool viewFile(const QString &p);

    void sendMsg2Seer(int sub_type, const QByteArray &d, HWND h) const;
    QVariant getDataFromSeerMsg(const QByteArray &ba) const;

    HWND m_viewer;
    HANDLE m_lib;
    const int m_wnd_index;
    const QString m_path;

    Ui::OITViewer *ui;
};
