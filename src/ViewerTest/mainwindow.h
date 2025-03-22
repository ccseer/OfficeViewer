#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>

QT_BEGIN_NAMESPACE
namespace Ui {
	class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private slots:
	void on_pushButton_clicked();

private:
	QProcess m_pro;

	HWND m_viewer;
	Ui::MainWindow* ui;

	// QWidget interface
protected:
	virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
};
#endif  // MAINWINDOW_H
