#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <math.h>

#include <QDebug>

#include <util.h>
#include "commands.h"
#include "reorganizer.h"

Reorganizer::Reorganizer(QWidget *parent) :
  QWidget(parent),
  mDispFont("sansserif", 10),
  mDispFontMet(mDispFont),
  mMouseDownPos()
{
  mCurrentLine = mCurrentOperatingLine = -1;
  mDoUpdateScrollBarOnChange = true;
  mBarHoriz = mBarVert = nullptr;
  mIsOnDirtyAction = false;
  mInternScrollOffset = 0;
  mCurrentLine = 0;
  mDesiredOperation = None;
}

void Reorganizer::SetScrollBars(QScrollBar *horiz, QScrollBar *vert)
{
  mBarHoriz = horiz;
  mBarVert = vert;
  connect(vert, &QScrollBar::sliderMoved,
          this, &Reorganizer::ScrolledToEntry);
}

void Reorganizer::OpenFile(QString name)
{
  QFile f(name);
  f.open(QFile::ReadOnly);
  if(f.error())
  {
    QMessageBox::critical(this, tr("Cannot open SRT"), tr("Error: %1").arg(f.errorString()));
    return;
  }
  // Clear everything
  mInternScrollOffset = 0;
  mCurrentLine = 0;
  mCurrentLine = mCurrentOperatingLine = -1;
  mModel.clear();
  mUndo.clear();
  // Disable updates
  mDoUpdateScrollBarOnChange = false;
  // Crappy SRT read routine
  QTextStream ts(&f);
  QString line, currtext;
  bool lastLineEmpty = false;
  int h1 = 0, m1 = 0, s1 = 0, ms1 = 0, h2 = 0, m2 = 0, s2 = 0, ms2 = 0;
  while(!ts.atEnd())
  { // Assume input is completely sane
    line = ts.readLine(); // Sequence number
    line = ts.readLine(); // Timecode
    sscanf(line.toStdString().c_str(),
           "%d:%d:%d,%d --> %d:%d:%d,%d",
           &h1, &m1, &s1, &ms1, &h2, &m2, &s2, &ms2);
    currtext = ts.readLine();
    while(!ts.atEnd())
    {
      if((line = ts.readLine()) == "")
        break;
      else
        currtext += '\n' + line;
    };

    // Add it into internal data model
    AppendToModel(TCtoMS(h1, m1, s1, ms1),
                  TCtoMS(h2, m2, s2, ms2),
                  currtext);

    // Cleanup
    lastLineEmpty = false;
    currtext.clear();
    h1 = 0, m1 = 0, s1 = 0, ms1 = 0, h2 = 0, m2 = 0, s2 = 0, ms2 = 0;
  }
  mDoUpdateScrollBarOnChange = true;
  UpdateExternals(true);
  mCurrentLine = mModel.size() - 1;
  f.close();
}

void Reorganizer::SaveFile(QString name)
{
  QFile f(name);
  f.open(QFile::WriteOnly);
  if(f.error())
  {
    QMessageBox::critical(this, tr("Cannot open SRT"), tr("Error: %1").arg(f.errorString()));
    return;
  }
  QTextStream ts(&f);
  i32 count = 1;
  for(auto &i : mModel)
  {
    ts << count << '\n';
    ts << MStoTC(i.begin) << " --> " << MStoSrtTC(i.end()) << '\n';
    for(auto &j : i.words)
    {
      ts << j.text;
      if(j.delim.cell())
        ts << j.delim << "\n\n";
    }
  }
}

void Reorganizer::paintEvent(QPaintEvent *e)
{
  // Current line is centered
  static constexpr QColor
    BgTile = QColor(255, 230, 200),
    BgEmpty = QColor(218, 192, 157),
    DivLine = QColor(0, 0, 0),
    FgText = QColor(0, 0, 0),
    FgGreyText = QColor(100, 100, 100);

  if(mModel.isEmpty())
    return;

  QPainter p(this);
  i32 h = height(),
      w = width(),
      h_2 = h / 2,
      lines_2 = ceil((float)h_2 / LineHeight),
      maxLines = mModel.size() - 1,
      fromLines = mCurrentLine - lines_2,
      toLines = I32Min2(lines_2 + mCurrentLine, maxLines),
      verticalOffset = h_2 - lines_2 * LineHeight - LineHeight / 2;

  QPen p1 { Qt::black },
       pt { FgText },
       pt2 { FgGreyText };
  QBrush b1 { BgTile, Qt::SolidPattern },
         b2 { BgEmpty, Qt::SolidPattern };

  p.setFont(mDispFont);

  qreal fromTop = verticalOffset, top;

  if(fromLines < 0)
  {
    fromTop += -fromLines * LineHeight;
    fromLines = 0;
  }
  top = fromTop;

  for(i32 i = fromLines; i <= toLines; i++)
  {
    auto &entry = mModel[i];
    if(entry.type == Dialog::Placeholder)
    {
      // Placeholder
      p.setPen(p1);
      p.setBrush(b2);
      p.drawRect(QRectF(0, top, TimeWidth, LineHeight));
      p.setPen(pt);
      p.drawText(QRectF(0, top, TimeWidth - HorizMargin, LineHeight),
                 Qt::AlignRight | Qt::AlignVCenter,
                 MStoTC(mModel[i].begin));
    }
    else
    {
      // Actual text
      p.setPen(p1);
      p.setBrush(b1);
      p.setPen(pt);
      p.drawRect(QRectF(0, top, TimeWidth, LineHeight));
      p.drawText(QRectF(0, top, TimeWidth_2 - HorizMargin, LineHeight),
                 Qt::AlignRight | Qt::AlignVCenter,
                 MStoTC(mModel[i].begin));
      p.drawText(QRectF(TimeWidth_2 + HorizMargin, top, TimeWidth_2 - HorizMargin, LineHeight),
                 Qt::AlignLeft | Qt::AlignVCenter,
                 MStoTC(mModel[i].end()));
      // All the text blocks
      f64 left = TimeWidth;
      for(auto &i : entry.words)
      {
        QRectF currWordRect = QRectF(left, top, i._cachedBlockWidthPx, LineHeight);
        p.drawRect(currWordRect);
        currWordRect.adjust(HorizMargin, 0, 0, 0);
        p.drawText(currWordRect,
                   Qt::AlignLeft | Qt::AlignVCenter,
                   i.text);

        left += i._cachedBlockWidthPx;
      }
    }
    top += LineHeight;
  }

  // Process the currently operating line
  if(mCurrentOperatingLine >= 0)
  {
    f64 opTop = fromTop + (mCurrentOperatingLine - fromLines) * LineHeight,
        opLeft = TimeWidth;
    QBrush bw { Qt::white, Qt::SolidPattern };
    p.setBrush(bw);
    p.setCompositionMode(QPainter::CompositionMode_Difference);
    QRectF fillArea;
    auto &opWords = mModel[mCurrentOperatingLine].words;
    if(mDesiredOperation == AtPlace) // Go nowhere
    {
      for(int i = 0; i < mCurrentOperatingWord; i++) opLeft += opWords[i]._cachedBlockWidthPx;
      p.drawRect(QRectF(opLeft, opTop, opWords[mCurrentOperatingWord]._cachedBlockWidthPx, LineHeight));
    }
    else if(mDesiredOperation == MergeNext || mDesiredOperation == SplitNext) // To right
    {
      for(int i = 0; i < mCurrentOperatingWord; i++) opLeft += opWords[i]._cachedBlockWidthPx;
      p.drawRect(QRectF(opLeft, opTop, w - opLeft, LineHeight));
    }
    else // To left
    {
      f64 opW = TimeWidth;
      for(int i = 0; i <= mCurrentOperatingWord; i++) opW += opWords[i]._cachedBlockWidthPx;
      p.drawRect(QRectF(0, opTop, opW, LineHeight));
    }
  }

  p.setPen(p1);
  p.setCompositionMode(QPainter::CompositionMode::CompositionMode_SourceOver);
  p.drawLine(QPointF(TimeWidth_2, 0),
             QPointF(TimeWidth_2, h));
  p.drawLine(QPointF(TimeWidth, 0),
             QPointF(TimeWidth, h));

  p.end();
}

void Reorganizer::mousePressEvent(QMouseEvent *e)
{
  mIsOnDirtyAction = true;
  // Determine where the mouse is at
  auto pos = e->pos();
  auto deltaY = pos.y() - height() / 2;
  mCurrentOperatingLine = mCurrentLine + (deltaY + Sign(deltaY) * LineHeight / 2) / LineHeight;
  if(mCurrentOperatingLine < 0 || mCurrentOperatingLine >= mModel.size())
    return;
  auto &opLine = mModel[mCurrentOperatingLine];
  auto &opWords = opLine.words;
  f64 endPos = TimeWidth;
  mCurrentOperatingWord = opWords.size() - 1;
  if(opLine.type == Dialog::Real)
  {
    for(int i = 0; i < opWords.size(); i++)
    {
      endPos += opWords[i]._cachedBlockWidthPx;
      if(endPos > pos.x())
      {
        mCurrentOperatingWord = i;
        update();
        break;
      }
    }
  }
  mMouseDownPos = pos;
  mDesiredOperation = AtPlace;
}

void Reorganizer::mouseMoveEvent(QMouseEvent *e)
{
  constexpr f64 XtoYCoeff = 0.5, // Usage: if coeff * DeltaX > DeltaY then considers going sideways
                ActivateThreshold = 15; // Only moves more than this pixels in one direction triggers
  auto pos = e->pos();
  if(!mIsOnDirtyAction)
    return; // Nothing needed to calculate
  f64 dx = pos.x() - mMouseDownPos.x(),
      dy = pos.y() - mMouseDownPos.y();
  if(fabs(dx) * XtoYCoeff > fabs(dy))
  { // Horizontal move
    if(fabs(dx) < ActivateThreshold)
      mDesiredOperation = AtPlace;
    else
      if(dx > 0) mDesiredOperation = MergeNext;
      else mDesiredOperation = MergePrev;
  }
  else
  { // Vertical move
    if(fabs(dy) < ActivateThreshold)
      mDesiredOperation = AtPlace;
    else
      if(dy > 0) mDesiredOperation = SplitNext;
      else mDesiredOperation = SplitPrev;
  }
  update();
}

void Reorganizer::mouseReleaseEvent(QMouseEvent *e)
{
  if(mDesiredOperation)
  {
    CommitCurrentOperation();
  }
  if(mCurrentOperatingLine >= 0)
  {
    mCurrentOperatingLine = -1;
    update();
  }
  mIsOnDirtyAction = false;
}

void Reorganizer::keyPressEvent(QKeyEvent *e)
{

}

void Reorganizer::wheelEvent(QWheelEvent *e)
{
  i32 px = e->angleDelta().y() * ScrollCoeff;
  ScrollPixelDelta(px);
}

void Reorganizer::Redo()
{
  mUndo.redo();
  update();
}

void Reorganizer::Undo()
{
  mUndo.undo();
  update();
}

void Reorganizer::ScrolledToEntry(int x)
{
  mCurrentLine = x;
  mInternScrollOffset = 0;
  update();
}

void Reorganizer::ScrollPixelDelta(int x)
{
  auto px = mInternScrollOffset + x;
  if(px / LineHeight != 0)
  {
    auto nextline = BoundTo(mCurrentLine + px / LineHeight, 0, mModel.size());
    mBarVert->setValue(nextline);
    mCurrentLine = nextline;
    update();
  }
  mInternScrollOffset = px % LineHeight;
}

Status Reorganizer::AppendToModel(u64 begin, u64 end, QString dialog)
{
  if(mModel.size())
  {
    auto lastEnd = mModel.back().end();
    if(begin < lastEnd)
      return FailOccupied;
//    if(lastEnd < begin)
//      mModel.append(Dialog { .type = Dialog::Placeholder,
//                             .begin = lastEnd,
//                             .duration = begin - lastEnd,
//                             .words = SplitDialogByDelim(dialog) });
  }
  mModel.append(Dialog { .type = Dialog::Real,
                         .begin = begin,
                         .duration = end - begin,
                         .words = SplitDialogByDelim(dialog) });
  UpdateExternals();
  return Success;
}

Status Reorganizer::AddToModel(u64 begin, u64 end, QString dialog)
{
  return Success;
}

Status Reorganizer::CommitCurrentOperation()
{
  if(mCurrentOperatingLine < 0 || mDesiredOperation == AtPlace)
    return FailNoAction;
  switch(mDesiredOperation)
  {
    case MergePrev:
      if(mCurrentOperatingLine == 0) return FailNoTarget; // Can't do it
      if(mModel[mCurrentOperatingLine - 1].type == Dialog::Placeholder)
      {
        if(mCurrentOperatingLine == 1) // Prev dialog is empty and no further scrolling is possible
          return FailNoTarget;
        else
        {
          mUndo.push(new LRCmd::MergeToPrevLine(mModel,
                                                mCurrentOperatingLine,
                                                mCurrentOperatingWord,
                                                mCurrentOperatingLine - 2));
          mCurrentOperatingLine--;
          return Success;
        }
      }
      mUndo.push(new LRCmd::MergeToPrevLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentOperatingWord,
                                            mCurrentOperatingLine - 1));
      mCurrentOperatingLine--;
      return Success;

      break;

    case MergeNext:
      if(mCurrentOperatingLine == mModel.size() - 1) return FailNoTarget; // Can't do
      if(mModel[mCurrentOperatingLine + 1].type == Dialog::Placeholder)
      {
        if(mCurrentOperatingLine == mModel.size() - 2) // No further scrolling
          return FailNoTarget;
        else
        {
          mUndo.push(new LRCmd::MergeToNextLine(mModel,
                                                mCurrentOperatingLine,
                                                mCurrentOperatingWord,
                                                mCurrentOperatingLine + 2));
          return Success;
        }
      }
      mUndo.push(new LRCmd::MergeToNextLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentOperatingWord,
                                            mCurrentOperatingLine + 1));
      return Success;

      break;

    case SplitPrev:

      break;

    case SplitNext:

      break;

    case None:
    case AtPlace: return FailNoTarget;
  }

  return FailNoAction;
}

void Reorganizer::UpdateExternals(bool force)
{
  if(mDoUpdateScrollBarOnChange || force)
  {
    mBarVert->setRange(0, mModel.size() - 1);
  }
}

QVector<DiscreteWord> Reorganizer::SplitDialogByDelim(QString dialog, QString delims)
{
  QVector<DiscreteWord> ret;

  auto last = 0;
  int i;

  for(i = 0; i < dialog.size(); i++)
  {
    for(auto j : delims)
    {
      if(dialog[i] == j)
      {
        auto newstr = dialog.mid(last, i - last);
        ret.append(DiscreteWord {
                     .text = newstr,
                     .delim = dialog[i],
                     ._cachedBlockWidthPx = mDispFontMet.width(newstr) + 2 * HorizMargin
                   });
        last = i + 1;

        break;
      }
    }
  }
  // If the string doesn't end with a delim, the last word is not put into ret.
  // Detect this and add it in right here
  if(i != dialog.size() - 1)
  {
    auto newstr = dialog.mid(last);
    ret.append(DiscreteWord {
                 .text = newstr,
                 .delim = '\0',
                 ._cachedBlockWidthPx = mDispFontMet.width(newstr) + 2 * HorizMargin
               });
  }
  return ret;
}


