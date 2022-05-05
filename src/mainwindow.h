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

  protected:
    virtual void closeEvent(QCloseEvent *e) override;

  private:
    void InitUi();

  private slots:
    void on_btnOpenSrt_clicked();
    void on_btnSaveSrt_clicked();
    void on_btnLoadWav_clicked();

  private:
    Ui::MainWindow *ui;
    StatusNotify *mNotif;
};
#endif // MAINWINDOW_H
