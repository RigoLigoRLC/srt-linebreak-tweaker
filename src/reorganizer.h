#ifndef REORGANIZER_H
#define REORGANIZER_H

#include <QWidget>
#include <QMouseEvent>
#include <QVector>
#include <QScrollBar>
#include <QFont>
#include <QFontMetricsF>
#include <QUndoStack>
#include <rint.h>
#include <common.h>

struct Dialog;
struct DiscreteWord;

class Reorganizer : public QWidget
{
    Q_OBJECT
  public:
    explicit Reorganizer(QWidget *parent = nullptr);

    void SetScrollBars(QScrollBar* horiz, QScrollBar* vert);

    void OpenFile(QString name);
    void SaveFile(QString name);

  protected:
    virtual void paintEvent(QPaintEvent* e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void wheelEvent(QWheelEvent *e) override;

  public slots:
    void Redo();
    void Undo();
    void ScrolledToEntry(int);
    void ScrollPixelDelta(int);

  private: // Methods
    // Model interface
    Status AppendToModel(u64 begin, u64 end, QString dialog);
    Status AddToModel(u64 begin, u64 end, QString dialog);

    Status CommitCurrentOperation();

    void UpdateExternals(bool force = false);

  private: // Helper functions
    QVector<DiscreteWord> SplitDialogByDelim(QString dialog, QString delims = " \n\t");

  private: // Properties
    // Model
    QVector<Dialog> mModel;

    // Status
    bool mDoUpdateScrollBarOnChange,
         mIsOnDirtyAction;
    i32 mCurrentLine, mCurrentOperatingLine, mCurrentOperatingWord;
    i32 mInternScrollOffset;
    QPointF mMouseDownPos;
    enum { None = 0, AtPlace, MergeNext, MergePrev, SplitNext, SplitPrev } mDesiredOperation;

    QUndoStack mUndo;

    // Related components
    QScrollBar *mBarHoriz, *mBarVert;
    QFont mDispFont;
    QFontMetricsF mDispFontMet;

  private: // Constants
    static constexpr i32
      // Horizontal space
      LineHeight = 20,
      HorizMargin = 5,
      TimeWidth = 90,
      DurationWidth = 50,
      EmptyLengthWidth = 40,
      ReservedSpace = TimeWidth * 2 + DurationWidth + EmptyLengthWidth,

      // NLE space
      NleHeight = 200
    ;
    static constexpr f64 ScrollCoeff = -0.9;

  signals:


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
};

#endif // REORGANIZER_H
