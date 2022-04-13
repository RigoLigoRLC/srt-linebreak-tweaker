#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  InitUi();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::InitUi()
{
  ui->reorg->SetScrollBars(ui->barHoriz, ui->barVert);
  connect(ui->actUndo, &QAction::triggered,
          ui->reorg, &Reorganizer::Undo);
  connect(ui->actRedo, &QAction::triggered,
          ui->reorg, &Reorganizer::Redo);
}

void MainWindow::on_btnOpenSrt_clicked()
{
  auto f = QFileDialog::getOpenFileName(this,
                                        tr("Open SRT file"),
                                        qApp->applicationDirPath(),
                                        tr("SRT file (*.srt);;All files (*.*)"));
  ui->reorg->OpenFile(f);
}


void MainWindow::on_btnSaveSrt_clicked()
{
  auto f = QFileDialog::getSaveFileName(this,
                                        tr("Save SRT file"),
                                        qApp->applicationDirPath(),
                                        tr("SRT file (*.srt);;All files (*.*)"));
  ui->reorg->SaveFile(f);
}

