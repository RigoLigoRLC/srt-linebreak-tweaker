#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <QTextCodec>
#include <QApplication>
#include <QStyleHints>
#include <QAudioDeviceInfo>
#include <math.h>

#include <QDebug>

#include <signal.h>
#include <util.h>
#include "commands.h"
#include "reorganizer.h"
#include <QLineEdit>

Reorganizer::Reorganizer(QWidget *parent) :
  QWidget(parent),
  mDispFont("sansserif", 10),
  mDispFontMet(mDispFont),
  mMouseDownPos(),
  mWav(this),
  mAudioOut(QAudioDeviceInfo::defaultOutputDevice())
{
  mCurrentActiveLine = mCurrentLongestLine = mCurrentLine = mCurrentOperatingLine = -1;
  mLongestLineWidth = 0.0;
  mDoUpdateScrollBarOnChange = true;
  mBarHoriz = mBarVert = nullptr;
  mDirtyActionType = NoAction;
  mVertScrollOffset = mHorizScrollOffset = 0;
  mCurrentLine = 0;
  mDesiredDragOp = NoDrag;
  mMouseDownTime = QTime::currentTime();
  mExpectingDblClk = false;
  mWaveformPlaying = mNleDragging = false;

  mNleRangeMsBegin = mNleRangeMsEnd = mNleMaximumLengthMs = 0;
  mAudioPlayRegionA = mAudioPlayRegionB = -1;

  mEdit = new QLineEdit(this);
  mEdit->setFixedWidth(250);
  mEdit->setVisible(false); // Hidden by default
  connect(mEdit, &QLineEdit::returnPressed, this, &Reorganizer::EditBlockTextInSitu_Commit);
}

void Reorganizer::SetScrollBars(QScrollBar *horiz, QScrollBar *vert, QScrollBar *nleHoriz)
{
  mBarHoriz = horiz;
  mBarVert = vert;
  mBarNleHoriz = nleHoriz;
  connect(vert, &QScrollBar::sliderMoved,
          this, &Reorganizer::ScrolledToEntry);
  connect(horiz, &QScrollBar::sliderMoved,
          this, &Reorganizer::ScrollHorizontal);
  connect(nleHoriz, &QScrollBar::valueChanged,
          this, &Reorganizer::ScrollNleMovement);
}

void Reorganizer::SetTimecodeEditBtns(QPushButton *begin, QPushButton *end)
{
  mBtnBegin = begin;
  mBtnEnd = end;
  begin->setFixedWidth(EmptyLengthWidth + TimeWidth);
  end->setFixedWidth(DurationWidth + TimeWidth);
}

void Reorganizer::OpenFile(QString name)
{
  QFile f(name);
  f.open(QFile::ReadOnly);
  if(f.error())
  {
    emit SendNotify(tr("Cannot open SRT file. Error: %1").arg(f.errorString()), 2);
    return;
  }
  // Clear everything
  mVertScrollOffset = 0;
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
  ts.setCodec("UTF-8");
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
  mBarVert->setValue(mCurrentLine + 1);
  emit SendNotify(tr("Loaded %1 lines").arg(mModel.size()), 0);
  f.close();
}

void Reorganizer::SaveFile(QString name)
{
  QFile f(name);
  f.open(QFile::WriteOnly);
  if(f.error())
  {
    emit SendNotify(tr("Cannot save SRT file. Error: %1").arg(f.errorString()), 2);
    return;
  }
  QTextStream ts(&f);
  i32 count = 1;
  for(auto &i : mModel)
  {
    ts << count << '\n';
    ts << MStoSrtTC(i.begin) << " --> " << MStoSrtTC(i.end()) << '\n';
    ts << i.UpdatedCompleteText() << '\n';
    ts << '\n';
    count++;
  }
}

void Reorganizer::OpenWave(QString name)
{
  QFile f(name);
  f.open(QFile::ReadOnly);
  if(f.error())
  {
    emit SendNotify(tr("Cannot open WAV file. Error: %1").arg(f.errorString()), 2);
    return;
  }
  mWav.clear();
  if(mWav.cacheAll(&f))
  {
    emit SendNotify(tr("WAV file successfully loaded."), 0);
  }
  else
  {
    QMessageBox::critical(this, tr("Cannot open WAV"), tr("WAV file unrecognized"));
    return;
  }
  mNleMaximumLengthMs = mWav.GetLengthMs();
  mNleRangeMsEnd = std::min(10000, mNleMaximumLengthMs);
  mBarNleHoriz->setMaximum(mNleMaximumLengthMs);
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

  QPainter p(this);
  i32 h = height(),
      h_list = h - NleHeight,
      w = width(),
      h_2 = h_list / 2,
      lines_2 = ceil((float)h_2 / LineHeight),
      maxLines = mModel.size() - 1,
      fromLines = mCurrentLine - lines_2,
      toLines = I32Min2(lines_2 + mCurrentLine, maxLines),
      verticalOffset = h_2 - lines_2 * LineHeight - LineHeight / 2;
  u64 lastEnd = 0; // The ending time of last dialog, used to paint the empty time if needed

  //
  //  ====== List editor area ======
  //

  if(mUpdateArea | ListArea && mModel.size())
  {
    mUpdateArea &= ~ListArea;

    QPen p1 { Qt::black },
    pt { FgText },
    pt2 { FgGreyText };
    QBrush b1 { BgTile, Qt::SolidPattern },
    b2 { BgEmpty, Qt::SolidPattern },
    ba { FgText, Qt::Dense5Pattern };

    p.setFont(mDispFont);
    p.setClipRect(QRectF(0, 0, w, h_list)); // Clip at list area first

    qreal fromTop = verticalOffset, top;

    if(fromLines < 0)
    {
      fromTop += -fromLines * LineHeight;
      fromLines = 0;
    }
    top = fromTop;

    // The shadow of Current Active Line
    p.setBrush(ba);
    p.drawRect(QRectF(0, (mCurrentActiveLine - fromLines) * LineHeight +
                      (fromLines ? verticalOffset : top),
                      w, LineHeight));
    for(i32 i = fromLines; i <= toLines; i++)
    {
      auto &entry = mModel[i];
      {
        p.setBrush(b1); // Light brush
        // All the text blocks
        f64 left = ReservedSpace - mHorizScrollOffset;
        for(auto &i : entry.words)
        {
          // Only draw visible blocks
          if(left + i._cachedBlockWidthPx > ReservedSpace)
          {
            QRectF currWordRect = QRectF(left, top, i._cachedBlockWidthPx, LineHeight);
            p.drawRect(currWordRect);
            currWordRect.adjust(HorizMargin, 0, 0, 0);
            p.drawText(currWordRect,
                       Qt::AlignLeft | Qt::AlignVCenter,
                       i.text);
          }

          left += i._cachedBlockWidthPx;
        }

        // Reserved space, timestamp etc
        p.setPen(p1);
        p.setBrush(b2); // Dark color
        p.setPen(pt);
        p.drawRect(QRectF(EmptyLengthWidth, top, ReservedSpace - EmptyLengthWidth, LineHeight));

        if(entry.begin != lastEnd) // Has empty space between current dialog and last dialog
        {
          p.drawPolygon(QVector<QPointF>{{0, top - LineHeight / 2},
                                         {EmptyLengthWidth - 8, top - LineHeight / 2},
                                         {EmptyLengthWidth, top},
                                         {EmptyLengthWidth - 8, top + LineHeight / 2},
                                         {0, top + LineHeight / 2}});
          p.drawText(QRectF(0, top - LineHeight / 2, EmptyLengthWidth - 8, LineHeight),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::number((entry.begin - lastEnd) / 1000.0)); // Empty space
        }
        p.drawText(QRectF(EmptyLengthWidth, top, TimeWidth - HorizMargin, LineHeight),
                   Qt::AlignRight | Qt::AlignVCenter,
                   MStoTC(entry.begin)); // From
        p.drawText(QRectF(EmptyLengthWidth + TimeWidth + HorizMargin, top,
                          TimeWidth - HorizMargin, LineHeight),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   MStoTC(entry.end())); // To
        p.drawText(QRectF(ReservedSpace - DurationWidth, top, DurationWidth - HorizMargin, LineHeight),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(entry.duration / 1000.0)); // Duration
        lastEnd = entry.end();
      }
      top += LineHeight;
    }

    // Division lines
    p.drawLine(QPointF(TimeWidth + EmptyLengthWidth, 0),
               QPointF(TimeWidth + EmptyLengthWidth, h_list));
    p.drawLine(QPointF(2 * TimeWidth + EmptyLengthWidth, 0),
               QPointF(2 * TimeWidth + EmptyLengthWidth, h_list));

    // Process the currently operating line
    if(mCurrentOperatingLine >= 0)
    {
      f64 opTop = fromTop + (mCurrentOperatingLine - fromLines) * LineHeight,
          opLeft = ReservedSpace;
      QBrush bw { Qt::white, Qt::SolidPattern };
      p.setBrush(bw);
      p.setCompositionMode(QPainter::CompositionMode_Difference);
      QRectF fillArea;
      auto &opWords = mModel[mCurrentOperatingLine].words;
      if(mDesiredDragOp == AtPlace) // Go nowhere
      {
        p.setClipRect(QRectF(ReservedSpace, 0, w - ReservedSpace, h_list)); // Clip at visible list area
        for(int i = 0; i < mCurrentEditingWord; i++) opLeft += opWords[i]._cachedBlockWidthPx;
        opLeft -= mHorizScrollOffset;
        p.drawRect(QRectF(opLeft, opTop, opWords[mCurrentEditingWord]._cachedBlockWidthPx, LineHeight));
      }
      else if(mDesiredDragOp == MergeNext || mDesiredDragOp == SplitNext) // To right
      {
        p.setClipRect(QRectF(ReservedSpace, 0, w - ReservedSpace, h_list)); // Clip at visible list area
        for(int i = 0; i < mCurrentEditingWord; i++) opLeft += opWords[i]._cachedBlockWidthPx;
        opLeft -= mHorizScrollOffset;
        p.drawRect(QRectF(opLeft, opTop, w - opLeft, LineHeight));
      }
      else // To left
      {
        // Do not clip, let the inverse color fill through the left border
        f64 opW = ReservedSpace;
        for(int i = 0; i <= mCurrentEditingWord; i++) opW += opWords[i]._cachedBlockWidthPx;
        p.drawRect(QRectF(0, opTop, opW - mHorizScrollOffset, LineHeight));
      }
      // Draw activate threshold
      p.drawEllipse(mMouseDownPos, ActivateThreshold, ActivateThreshold);
    }

    p.setPen(p1);
    p.setCompositionMode(QPainter::CompositionMode::CompositionMode_SourceOver);
  }

  //
  // ====== NLE editor area ======
  //

  if(mUpdateArea | NleArea)
  {
    mUpdateArea &= ~NleArea;
    p.translate(0, h_list); // Use relative coordinate of NLE editor
    p.setClipping(false);

    f32 pxPerMs = f32(w) / (mNleRangeMsEnd - mNleRangeMsBegin);

    // NLE blockes
    p.setFont(QFont("sansserif", 15));
    if(mNleRangeMsBegin < mNleRangeMsEnd)
    {
      i32 beginDialog = FindDialogAt(mNleRangeMsBegin);
      if(beginDialog >= 0)
      {
        f32 dialogTopLeft = (i32(mModel[beginDialog].begin) - mNleRangeMsBegin) * pxPerMs;
        i32 i = beginDialog;
        p.setBrush(QBrush(BgTile));
        while(dialogTopLeft < w)
        {
          p.drawRect(QRectF(dialogTopLeft, 0,
                            mModel[i].duration * pxPerMs, BlockHeight));
          // Text bounding box
//          p.drawRect(QRectF(QPointF(std::max(dialogTopLeft, 0.0f) + NleBlockMargin,
//                                    NleBlockMargin),
//                            QPointF(std::min(dialogTopLeft + mModel[i].duration * pxPerMs - NleBlockMargin, f32(w) - NleBlockMargin),
//                                    BlockHeight - NleBlockMargin)));
          p.drawText(QRectF(QPointF(std::max(dialogTopLeft, 0.0f) + NleBlockMargin,
                                    NleBlockMargin),
                            QPointF(std::min(dialogTopLeft + mModel[i].duration * pxPerMs - NleBlockMargin, f32(w) - NleBlockMargin),
                                    BlockHeight - NleBlockMargin)),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                     mModel[i].completeText);
          // Debug text
//          p.drawText(QPointF(dialogTopLeft, 0), QString::number(i) + ", " + QString::number(dialogTopLeft) + ", ");
          i++;
          dialogTopLeft = (mModel[i].begin - mNleRangeMsBegin) * pxPerMs;
        }
      }
      p.drawText(QPointF(0, 0), QString::number(beginDialog));
    }

    // Paint waveform
    if(mNleRangeMsBegin < mNleRangeMsEnd)
    {
      f32 samplePerPx = (mNleRangeMsEnd - mNleRangeMsBegin) / 1000.0 * mWav.SampleRate() / w,
          sampleBegin = mNleRangeMsBegin / 1000.0 * mWav.SampleRate(),
          sampleEnd;
      // Make sampleBegin always a multiply of samplePerPx
      // Doesn't lose a lot of precision but brings huge stability to the waveforms
      sampleBegin = floor(sampleBegin / samplePerPx) * samplePerPx;
      sampleEnd = sampleBegin + samplePerPx;
      for(i32 i = 0; i < w; i++)
      {
        auto peaks = mWav.GetWaveformPeaksForRange(floor(sampleBegin), floor(sampleEnd));
        p.drawLine(QPointF(i, WaveformHeight / 2 * (1 - peaks.first)  + NleHeight - WaveformHeight),
                   QPointF(i, WaveformHeight / 2 * (1 - peaks.second) + NleHeight - WaveformHeight));

        sampleBegin = sampleEnd;
        sampleEnd += samplePerPx;
      }

      QLinearGradient lg(0, 0, 0, WaveformHeight);

      lg.setColorAt(0.0, QColor(255,255,255,170));
      lg.setColorAt(0.5, QColor(255,255,255,64));
      lg.setColorAt(1.0, QColor(255,255,255,170));

      p.setPen(Qt::NoPen);
      p.setBrush(QBrush(lg));
      p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
//      p.drawRect(QRectF(0, NleHeight - WaveformHeight, w, WaveformHeight));
      // I don't know why the fuck this doesn't work so I had to use transform method to move
      // the entire painting area.
      // Please, Qt, how the fuck does the gradients' coordinates work? Isn't it just supposed to
      // be gradient-ing in my painted area?
      p.translate(0,   NleHeight - WaveformHeight);
      p.drawRect(QRectF(0, 0, w, WaveformHeight));
      p.translate(0, -(NleHeight - WaveformHeight));
      p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // paint time scale
    if(mNleRangeMsBegin < mNleRangeMsEnd)
    {
      // Determine interval

      f32 range = mNleRangeMsEnd - mNleRangeMsBegin;
      // log1 = 0
      // log5 = 0.69897
      f32 rangeLog = log10(range), exponential;
      rangeLog = modf(rangeLog, &exponential);
      // Smaller than 5*10^n uses 1*10^(n-1) as interval
      // Greater than 5*10^n uses 5*10^(n-1) as interval
      f32 interval = pow(((rangeLog > 0.69897f) ? 5 : 1) * 10, exponential - 1);

      // Round the begin time to a multiply of the interval
      f32 begin = floor(mNleRangeMsBegin / interval) * interval,
          linePos = (begin - mNleRangeMsBegin) * pxPerMs;

      p.setPen(DivLine);
      p.setBrush(FgText);
      p.drawLine(QPointF(0, BlockHeight), QPointF(w, BlockHeight));
      p.drawLine(QPointF(0, BlockHeight + NleScaleHeight), QPointF(w, BlockHeight + NleScaleHeight));
      while(linePos < w)
      {
        p.drawLine(QPointF(linePos, BlockHeight),
                   QPointF(linePos, BlockHeight + NleScaleHeight));
        p.drawText(QRectF(QPointF(linePos + 2, BlockHeight),
                          QSizeF(interval * pxPerMs, NleScaleHeight)),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   MStoTC(begin));
        begin += interval;
        linePos = (begin - mNleRangeMsBegin) * pxPerMs;
      }
    }
  }

  p.end();
}

void Reorganizer::mousePressEvent(QMouseEvent *e)
{
  // Determine where the mouse is at
  auto pos = e->pos();
  auto h = height() - NleHeight,
       deltaY = pos.y() - h / 2,
       realX = pos.x() + mHorizScrollOffset;
  f64 endPos;

  if(e->pos().y() > h)
  {
    e->pos().ry() -= h;
    return NleMousePressEvent(e);
  }

  mCurrentActiveLine = mCurrentLine + (deltaY + Sign(deltaY) * LineHeight / 2) / LineHeight;
  if(mCurrentActiveLine < 0 || mCurrentActiveLine >= mModel.size()) // Line invalid
  {
    SetCurrentActiveLine(-1);
  }
  else
  {
    // Clicking on a valid line, update current active line
    SetCurrentActiveLine(mCurrentOperatingLine = mCurrentActiveLine);

    // Figure out the word currently under the mouse
    auto &opLine = mModel[mCurrentOperatingLine];
    auto &opWords = opLine.words;
    endPos = ReservedSpace;
    mCurrentEditingWord = opWords.size() - 1;
    if(opLine.type == Dialog::Real)
    {
      for(int i = 0; i < opWords.size(); i++)
      {
        endPos += opWords[i]._cachedBlockWidthPx;
        if(endPos > realX)
        {
          mCurrentEditingWord = i;
          UpdateListArea();
          break;
        }
      }
    }
  }
  if(pos.x() < ReservedSpace)
  {
    // TODO: Edit of timestamp
  }

  // Double click detection
  auto nowTime = QTime::currentTime();
  QPointF newPos = e->pos();
  if(mExpectingDblClk &&
     mMouseDownTime.msecsTo(nowTime) < QApplication::doubleClickInterval() &&
     QLineF(mMouseDownPos, newPos).length() < qApp->styleHints()->mouseDoubleClickDistance())
  {
    if(mCurrentOperatingLine >= 0)
    {
      auto &opWords = mModel[mCurrentOperatingLine].words;
      SetDirtyAction(DblClkEditBlock); // Identify as double click
      EditBlockTextInSitu_Start(
            QPointF(endPos - opWords[mCurrentEditingWord]._cachedBlockWidthPx,
                    h / 2 + (mCurrentActiveLine - mCurrentLine + 0.5) * LineHeight),
            opWords[mCurrentEditingWord].text);
      mExpectingDblClk = false;
    }
  }
  else
  {
    mExpectingDblClk = true;
    // Not double click, consider other situations
    if(mDirtyActionType == DblClkEditBlock) // Cancelling an double click edit
    {
      EditBlockTextInSitu_Abort();
    }
    else
    {
      SetDirtyAction(DragBlock);
      mDesiredDragOp = AtPlace;
    }
  }

  mMouseDownPos = pos; // Remember mouse position, gonna use in double click detection
  mMouseDownTime = nowTime;
}

void Reorganizer::mouseMoveEvent(QMouseEvent *e)
{
  auto pos = e->pos();

  if(e->pos().y() > height() - NleHeight)
    return NleMouseMoveEvent(e);

  switch(mDirtyActionType)
  {
    case DragBlock:
    {
      f64 dx = pos.x() - mMouseDownPos.x(),
          dy = pos.y() - mMouseDownPos.y();
      if(fabs(dx) * XtoYCoeff > fabs(dy))
      { // Horizontal move
        if(fabs(dx) < ActivateThreshold)
          mDesiredDragOp = AtPlace;
        else
          if(dx > 0) mDesiredDragOp = MergeNext;
          else mDesiredDragOp = MergePrev;
      }
      else
      { // Vertical move
        if(fabs(dy) < ActivateThreshold)
          mDesiredDragOp = AtPlace;
        else
          if(dy > 0) mDesiredDragOp = SplitNext;
          else mDesiredDragOp = SplitPrev;
      }
      UpdateListArea();
      break;
    }

    case DragNleBlock:
      break;

    case DragNleTiming:
      break;

    case DblClkEditBlock:
      break;

    default: break;
  }
}

void Reorganizer::mouseReleaseEvent(QMouseEvent *e)
{

  if(mDesiredDragOp)
  {
    CommitCurrentOperation();
    mDesiredDragOp = NoDrag;
  }
  if(mCurrentOperatingLine >= 0)
  {
    mCurrentOperatingLine = -1;
    UpdateListArea();
  }
  // Only clear status for drag operations
  if(mDirtyActionType < DblClkEditBlock)
    SetDirtyAction(NoAction);
}

void Reorganizer::keyPressEvent(QKeyEvent *e)
{
  // FIXME: Doesn't work!
  if(mDirtyActionType && e->key() == Qt::Key_Escape)
  {
    SetDirtyAction(NoAction);
    UpdateListArea();
  }
}

void Reorganizer::wheelEvent(QWheelEvent *e)
{
  if(e->position().y() > height() - NleHeight)
    return NleWheelEvent(e);
  i32 px = e->angleDelta().y() * ScrollCoeff;
  ScrollPixelDelta(px);
}

void Reorganizer::resizeEvent(QResizeEvent *e)
{
  auto maxHoriz = std::max(mLongestLineWidth + ReservedSpace - e->size().width(), 0.0);
  mBarHoriz->setMaximum(maxHoriz);
  if(maxHoriz == 0.0)
    mHorizScrollOffset = 0;
  if(mDirtyActionType == DblClkEditBlock)
    EditBlockTextInSitu_Placement(QPointF(mInSituEditorLeftMargin,
                                          mInSituEditorOffCenterMargin +
                                            (e->size().height() - NleHeight) / 2));
}

void Reorganizer::NleMousePressEvent(QMouseEvent *e)
{
  auto pos = e->pos();
  if(pos.y() < BlockHeight)
  {
    // Subtitle block
  }
  else if(pos.y() < BlockHeight + NleScaleHeight)
  {
    // Scale
  }
  else
  {
    // Waveform
    if(e->button() == Qt::LeftButton)
    {
      mAudioPlayRegionA = NleXtoMS(pos.x());
      mNleDragging = true;
    }
    else
    {
      // TODO: Menu
    }
  }
}

void Reorganizer::NleMouseMoveEvent(QMouseEvent *e)
{

}

void Reorganizer::NleMouseReleaseEvent(QMouseEvent *e)
{
  mNleDragging = false;
  switch(mNleCurrentOp)
  {
    case DragWaveform:
      if(mAudioPlayRegionA > mAudioPlayRegionB)
        std::swap(mAudioPlayRegionA, mAudioPlayRegionB);
      PlayAudioRegion(mAudioPlayRegionA, mAudioPlayRegionB);
      mWaveformPlaying = true;
      break;

    default:
      break;
  }
}

void Reorganizer::NleWheelEvent(QWheelEvent *e)
{
  auto delta = e->angleDelta().x() * -NleScrollCoeff; // ms
  NleShiftTimeMs(delta);

  UpdateNLEArea();
}

void Reorganizer::Redo()
{
  mUndo.redo();
  UpdateExternals();
  UpdateListArea();
}

void Reorganizer::Undo()
{
  mUndo.undo();
  UpdateExternals();
  UpdateListArea();
}

void Reorganizer::ScrolledToEntry(int x)
{
  mCurrentLine = x;
  mVertScrollOffset = 0;
  UpdateListArea();
}

void Reorganizer::ScrollHorizontal(int x)
{
  mHorizScrollOffset = x;
  UpdateListArea();
}

void Reorganizer::ScrollPixelDelta(int x)
{
  if(mDirtyActionType != NoAction)
    return;

  auto px = mVertScrollOffset + x;
  if(px / LineHeight != 0)
  {
    auto nextline = BoundTo(mCurrentLine + px / LineHeight, 0, mModel.size());
    mBarVert->setValue(nextline);
    mCurrentLine = nextline;
    UpdateListArea();
  }
  mVertScrollOffset = px % LineHeight;
}

void Reorganizer::ScrollNleMovement(int x)
{
  auto delta = x - mNleRangeMsBegin;
  NleShiftTimeMs(delta, false);

  UpdateNLEArea();
}

//
// == In situ Block Editing ==
//

void Reorganizer::EditBlockTextInSitu_Start(QPointF bottomLeft, QString text)
{
  EditBlockTextInSitu_Placement(bottomLeft);
  mEdit->setVisible(true);
  mEdit->setEnabled(true);
  mEdit->setText(text);
  mEdit->setSelection(0, text.size());
  mEdit->setCursorPosition(text.size());
}

void Reorganizer::EditBlockTextInSitu_Abort()
{
  mEdit->setVisible(false);
  mEdit->setDisabled(true);
  SetDirtyAction(NoAction);
  mInSituEditorLeftMargin = mInSituEditorOffCenterMargin = 0;
}

void Reorganizer::EditBlockTextInSitu_Commit()
{
  mEdit->setVisible(false);
  mEdit->setDisabled(true);
  auto delta = SplitDialogByDelim(mEdit->text());
  switch(delta.size())
  {
    case 0:
      mUndo.push(new LRCmd::RemoveWord(mModel, mCurrentActiveLine, mCurrentEditingWord));
      break;

    case 1:
      mUndo.push(new LRCmd::ChangeWord(mModel, mCurrentActiveLine, mCurrentEditingWord, delta[0]));
      break;

    default:
      mUndo.beginMacro(tr("Change word to multiple words"));
      mUndo.push(new LRCmd::RemoveWord(mModel, mCurrentActiveLine, mCurrentEditingWord));
      mUndo.push(new LRCmd::InsertWords(mModel, mCurrentActiveLine, mCurrentEditingWord, delta));
      mUndo.endMacro();
      break;
  }

  SetDirtyAction(NoAction);
  UpdateListArea();
  mInSituEditorLeftMargin = mInSituEditorOffCenterMargin = 0;
}

void Reorganizer::AudioPlaybackStopped()
{

}

//
// == Status Setters ==
//

void Reorganizer::SetDirtyAction(DirtyActionType t)
{
  mDirtyActionType = t;
  if(t != NoAction)
  {
    mBarHoriz->setDisabled(true);
    mBarVert->setDisabled(true);
    mBarNleHoriz->setDisabled(true);
  }
  else
  {
    mBarHoriz->setDisabled(false);
    mBarVert->setDisabled(false);
    mBarNleHoriz->setDisabled(false);
  }
}

void Reorganizer::SetCurrentActiveLine(int x)
{
  mCurrentActiveLine = x;
  if(x < 0) return;
  UpdateTimecodeButtons();
  // TODO: Zoom in on NLE
}

void Reorganizer::SanitizeActiveSelection()
{
  if(mCurrentActiveLine < 0)
    mCurrentActiveLine = 0;
  else if(mCurrentActiveLine >= mModel.size())
    mCurrentActiveLine = mModel.size() - 1;
}

void Reorganizer::NleShiftTimeMs(int ms, bool changeScrollBar)
{
  // Clamp
  if(ms < 0)
    ms = -std::min(-ms, mNleRangeMsBegin);
  else if(ms > 0)
    ms =  std::min( ms, mNleMaximumLengthMs - mNleRangeMsEnd);
  mNleRangeMsBegin += ms;
  mNleRangeMsEnd += ms;
  if(changeScrollBar)
    mBarNleHoriz->setValue(mBarNleHoriz->value() + ms);
}

i32 Reorganizer::NleXtoMS(i32 x)
{
  return mNleRangeMsBegin + (f32(x) / width()) * (mNleRangeMsEnd - mNleRangeMsBegin);
}

void Reorganizer::PlayAudioRegion(i32 beginMs, i32 endMs)
{
  i32 beginSample = beginMs / 1000.0 * mWav.SampleRate(),
      endSample   = endMs   / 1000.0 * mWav.SampleRate();

//  mAudioOut.start(QBuffer())
}

//
// == Model Editing ==
//

Status Reorganizer::AppendToModel(u64 begin, u64 end, QString dialog)
{
  if(mModel.size())
  {
    auto lastEnd = mModel.back().end();
    if(begin < lastEnd)
      return FailOccupied;
  }
  mModel.append(Dialog { .type = Dialog::Real,
                         .begin = begin,
                         .duration = end - begin,
                         .words = SplitDialogByDelim(dialog),
                         .completeText = dialog });
  mModel.back().UpdatedWidth();
  UpdateExternals(false);
  return Success;
}

Status Reorganizer::AddToModel(u64 begin, u64 end, QString dialog)
{
  return Success;
}

i32 Reorganizer::FindDialogAt(u64 at)
{
  if(mModel.isEmpty())
    return -1;
  // Find closest begin time
  i32 A = 0, B = mModel.size() - 1;
  while(A < B - 1)
  {
    i32 mid = (B - A) / 2 + A;
    if(mModel[mid].begin > at)
      B = mid;
    else if(mModel[mid].begin < at)
      A = mid;
    else
      return mid;
  }
  // When this loop is over, we'd have `at` between A.begin and B.begin (except when you only have 2 elements)
  // Check if A's length covers `at`
  if(mModel[A].end() > at)
    return A;
  // If A is not covering `at`, consider the edge case of 2 elements
  else if (mModel[B].end() > at)
    return B;
  else
    return -1;
}

Status Reorganizer::CommitCurrentOperation()
{
  if(mCurrentOperatingLine < 0 || mDesiredDragOp == AtPlace)
    return FailNoAction;
  if(mCurrentOperatingLine > mModel.size())
    return FailInvalidOp;
  switch(mDesiredDragOp)
  {
    case MergePrev:
      if(mCurrentOperatingLine == 0)
      {
        emit SendNotify(tr("Can't merge to previous line, because this is already first line!"), 1);
        return FailNoTarget; // Can't do it
      }
      mUndo.push(new LRCmd::MergeToPrevLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentEditingWord,
                                            mCurrentOperatingLine - 1));
      mCurrentOperatingLine--;
      break;

    case MergeNext:
      if(mCurrentOperatingLine == mModel.size() - 1)
      {
        emit SendNotify(tr("Can't merge to next line, because this is already last line!"), 1);
        return FailNoTarget; // Can't do
      }
      mUndo.push(new LRCmd::MergeToNextLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentEditingWord,
                                            mCurrentOperatingLine + 1));
      break;

    case SplitPrev:
      if(mCurrentEditingWord == mModel.size() - 1)
      {
          emit SendNotify(tr("Can't split to previous line from the last word!"), 1);
          return FailNoTarget;
      }
      mUndo.push(new LRCmd::SplitToPrevLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentEditingWord));
      break;

    case SplitNext:
      if(mCurrentEditingWord == 0)
      {
        emit SendNotify(tr("Can't split to next line from the first word!"), 1);
        return FailNoTarget;
      }
      mUndo.push(new LRCmd::SplitToNextLine(mModel,
                                            mCurrentOperatingLine,
                                            mCurrentEditingWord));
      break;

    case NoDrag:
    case AtPlace: return FailNoTarget;
  }

  SanitizeActiveSelection();
  UpdateExternals();
  return Success;
}

void Reorganizer::UpdateTimecodeButtons()
{
  // Set timecode button text
  if(mCurrentActiveLine < 0) return;
  mBtnBegin->setText(MStoTC(mModel[mCurrentActiveLine].begin));
  mBtnEnd->setText(MStoTC(mModel[mCurrentActiveLine].end()));
}

void Reorganizer::UpdateListArea()
{
  mUpdateArea |= ListArea;
  update();
}

void Reorganizer::UpdateNLEArea()
{
  mUpdateArea |= NleArea;
  update();
}

void Reorganizer::UpdateAll()
{
  mUpdateArea = ListArea | NleArea;
  update();
}

void Reorganizer::UpdateExternals(bool force)
{
  if(mDoUpdateScrollBarOnChange || force)
  {
    mBarVert->setRange(0, mModel.size() - 1);

    // Little room for improvements, honestly, otherwise just do verification in paintEvent
    f64 maxWidth = 0.0;
    for(auto &i : mModel)
      if(i.width > maxWidth)
        maxWidth = i.width;

    mBarHoriz->setMaximum(std::max(maxWidth + ReservedSpace - width(), 0.0));
    mLongestLineWidth = maxWidth;
    UpdateTimecodeButtons();
  }
}

void Reorganizer::EditBlockTextInSitu_Placement(QPointF bottomLeft)
{
  if(mInSituEditorLeftMargin == 0.0)
  {
    // Only update these values when initiating editor, do not overwrite when resize event occurs
    mInSituEditorLeftMargin = bottomLeft.x();
    mInSituEditorOffCenterMargin = bottomLeft.y() - (height() - NleHeight) / 2;
  }

  // Sanitize display position
    auto h_w = mEdit->height();
    // Y
    if(bottomLeft.y() + h_w > height() - NleHeight)
      bottomLeft.ry() -= LineHeight + h_w;
    else if(bottomLeft.y() < 0)
      bottomLeft.setY(0);
    // X
    if(bottomLeft.x() < ReservedSpace)
      bottomLeft.setX(ReservedSpace);
    else if(bottomLeft.x() + LineEditorWidth > width())
      bottomLeft.setX(width() - LineEditorWidth);

  mEdit->setGeometry(bottomLeft.x(), bottomLeft.y(), LineEditorWidth, h_w);
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
  if(dialog.size() && i != dialog.size() - 1)
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


