#include "officeviewer.h"

#include <QDir>
#include <QPointer>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QThread>

#include "sccvw.h"

#pragma comment(lib, "user32.lib")
// need to be included below sccvw.h
#include "scclink.c"

#define qprintt qDebug() << "[officeviewer]"
LRESULT CALLBACK ViewerWndProc(HWND hwnd,
                               UINT msg,
                               WPARAM wParam,
                               LPARAM lParam);

class Thread : public QThread {
    using QThread::run;

public:
    ~Thread() override
    {
        quit();
        wait();
        qprintt << "~" << this;
    }
};

OfficeViewer::OfficeViewer(QWidget *parent)
    : ViewerBase(parent),
      m_container(nullptr),
      m_layout(nullptr),
      m_viewer(nullptr),
      m_lib(nullptr)
{
    // a.setOrganizationName("Corey");
    // a.setApplicationName("Seer");
    // a.setOrganizationDomain("https://1218.io");
    // a.setApplicationDisplayName("OfficeViewer");

    // will generate a .oit cache directory at runtime
    // put it under Seer folder
    qputenv("OIT_DATA_PATH", QStandardPaths::writableLocation(
                                 QStandardPaths::AppLocalDataLocation)
                                 .toUtf8());
    qprintt << this;
}

OfficeViewer::~OfficeViewer()
{
    qprintt << "~" << this;
    if (m_viewer) {
        SendMessage(m_viewer, SCCVW_CLOSEFILE, 0, 0L);
        DestroyWindow(m_viewer);
        m_viewer = 0;
    }
    if (m_lib) {
        FreeLibrary((HMODULE)m_lib);
    }
}

QSize OfficeViewer::getContentSize() const
{
    return m_d->d->dpr * QSize(800, 700);
}

void OfficeViewer::updateDPR(qreal r)
{
    m_d->d->dpr = r;
    if (m_layout) {
        m_layout->setContentsMargins(r * 9, r * 9, r * 9, r * 9);
    }
}

void OfficeViewer::loadImpl(QBoxLayout *layout_content,
                            QHBoxLayout *layout_control_bar)
{
    if (layout_control_bar) {
        layout_control_bar->addStretch();
    }
    m_layout    = layout_content;
    m_container = new QWidget(this);
    m_container->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    m_container->setAttribute(Qt::WA_NativeWindow, true);
    layout_content->addWidget(m_container);

    updateDPR(m_d->d->dpr);
    m_timer_resize.setSingleShot(true);
    m_timer_resize.setInterval(200);
    connect(&m_timer_resize, &QTimer::timeout, this, &OfficeViewer::doResize);

    asyncInit();
}

void OfficeViewer::asyncInit()
{
    QString dir_dll = getDLLPath();
    if (dir_dll.isEmpty()) {
        qprintt << "dir dll";
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }
    dir_dll.replace("/", "\\");
    if (!dir_dll.endsWith("\\")) {
        dir_dll.append("\\");
    }
    auto thread     = new Thread;
    QPointer worker = new DllLoader(dir_dll);
    connect(this, &QObject::destroyed, [worker]() {
        if (worker) {
            qprintt << "ui destroyed, stopping thread";
            worker->m_stop.store(true);
        }
    });
    connect(worker, &DllLoader::sigFinished, worker, &QObject::deleteLater);
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    //
    connect(worker, &DllLoader::sigFinished, this, &OfficeViewer::onDllLoaded);
    connect(thread, &QThread::started, worker, &DllLoader::process);
    worker->moveToThread(thread);
    thread->start();
}

void OfficeViewer::onDllLoaded(HMODULE lib)
{
    qprintt << "onDllLoaded" << lib;
    if (!lib) {
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }
    m_lib                        = lib;
    MEMORY_BASIC_INFORMATION mbi = {};
    VirtualQuery((void *)getDLLPath, &mbi, sizeof(mbi));
    m_viewer = CreateWindow(
        L"SCCVIEWER", L"OIT_Viewer", WS_CHILD | WS_OVERLAPPED | WS_CLIPCHILDREN,
        0, 0, 0, 0, (HWND)m_container->winId(), 0, (HMODULE)mbi.AllocationBase,
        // GetModuleHandle(nullptr),
        NULL);
    if (!m_viewer) {
        qprintt << "CreateViewer Err" << GetLastError();
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }

    LONG_PTR origProc = GetWindowLongPtr(m_viewer, GWLP_WNDPROC);
    SetWindowLongPtr(m_viewer, GWLP_USERDATA, origProc);
    SetWindowLongPtr(m_viewer, GWLP_WNDPROC, (LONG_PTR)ViewerWndProc);

    // SendMessage(m_viewer, SCCVW_SETOPTION)
    if (!viewFile(m_d->d->path)) {
        qprintt << "ViewFile Err";
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }

    emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
    QTimer::singleShot(0, this, &OfficeViewer::doResize);
}

bool OfficeViewer::viewFile(const QString &p)
{
    qprintt << "viewFile";
    if (!IsWindow(m_viewer)) {
        qprintt << "viewFile IsWindow";
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
        qprintt << "viewFile err " << ret;
        return false;
    }
    return true;
}

void OfficeViewer::doResize()
{
    if (!m_viewer || !IsWindow(m_viewer) || !m_container) {
        return;
    }
    RECT rt;
    if (!GetClientRect((HWND)m_container->winId(), &rt)) {
        qprintt << "doResize: GetClientRect error";
        return;
    }

    InvalidateRect(m_viewer, NULL, 0);
    MoveWindow(m_viewer, 0, 0, rt.right - rt.left, rt.bottom - rt.top, TRUE);
    ShowWindow(m_viewer, SW_SHOW);
    qprintt << "doResize" << rt.right - rt.left << rt.bottom - rt.top;
}

void OfficeViewer::resizeEvent(QResizeEvent *event)
{
    ViewerBase::resizeEvent(event);
    // doResize();
    m_timer_resize.start();
}

LRESULT CALLBACK ViewerWndProc(HWND hwnd,
                               UINT msg,
                               WPARAM wParam,
                               LPARAM lParam)
{
    switch (msg) {
    case SCCVW_CONTEXTMENU: {
        // return 0, the Viewer will pop up its own context menu.
        // If the return value is anything but 0, the Viewer does nothing.
        // if you want to stop the event being handled by Qt,
        // return true and set result.
        return 0;
    }
    case SCCVW_BAILOUT: {
        // file viewing has stopped
        QString p;
        switch (lParam) {
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
        qprintt << "file viewing has stopped" << p;

        // emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        break;
    }
    case SCCVW_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000);
        if (ctrl && lParam == 'C') {
            // hwnd == m_viewer
            SendMessage(hwnd, SCCVW_COPYTOCLIP, 0, 0L);
        }
        break;
    }
    }

    if (WNDPROC origProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA)) {
        return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

QString OfficeViewer::getDLLPath()
{
    MEMORY_BASIC_INFORMATION mbi = {};
    VirtualQuery((void *)getDLLPath, &mbi, sizeof(mbi));
    HMODULE hm = (HMODULE)mbi.AllocationBase;

    QString dir;
    TCHAR path[MAX_PATH] = {};
    if (hm && GetModuleFileName(hm, path, MAX_PATH)) {
        dir = QString::fromWCharArray(path);
        dir = QFileInfo(dir).absoluteDir().absolutePath();
    }
    else {
        qprintt << "GetModuleFileName" << GetLastError();
    }
    return dir;
}

//////////////////////////////////////////////////////////////////////
DllLoader::DllLoader(const QString &dir) : m_stop(false), m_dir(dir)
{
    qprintt << this;
}

DllLoader::~DllLoader()
{
    qprintt << "~" << this;
}

void DllLoader::process()
{
    if (m_stop.load()) {
        qprintt << "DLLLoaderWorker::process quit early";
        emit sigFinished(nullptr);
        return;
    }
    HMODULE lib = SCCLoadViewerDLL((wchar_t *)m_dir.utf16());
    if (m_stop.load()) {
        if (lib) {
            FreeLibrary(lib);
        }
        emit sigFinished(nullptr);
        return;
    }
    if (!lib) {
        qprintt << "SCCLoadViewerDLL failed" << m_dir;
    }
    emit sigFinished(lib);
}
