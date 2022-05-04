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
#include <rint.h>
#include <common.h>

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

  public slots:
    void Redo();
    void Undo();

  private slots:
    void ScrolledToEntry(int);
    void ScrollHorizontal(int);
    void ScrollPixelDelta(int);

    /// @param bottomLeft the bottom left point of the double clicked block
    void EditBlockTextInSitu_Start(QPointF bottomLeft, QString text);
    void EditBlockTextInSitu_Abort();
    void EditBlockTextInSitu_Commit();

  private: // Methods
    // Status setters (with extra event processing inside)
    enum DirtyActionType { NoAction = 0, DragBlock, DragNleBlock, DragNleTiming, DblClkEditBlock };
    void SetDirtyAction(DirtyActionType);
    void SetCurrentActiveLine(int);

    // Model interface
    Status AppendToModel(u64 begin, u64 end, QString dialog);
    Status AddToModel(u64 begin, u64 end, QString dialog);

    Status CommitCurrentOperation();

    /// @param affectLongest Whether the conducted operation could possibly shorten
    ///        the longest line marked in mCurrentLongestLine.
    void UpdateExternals(bool force = false);

    void EditBlockTextInSitu_Placement(QPointF bottomLeft);

  private: // Helper functions
    QVector<DiscreteWord> SplitDialogByDelim(QString dialog, QString delims = " \n\t");

  private: // Properties
    // Model
    QVector<Dialog> mModel;

    // Status
    bool mDoUpdateScrollBarOnChange, mExpectingDblClk;
    DirtyActionType mDirtyActionType;
    i32 mCurrentLine, mCurrentOperatingLine, mCurrentOperatingWord, mCurrentLongestLine,
        mCurrentActiveLine;
    f64 mLongestLineWidth;
    i32 mVertScrollOffset, mHorizScrollOffset;
    QPointF mMouseDownPos;
    QTime mMouseDownTime;
    enum { NoDrag = 0, AtPlace, MergeNext, MergePrev, SplitNext, SplitPrev } mDesiredDragOp;

    QUndoStack mUndo;

    // Related components
    QScrollBar *mBarHoriz, *mBarVert, *mBarNleHoriz;
    QFont mDispFont;
    QFontMetricsF mDispFontMet;

    QPushButton *mBtnBegin, *mBtnEnd;

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
      NleHeight = 200
    ;
    static constexpr f64
      ScrollCoeff = -0.9,
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
  u64 end() { return begin + duration; };

  f64 width;
  f64 UpdatedWidth()
  {
    f64 ret = 0.0;
    for(auto &i : words)
      ret += i._cachedBlockWidthPx;
    return (width = ret);
  }
};

#endif // REORGANIZER_H
