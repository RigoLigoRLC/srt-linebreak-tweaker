#ifndef REORGANIZER_H
#define REORGANIZER_H

#include <QWidget>
#include <QMouseEvent>
#include <QVector>
#include <QScrollBar>
#include <QLineEdit>
#include <QPushButton>
#include <QFont>
#include <QFontMetricsF>
#include <QUndoStack>
#include <QTime>
#include <QAudioOutput>
#include <rint.h>
#include <common.h>
#include <wavdecoder.h>

struct Dialog;
struct DiscreteWord;

class Reorganizer : public QWidget
{
    Q_OBJECT
  public:
    explicit Reorganizer(QWidget *parent = nullptr);

    void SetScrollBars(QScrollBar* horiz, QScrollBar* vert, QScrollBar *nleHoriz);
    void SetTimecodeEditBtns(QPushButton* begin, QPushButton* end);

    void OpenFile(QString name);
    void SaveFile(QString name);
    void OpenWave(QString name);

  protected:
    virtual void paintEvent(QPaintEvent* e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void wheelEvent(QWheelEvent *e) override;
    virtual void resizeEvent(QResizeEvent *e) override;

    void NleMousePressEvent(QMouseEvent *e);
    void NleMouseMoveEvent(QMouseEvent *e);
    void NleMouseReleaseEvent(QMouseEvent *e);
    void NleWheelEvent(QWheelEvent *e);

  public slots:
    void Redo();
    void Undo();

  private slots:
    void ScrolledToEntry(int);
    void ScrollHorizontal(int);
    void ScrollPixelDelta(int);
    void ScrollNleMovement(int);

    /// @param bottomLeft the bottom left point of the double clicked block
    void EditBlockTextInSitu_Start(QPointF bottomLeft, QString text);
    void EditBlockTextInSitu_Abort();
    void EditBlockTextInSitu_Commit();

    void AudioPlaybackStopped();

  private: // Methods
    // Status setters (with extra event processing inside)
    enum DirtyActionType { NoAction = 0, DragBlock, DragNleBlock, DragNleTiming, DblClkEditBlock };
    void SetDirtyAction(DirtyActionType);
    void SetCurrentActiveLine(int);

    void SanitizeActiveSelection();

    void NleShiftTimeMs(int, bool changeScrollBar = true);
    i32 NleXtoMS(i32);

    void PlayAudioRegion(i32 beginMs, i32 endMs);

    // Model interface
    Status AppendToModel(u64 begin, u64 end, QString dialog);
    Status AddToModel(u64 begin, u64 end, QString dialog);

    i32 FindDialogAt(u64 at); ///< Bisect model to find a dialog that goes through a time

    Status CommitCurrentOperation();

    void UpdateTimecodeButtons();
    void UpdateListArea();
    void UpdateNLEArea();
    void UpdateAll();

    void UpdateExternals(bool force = false);

    void EditBlockTextInSitu_Placement(QPointF bottomLeft);

  private: // Helper functions
    QVector<DiscreteWord> SplitDialogByDelim(QString dialog, QString delims = " \n\t");

  private: // Properties
    // Model
    QVector<Dialog> mModel;
    WavDecoder mWav;

    // Status
    bool mDoUpdateScrollBarOnChange, mExpectingDblClk, mWaveformPlaying, mNleDragging;
    DirtyActionType mDirtyActionType;
    i32 mCurrentLine, mCurrentOperatingLine, mCurrentEditingWord, mCurrentLongestLine,
        mCurrentActiveLine;
    f64 mLongestLineWidth;
    i32 mVertScrollOffset, mHorizScrollOffset;
    QPointF mMouseDownPos; ///< Only for list editor
    QTime mMouseDownTime; ///< Only for double click detection
    enum { NoDrag = 0, AtPlace, MergeNext, MergePrev, SplitNext, SplitPrev } mDesiredDragOp;

    // NLE Editor
    i32 mNleRangeMsBegin, mNleRangeMsEnd, mNleMaximumLengthMs;
    i32 mAudioPlayRegionA, mAudioPlayRegionB; ///< In milliseconds
    enum { NoNle = 0, DragWaveform, MoveDialog, DragDialogHead, DragDialogTail } mNleCurrentOp;

    // Repaint parameters
    enum UpdateAreaFlag { NoUpd = 0, ListArea = 1, NleArea = 2 };
    i32 mUpdateArea;

    // Undo stack
    QUndoStack mUndo;

    // Related components
    QScrollBar *mBarHoriz, *mBarVert, *mBarNleHoriz;
    QFont mDispFont;
    QFontMetricsF mDispFontMet;

    QPushButton *mBtnBegin, *mBtnEnd;

    // Player
    QAudioOutput mAudioOut;

    // In-situ Editor related
    QLineEdit *mEdit;
    f64 mInSituEditorLeftMargin, mInSituEditorOffCenterMargin;

  private: // Constants
    static constexpr i32
      // Horizontal space
      LineHeight = 20,
      HorizMargin = 5,
      TimeWidth = 90,
      DurationWidth = 50,
      EmptyLengthWidth = 40,
      ReservedSpace = TimeWidth * 2 + DurationWidth + EmptyLengthWidth,

      LineEditorWidth = 250, // Fixed wdith for block editor QLineEdit *mEdit

      // NLE space
      WaveformHeight = 110,
      BlockHeight = 60,
      NleScaleHeight = 30,
      NleHeight = WaveformHeight + BlockHeight + NleScaleHeight,
      NleBlockMargin = 5
    ;
    static constexpr f64
      ScrollCoeff = -0.9,
      NleScrollCoeff = 1, ///< NLE Scroll: Unit to Millisecond
      XtoYCoeff = 0.5, // Usage: if coeff * DeltaX > DeltaY then considers going sideways (merge)
      ActivateThreshold = 15; // Only moves more than this pixels in one direction triggers a merge/split

  public:
    static constexpr i32
      ListReservedSpace = ReservedSpace;

  signals:
    void SendNotify(QString msg, int severity);

};

struct DiscreteWord
{
  QString text;
  QChar delim;
  f64 _cachedBlockWidthPx;
};

struct Dialog
{
  enum { Real, Placeholder } type;
  u64 begin, duration;
  QVector<DiscreteWord> words;
  QString completeText;
  u64 end() { return begin + duration; };

  f64 width;
  f64 UpdatedWidth()
  {
    f64 ret = 0.0;
    for(auto &i : words)
      ret += i._cachedBlockWidthPx;
    return (width = ret);
  }

  QString& UpdatedCompleteText()
  {
    completeText.clear();
    for(auto &j : words)
    {
      completeText += j.text;
      if(j.delim.cell())
        completeText += j.delim;
    }
    return completeText;
  }
};
class AudioDataFeeder : public QObject
{
    Q_OBJECT
  public:
    AudioDataFeeder(QObject* parent, WavDecoder* wav);
    void SetInterval(i32 ms);
    void Start(QIODevice*);

  public slots:

};

#endif // REORGANIZER_H
