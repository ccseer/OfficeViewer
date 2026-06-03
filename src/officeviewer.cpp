#include "officeviewer.h"

#include <QDir>
#include <QMetaObject>
#include <QPointer>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QThread>

#include "sccvw.h"
// sequence matters, must be included after sccvw.h
#include "scclink.c"
#include "sccop.h"
#include "seer/viewerhelper.h"

#pragma comment(lib, "user32.lib")

#define qprintt qDebug() << "[officeviewer]"
LRESULT CALLBACK ViewerWndProc(HWND hwnd,
                               UINT msg,
                               WPARAM wParam,
                               LPARAM lParam);

namespace {
constexpr auto g_prop_key = L"OfficeViewer";
constexpr int kZoomMinPercent = 25;
constexpr int kZoomMaxPercent = 500;
constexpr int kZoomStepPercent = 10;

bool setViewerOption(HWND viewer,
                     VTDWORD optionId,
                     VTVOID *data,
                     const char *name)
{
    if (!viewer || !IsWindow(viewer)) {
        return false;
    }

    SCCVWOPTIONSPEC40 optionSpec = {};
    optionSpec.dwSize            = sizeof(SCCVWOPTIONSPEC40);
    optionSpec.dwId              = optionId;
    optionSpec.dwFlags           = SCCVWOPTION_CURRENT;
    optionSpec.pData             = data;

    const auto ret
        = SendMessage(viewer, SCCVW_SETOPTION, 0, (LPARAM)&optionSpec);
    if (ret != SCCVWERR_OK) {
        qprintt << "SCCVW_SETOPTION failed" << name << optionId << ret;
        return false;
    }

    return true;
}

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
}  // namespace

OfficeViewer::OfficeViewer(QWidget *parent)
    : ViewerBase(parent),
      m_container(nullptr),
      m_layout(nullptr),
      m_btn_zoom(nullptr),
      m_viewer(nullptr),
      m_lib(nullptr),
      m_zoom_percent(100)
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
        RemoveProp(m_viewer, g_prop_key);
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
    const auto sz_def = options()->dpr() * QSize(800, 700);
    auto cmd
        = options()->property(ViewOptionsKeys::kKeyPluginCmd).toStringList();
    if (!cmd.isEmpty()) {
        auto parsed = seer::parseViewerSizeFromConfig(cmd);
        qprintt << "getContentSize: parsed" << parsed << cmd;
        if (parsed.isValid()) {
            return parsed;
        }
    }
    return sz_def;
}

void OfficeViewer::updateDPR(qreal r)
{
    if (m_layout) {
        m_layout->setContentsMargins(r * 9, r * 9, r * 9, r * 9);
    }
}

void OfficeViewer::loadImpl(QBoxLayout *layout_content,
                            QHBoxLayout *layout_control_bar)
{
    if (layout_control_bar) {
        m_btn_zoom = new QPushButton(this);
        m_btn_zoom->setFlat(true);
        m_btn_zoom->setFocusPolicy(Qt::NoFocus);
        m_btn_zoom->setCursor(Qt::PointingHandCursor);
        m_btn_zoom->setToolTip("Reset zoom");
        syncZoomButton();
        connect(m_btn_zoom, &QPushButton::clicked, this, &OfficeViewer::resetZoom);
        layout_control_bar->addWidget(m_btn_zoom);
        layout_control_bar->addStretch();
    }
    m_layout    = layout_content;
    m_container = new QWidget(this);
    m_container->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    m_container->setAttribute(Qt::WA_NativeWindow, true);
    layout_content->addWidget(m_container);

    updateDPR(options()->dpr());
    m_timer_resize.setSingleShot(true);
    m_timer_resize.setInterval(200);
    connect(&m_timer_resize, &QTimer::timeout, this, &OfficeViewer::doResize);

    asyncInit();
}

void OfficeViewer::asyncInit()
{
    QString dir_dll = seer::getDLLPath();
    if (dir_dll.isEmpty()) {
        qprintt << "dir dll";
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }
    dir_dll.replace("/", "\\");
    if (!dir_dll.endsWith("\\")) {
        dir_dll.append("\\");
    }
    auto thread           = new Thread;
    QPointer worker       = new DllLoader(dir_dll);
    QPointer<OfficeViewer> self(this);
    connect(this, &QObject::destroyed, [worker]() {
        if (worker) {
            qprintt << "ui destroyed, stopping thread";
            worker->m_stop.store(true);
        }
    });
    connect(worker, &DllLoader::sigFinished, worker, &QObject::deleteLater);
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    connect(worker, &DllLoader::sigFinished, worker, [self](HMODULE lib) {
        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, lib]() {
                    if (self) {
                        self->onDllLoaded(lib);
                    } else if (lib) {
                        FreeLibrary(lib);
                    }
                },
                Qt::QueuedConnection);
        } else if (lib) {
            FreeLibrary(lib);
        }
    });
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
    VirtualQuery((void *)seer::getDLLPath, &mbi, sizeof(mbi));
    m_viewer = CreateWindow(L"SCCVIEWER", L"OIT_Viewer",
                            WS_CHILD | WS_OVERLAPPED | WS_CLIPCHILDREN, 0, 0, 0,
                            0, (HWND)m_container->winId(), 0,
                            (HMODULE)mbi.AllocationBase, NULL);
    if (!m_viewer) {
        qprintt << "CreateViewer Err" << GetLastError();
        emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
        return;
    }

    LONG_PTR origProc = GetWindowLongPtr(m_viewer, GWLP_WNDPROC);
    SetWindowLongPtr(m_viewer, GWLP_USERDATA, origProc);
    SetWindowLongPtr(m_viewer, GWLP_WNDPROC, (LONG_PTR)ViewerWndProc);

    SetProp(m_viewer, g_prop_key, this);

    VTDWORD wpDisplayMode = SCCVW_WPMODE_NORMAL;
    setViewerOption(m_viewer, SCCOPT_WPDISPLAYMODE, &wpDisplayMode,
                    "SCCOPT_WPDISPLAYMODE");

    VTBOOL useDocPageSettings = FALSE;
    setViewerOption(m_viewer, SCCOPT_USEDOCPAGESETTINGS, &useDocPageSettings,
                    "SCCOPT_USEDOCPAGESETTINGS");

    VTDWORD dialogFlags = SCCVW_DIALOG_NOADDOPTIONSTOMENU
                          | SCCVW_DIALOG_NOADDPRINTTOMENU
                          | SCCVW_DIALOG_NOADDDOPRINTTOMENU;
    setViewerOption(m_viewer, SCCOPT_DIALOGFLAGS, &dialogFlags,
                    "SCCOPT_DIALOGFLAGS");

    // Disable all print-related functionality
    disablePrintFeatures();

    if (!viewFile(options()->path())) {
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
    // only the SCCVWVIEWFILE80 structure is truly intended to accept Unicode.
    SCCVWVIEWFILE80 lvf = {};
    lvf.dwSize          = sizeof(SCCVWVIEWFILE80);
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

void OfficeViewer::disablePrintFeatures()
{
    if (!m_viewer || !IsWindow(m_viewer)) {
        return;
    }

    qprintt << "Disabling print features";

    VTDWORD whatToPrint = SCCVW_PRINT_ALLPAGES;
    setViewerOption(m_viewer, SCCOPT_WHATTOPRINT, &whatToPrint,
                    "SCCOPT_WHATTOPRINT");

    VTDWORD printCopies = 1;
    setViewerOption(m_viewer, SCCOPT_PRINTCOPIES, &printCopies,
                    "SCCOPT_PRINTCOPIES");

    VTDWORD wpPrintMode = 0;
    setViewerOption(m_viewer, SCCOPT_WPPRINTMODE, &wpPrintMode,
                    "SCCOPT_WPPRINTMODE");
}

bool OfficeViewer::applyZoomPercent(int percent)
{
    if (!m_viewer || !IsWindow(m_viewer)) {
        return false;
    }

    const int clamped = qBound(kZoomMinPercent, percent, kZoomMaxPercent);
    VTDWORD wpValue = static_cast<VTDWORD>(clamped);
    SCCVWIMAGEZOOM imageZoom = {};
    imageZoom.dwXNu = clamped;
    imageZoom.dwXDe = 100;
    imageZoom.dwYNu = clamped;
    imageZoom.dwYDe = 100;

    SCCVWDISPLAYINFO80 displayInfo = {};
    displayInfo.dwSize = sizeof(SCCVWDISPLAYINFO80);
    const auto displayRet
        = SendMessage(m_viewer, SCCVW_GETDISPLAYINFO, 0, (LPARAM)&displayInfo);

    const bool wpOk = setViewerOption(m_viewer, SCCOPT_WPZOOM, &wpValue,
                                      "SCCOPT_WPZOOM");
    bool graphicOk = false;
    if (displayRet == SCCVWERR_OK) {
        if (displayInfo.dwType == SCCVWTYPE_IMAGE) {
            graphicOk = setViewerOption(m_viewer, SCCOPT_BMPZOOM, &imageZoom,
                                        "SCCOPT_BMPZOOM");
        } else if (displayInfo.dwType == SCCVWTYPE_VECTOR) {
            graphicOk = setViewerOption(m_viewer, SCCOPT_VECZOOM, &imageZoom,
                                        "SCCOPT_VECZOOM");
        }
    }

    if (!wpOk && !graphicOk) {
        return false;
    }

    m_zoom_percent = clamped;
    syncZoomButton();
    qprintt << "Zoom percent:" << m_zoom_percent;
    return true;
}

void OfficeViewer::stepZoom(int deltaPercent)
{
    applyZoomPercent(m_zoom_percent + deltaPercent);
}

void OfficeViewer::resetZoom()
{
    applyZoomPercent(100);
}

void OfficeViewer::syncZoomButton()
{
    if (!m_btn_zoom) {
        return;
    }

    m_btn_zoom->setText(QString::number(m_zoom_percent) + "%");
    m_btn_zoom->setEnabled(m_zoom_percent != 100);
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
    case SCCVW_PRINT:
    case SCCVW_PRINTSETUP:
    case SCCVW_PRINTEX:
        // Block all print-related messages
        qprintt << "Print message blocked:" << msg;
        return 1;  // Return non-zero to indicate message was handled
    case SCCVW_CONTEXTMENU: {
        // Seer does not expose OIT's built-in print flow.
        return 1;
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
        OfficeViewer *viewer = (OfficeViewer *)GetProp(hwnd, g_prop_key);

        // Ctrl+C: Copy to clipboard
        if (ctrl && lParam == 'C') {
            SendMessage(hwnd, SCCVW_COPYTOCLIP, 0, 0L);
        }
        else if (viewer
                 && (lParam == VK_ADD || (ctrl && lParam == VK_OEM_PLUS))) {
            viewer->stepZoom(kZoomStepPercent);
            return 1;
        }
        else if (viewer
                 && (lParam == VK_SUBTRACT || (ctrl && lParam == VK_OEM_MINUS))) {
            viewer->stepZoom(-kZoomStepPercent);
            return 1;
        }
        // Esc or Ctrl+W: Close the preview window
        else if (lParam == VK_ESCAPE || (ctrl && lParam == 'W')) {
            if (viewer) {
                qprintt << "Close window requested via keyboard shortcut";
                // Use QMetaObject::invokeMethod to safely call from Windows callback
                QMetaObject::invokeMethod(
                    viewer,
                    [viewer]() {
                        viewer->sigCommand(ViewCommandType::VCT_HideWindow, 0);
                    },
                    Qt::QueuedConnection);
            }
        }
        break;
    }
    case WM_MOUSEWHEEL: {
        if ((GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0) {
            if (OfficeViewer *viewer = (OfficeViewer *)GetProp(hwnd, g_prop_key)) {
                const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta > 0) {
                    viewer->stepZoom(kZoomStepPercent);
                } else if (delta < 0) {
                    viewer->stepZoom(-kZoomStepPercent);
                }
                return 1;
            }
        }
        break;
    }
    }

    if (WNDPROC origProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA)) {
        return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
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
