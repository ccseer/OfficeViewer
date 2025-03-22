#include "oitviewer.h"

#include <QDataStream>
#include <QIODevice>
#include <QScreen>
#include <QTimer>
#include <iostream>

#include "sccvw.h"
#include "seer/embedplugin.h"
#include "ui_oitviewer.h"

// has to after "sccvw.h"
#include "scclink.c"
#pragma comment(lib, "user32.lib")

OITViewer::OITViewer(int wnd_index, QString p, QWidget *parent)
    : QWidget(parent),
      m_viewer(nullptr),
      m_lib(nullptr),
      m_wnd_index(wnd_index),
      m_path(std::move(p)),
      ui(new Ui::OITViewer)
{
    ui->setupUi(this);
    // no taskbar icon
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setWindowTitle("OITViewer");
    // default size
    resize(960, 720);
}

OITViewer::~OITViewer()
{
    if (m_viewer) {
        SendMessage(m_viewer, SCCVW_CLOSEFILE, 0, 0L);
    }
    if (m_lib) {
        FreeLibrary((HMODULE)m_lib);
    }
    delete ui;
}

bool OITViewer::init()
{
    /// init() costs 200ms
    QString dir_dll = qApp->applicationDirPath() + "\\";
    m_lib           = SCCLoadViewerDLL((wchar_t *)dir_dll.utf16());
    if (m_lib == nullptr) {
        std::cout << "SCCLoadViewerDLL Err" << '\n';
        return false;
    }
    if (!createViewer()) {
        std::cout << "CreateViewer Err" << '\n';
        return false;
    }
    if (!viewFile(m_path)) {
        std::cout << "ViewFile Err" << '\n';
        return false;
    }
    QTimer::singleShot(0, this, [=]() {
        // findwnd seer ->
        if (auto h
            = FindWindowEx(nullptr, nullptr, L"SeerWindowClass", nullptr)) {
            sendMsg2Seer(SEER_OIT_SUB_LOAD_OK, {}, h);
        }
        else {
            std::cout << "FindWindowEx NULL \n";
        }
    });
    return true;
}

bool OITViewer::createViewer()
{
    std::cout << "createViewer \n";
    // If the class is something else,
    // it will call DefWindowProc for default message processing.
    m_viewer = CreateWindow(L"SCCVIEWER", L"OIT_Viewer",
                            WS_CHILD | WS_OVERLAPPED | WS_CLIPCHILDREN, 0, 0, 0,
                            0, (HWND)winId(), 0, GetModuleHandle(NULL), NULL);
    if (m_viewer == nullptr) {
        std::cout << "createViewer NULL \n";
        return false;
    }
    return true;
}

void OITViewer::doResize(const QSize &sz) const
{
    if (m_viewer && IsWindow(m_viewer)) {
        InvalidateRect(m_viewer, NULL, 0);
        MoveWindow(m_viewer, 0, 0, sz.width(), sz.height(), TRUE);
        ShowWindow(m_viewer, SW_SHOW);
        std::cout << "resized " << sz.width() << " " << sz.height() << '\n';
    }
}

bool OITViewer::viewFile(const QString &p)
{
    std::cout << "viewFile" << '\n';
    if (!IsWindow(m_viewer)) {
        std::cout << "viewFile IsWindow" << '\n';
        return false;
    }

    SCCVWVIEWFILE40 lvf = {};
    lvf.dwSize          = sizeof(SCCVWVIEWFILE40);
    // IMPORTANT
    lvf.dwSpecType      = IOTYPE_UNICODEPATH;
    lvf.pSpec           = (VTVOID *)(wchar_t *)p.utf16();
    lvf.dwViewAs        = 0;
    lvf.bUseDisplayName = FALSE;
    lvf.bDeleteOnClose  = FALSE;
    lvf.dwFlags         = 0;
    lvf.dwReserved1     = 0;
    lvf.dwReserved2     = 0;
    const auto ret = SendMessage(m_viewer, SCCVW_VIEWFILE, 0, (LPARAM)&lvf);
    if (SCCVWERR_OK != ret) {
        std::cout << "viewFile err " << ret << '\n';
        return false;
    }
    doResize(size());
    return true;
}

void OITViewer::sendMsg2Seer(int sub_type, const QByteArray &d, HWND h) const
{
    std::cout << "sendMsg2Seer " << m_wnd_index << " " << sub_type << '\n';

    auto hwnd_parent = GetAncestor(h, GA_ROOT);

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds.setVersion(QDataStream::Qt_5_15);
    OITData oitd{sub_type, m_wnd_index, d};
    ds << oitd;

    COPYDATASTRUCT cd;
    cd.cbData = ba.size();
    cd.lpData = (void *)ba.data();
    cd.dwData = SEER_OIT_MSG;
    SendMessage(hwnd_parent, WM_COPYDATA, 0, (LPARAM)(LPVOID)&cd);
}

QVariant OITViewer::getDataFromSeerMsg(const QByteArray &ba) const
{
    QDataStream ds(ba);
    ds.setVersion(QDataStream::Qt_5_15);
    QVariant v;
    ds >> v;
    return v;
}

bool OITViewer::nativeEvent(const QByteArray &ba, void *msg, qintptr *result)
{
    MSG *m = (MSG *)msg;
    switch (m->message) {
    case SCCVW_CONTEXTMENU: {
        // return 0, the Viewer will pop up its own context menu.
        // If the return value is anything but 0, the Viewer does nothing.
        *result = -1;
        // if you want to stop the event being handled by Qt,
        // return true and set result.
        return true;
    }
    case SCCVW_BAILOUT: {
        // file viewing has stopped
        QString p;
        switch ((DWORD)m->lParam) {
        case SCCVW_BAILOUT_MEMORY:
            p = "Memory Failure!";
            break;
        case SCCVW_BAILOUT_STREAMBAIL:
            p = "Filter Bailed Out!";
            break;
        case SCCVW_BAILOUT_FILEOPENFAILED:
            p = "File Open Failed!";
            break;
        case SCCVW_BAILOUT_MISSINGELEMENT:
            p = "Couldn't Find Needed Element!";
            break;
        case SCCVW_BAILOUT_BADFILE:
            p = "Bad File Error!";
            break;
        case SCCVW_BAILOUT_PROTECTEDFILE:
            p = "This is Protected File!";
            break;
        case SCCVW_BAILOUT_SUPFILEOPENFAILS:
            p = "Needed Files Not Found!";
            break;
        case SCCVW_BAILOUT_EMPTYFILE:
            p = "This is an Empty File!";
            break;
        case SCCVW_BAILOUT_EMPTYSECTION:
            p = "This section is empty!";
            break;
        case SCCVW_BAILOUT_NOFILTER:
            p = "No Filter Available for this type!";
            break;
        case SCCVW_BAILOUT_WRITEERROR:
            p = "There was a write Error!";
            break;
        case SCCVW_BAILOUT_FILECHANGED:
            p = "The file has changed!";
            break;
        case SCCVW_BAILOUT_GPFAULT:
            p = "General Protection Fault!";
            break;
        case SCCVW_BAILOUT_DIVIDEBYZERO:
            p = "Divide by Zero Error!";
            break;
        case SCCVW_BAILOUT_NOSUPPORTEDFILE:
            p = "This file type is not supported!";
            break;
        case SCCVW_BAILOUT_OTHEREXCEPTION:
            p = "General Exception!";
            break;
        case SCCVW_BAILOUT_NOENGINE:
            p = "Display Engine Not Available!";
            break;
        case SCCVW_BAILOUT_UNKNOWNNOTVIEWED:
            p = "Unknown File Type - Not Viewed.";
            break;
        case SCCVW_BAILOUT_FILTERTIMEOUT:
            p = "The Filter Timed Out!";
            break;
        case SCCVW_BAILOUT_UNKNOWN:
        default:
            p = "Unknown Error!";
            break;
        }
        std::cout << "file viewing has stopped " << p.toStdString() << '\n';

        *result = -1;
        sendMsg2Seer(SEER_OIT_SUB_LOAD_ERR, {}, m->hwnd);
        return true;
    }
    case SCCVW_KEYDOWN: {
        if (auto key = translateKey(m->lParam)) {
            int mod = 0;
            if (0xF0 & GetKeyState(VK_CONTROL)) {
                mod += Qt::CTRL;
            }
            if (0xF0 & GetKeyState(VK_SHIFT)) {
                mod += Qt::SHIFT;
            }
            if (0xF0 & GetKeyState(VK_MENU)) {
                mod += Qt::ALT;
            }

            // KBDLLHOOKSTRUCT key = *((KBDLLHOOKSTRUCT *)m->lParam);
            QKeySequence ks(mod + key);
            if (ks.matches(QKeySequence::Copy)) {
                auto p = SendMessage(m_viewer, SCCVW_COPYTOCLIP, 0, 0L);
                std::cout << "KEYDOWN copy " << p << '\n';
            }
            else {
                QByteArray ba;
                QDataStream ds(&ba, QIODevice::WriteOnly);
                ds.setVersion(QDataStream::Qt_5_15);
                ds << (mod + key);
                sendMsg2Seer(SEER_OIT_SUB_KEY_PRESS, ba, m->hwnd);
            }
        }
        break;
    }
    case WM_COPYDATA: {
        auto cds = (PCOPYDATASTRUCT)m->lParam;
        if (!cds) {
            break;
        }
        if (cds->dwData == QEvent::Quit) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
        else if (cds->dwData == SEER_OIT_SIZE_CHANGED) {
            const QVariant v = getDataFromSeerMsg(
                QByteArray(reinterpret_cast<char *>(cds->lpData), cds->cbData));
            const QSize sz = v.toSize();
            std::cout << "SEER_OIT_SIZE_CHANGED " << sz.width() << " "
                      << sz.height() << '\n';
            if (sz.isValid()) {
                doResize(sz);
            }
            else {
                doResize(size());
            }
        }
        break;
    }
    }
    return QWidget::nativeEvent(ba, msg, result);
}

int OITViewer::translateKey(int key)
{
    // QxtGlobalShortcutPrivate::nativeKeycode(Qt::Key key)
    switch (key) {
    case VK_ESCAPE:
        return Qt::Key_Escape;
    case VK_TAB:
        // case Qt::Key_Backtab:
        return Qt::Key_Tab;
    case VK_BACK:
        return Qt::Key_Backspace;
    case VK_RETURN:
        // case Qt::Key_Return:
        return Qt::Key_Enter;
    case VK_INSERT:
        return Qt::Key_Insert;
    case VK_DELETE:
        return Qt::Key_Delete;
    case VK_PAUSE:
        return Qt::Key_Pause;
    case VK_PRINT:
        return Qt::Key_Print;
    case VK_CLEAR:
        return Qt::Key_Clear;
    case VK_HOME:
        return Qt::Key_Home;
    case VK_END:
        return Qt::Key_End;
    case VK_LEFT:
        return Qt::Key_Left;
    case VK_UP:
        return Qt::Key_Up;
    case VK_RIGHT:
        return Qt::Key_Right;
    case VK_DOWN:
        return Qt::Key_Down;
    case VK_PRIOR:
        return Qt::Key_PageUp;
    case VK_NEXT:
        return Qt::Key_PageDown;
    case VK_F1:
        return Qt::Key_F1;
    case VK_F2:
        return Qt::Key_F2;
    case VK_F3:
        return Qt::Key_F3;
    case VK_F4:
        return Qt::Key_F4;
    case VK_F5:
        return Qt::Key_F5;
    case VK_F6:
        return Qt::Key_F6;
    case VK_F7:
        return Qt::Key_F7;
    case VK_F8:
        return Qt::Key_F8;
    case VK_F9:
        return Qt::Key_F9;
    case VK_F10:
        return Qt::Key_F10;
    case VK_F11:
        return Qt::Key_F11;
    case VK_F12:
        return Qt::Key_F12;
    case VK_F13:
        return Qt::Key_F13;
    case VK_F14:
        return Qt::Key_F14;
    case VK_F15:
        return Qt::Key_F15;
    case VK_F16:
        return Qt::Key_F16;
    case VK_F17:
        return Qt::Key_F17;
    case VK_F18:
        return Qt::Key_F18;
    case VK_F19:
        return Qt::Key_F19;
    case VK_F20:
        return Qt::Key_F20;
    case VK_F21:
        return Qt::Key_F21;
    case VK_F22:
        return Qt::Key_F22;
    case VK_F23:
        return Qt::Key_F23;
    case VK_F24:
        return Qt::Key_F24;
    case VK_SPACE:
        return Qt::Key_Space;
    case VK_MULTIPLY:
        return Qt::Key_Asterisk;
    case VK_ADD:
        return Qt::Key_Plus;
    case VK_SEPARATOR:
        return Qt::Key_Comma;
    case VK_SUBTRACT:
        return Qt::Key_Minus;
    case VK_DIVIDE:
        return Qt::Key_Slash;
    }

    // numbers
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return key;
    }
    // letters
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return key;
    }
    return 0;
}
