#include "statusnotify.h"
#include <QVariantAnimation>
#include <QAnimationGroup>

StatusNotify::StatusNotify(QWidget *parent) : QLabel(parent)
{
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
  setContentsMargins(5, 5, 5, 5);

  auto expand = new QVariantAnimation(this),
       shrink = new QVariantAnimation(this);
  expand->setStartValue(0);
  expand->setEndValue(ExpandedWidth);
  expand->setDuration(DurationMoving);
  expand->setEasingCurve(QEasingCurve::InExpo);
  shrink->setStartValue(ExpandedWidth);
  shrink->setEndValue(0);
  shrink->setDuration(DurationMoving);
  shrink->setEasingCurve(QEasingCurve::InExpo);

  auto slot = [&](QVariant v)
  {
    setFixedWidth(v.toInt());
  };
  connect(expand, &QVariantAnimation::valueChanged, slot);
  connect(shrink, &QVariantAnimation::valueChanged, slot);

  mAnim.addAnimation(expand);
  mAnim.addPause(DurationKeep);
  mAnim.addAnimation(shrink);
  connect(&mAnim, &QSequentialAnimationGroup::finished,
          [&](){ setVisible(false); });
}

void StatusNotify::Activate(QString str, int severity)
{
  constexpr static QColor colors[]
  {
    {154, 167, 214},
    {243, 191, 81},
    {158, 62, 48},
  };
  if(severity < 0) severity = 0;
  if(severity > 2) severity = 2;

  setStyleSheet("border-radius: 5px; background-color: " + colors[severity].name());
  setContentsMargins(5, 5, 5, 5);

  setText(str);
  setVisible(true);

  if(mAnim.state() == QAbstractAnimation::Running)
    mAnim.stop();
  mAnim.start();
}
