#ifndef STATUSNOTIFY_H
#define STATUSNOTIFY_H

#include <QLabel>
#include <QSequentialAnimationGroup>
#include <rint.h>

class StatusNotify : public QLabel
{
  public:
    StatusNotify(QWidget *parent = nullptr);

  public:
    // Constant
    constexpr static i32
      ExpandedWidth = 600;
    constexpr static i32
      DurationMoving = 300,
      DurationKeep = 4000;

  private:
    QSequentialAnimationGroup mAnim;

  public slots:
    void Activate(QString str, int severity);

};

#endif // STATUSNOTIFY_H
