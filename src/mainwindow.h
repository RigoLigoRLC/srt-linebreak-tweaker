#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "statusnotify.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

  private:
    void InitUi();

  private slots:
    void on_btnOpenSrt_clicked();

    void on_btnSaveSrt_clicked();

  private:
    Ui::MainWindow *ui;
    StatusNotify *mNotif;
};
#endif // MAINWINDOW_H
