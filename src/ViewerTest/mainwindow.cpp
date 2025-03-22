#include "mainwindow.h"

#include <QAction>
#include <QTimer>
#include <QWindow>

#include "ui_mainwindow.h"

#pragma comment(lib, "User32.lib")
#include <qt_windows.h>

struct ViewerData {
	// in
	qint64 pro_id;
	// out
	HWND wnd;
};

BOOL CALLBACK EnumWindows4Oit(HWND hwnd, LPARAM lParam)
{
	DWORD dwProcessID = 0;
	ViewerData& ret = *((ViewerData*)lParam);
	GetWindowThreadProcessId(hwnd, &dwProcessID);
	if (dwProcessID == ret.pro_id) {
		TCHAR buffer[256];
		int written = GetWindowText(hwnd, buffer, 256);
		if (written
			&& QString::fromWCharArray(buffer).startsWith("OITViewer")) {
			ret.wnd = hwnd;
			return FALSE;
		}
	}
	return TRUE;
}

//////////////////////////////////
MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	QAction* a = new QAction(this);
	a->setShortcut(QKeySequence::Copy);
	this->addAction(a);
	connect(a, &QAction::triggered, this, [=](bool a) { qDebug(); });
	connect(&m_pro, &QProcess::readyReadStandardOutput, this,
		[=]() { qDebug() << m_pro.readAllStandardOutput(); });

	if (auto ws_wnd = FindWindowEx(nullptr, nullptr, L"SeerWindowClass", nullptr)) {
		ui->pushButton->setText("222");
	}
}

MainWindow::~MainWindow()
{
	qDebug() << m_pro.readAllStandardOutput();
	m_pro.kill();
	delete ui;
}

void MainWindow::on_pushButton_clicked()
{
	constexpr auto exe = "your/path/to/OitViewer.exe";
	m_pro.start(exe, { "H:/1.ppt" });
	QTimer::singleShot(1000, this, [=]() {
		//  if (m_pro.state() != m_pro.Running) {
		//      qDebug() << "error";
		//      return;
		//  }

		ViewerData h{ m_pro.processId(), NULL };
		EnumWindows(EnumWindows4Oit, (LPARAM)&h);
		if (h.wnd) {
			m_viewer = h.wnd;
			auto alien = QWindow::fromWinId((WId)h.wnd);
			auto wnd = QWidget::createWindowContainer(alien);
			ui->widget->layout()->addWidget(wnd);
			return;
		}
		});
}

bool MainWindow::nativeEvent(const QByteArray& ba, void* msg, long* result)
{
	MSG* m = (MSG*)msg;
	switch (m->message) {
	case WM_COPYDATA: {
		auto cds = (PCOPYDATASTRUCT)m->lParam;
		if (cds && m_viewer) {
			if (cds->dwData == QEvent::KeyPress) {
				QByteArray ba(reinterpret_cast<char*>(cds->lpData),
					cds->cbData);
				QDataStream ds(ba);
				int key = 0;
				ds >> key;

				qDebug() << QKeySequence(key).toString();
			}
			//   if (cds->dwData == 100) {
			//       SendMessage(m_viewer, SCCVW_COPYTOCLIP, 0, 0L);
			//   }
			//   else if (cds->dwData == 0) {
			//       QTimer::singleShot(0, qApp, &QCoreApplication::quit);
			//   }
		}
		break;
	}
	};
	return QWidget::nativeEvent(ba, msg, result);
}
