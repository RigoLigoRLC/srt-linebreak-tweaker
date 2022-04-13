
#include <util.h>
#include <QString>

u64 TCtoMS(int h, int m, int s, int ms)
{
  return 3600000 * h + 60000 * m + 1000 * s + ms;
}

QString MStoTC(u64 ms)
{
  return QString("%1:%2:%3,%4")
      .arg(ms / 3600000)
      .arg(ms % 3600000 / 60000, 2, 10, QChar('0'))
      .arg(ms % 60000 / 1000, 2, 10, QChar('0'))
      .arg(ms % 1000, 3, 10, QChar('0'));
}

QString MStoSrtTC(u64 ms)
{
  return QString("%1:%2:%3,%4")
      .arg(ms / 3600000, 2, 10, QChar('0'))
      .arg(ms % 3600000 / 60000, 2, 10, QChar('0'))
      .arg(ms % 60000 / 1000, 2, 10, QChar('0'))
      .arg(ms % 1000, 3, 10, QChar('0'));
}
