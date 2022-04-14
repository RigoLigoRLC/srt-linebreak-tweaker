
#pragma once

#include <QUndoCommand>
#include <QVector>
#include <rint.h>

struct Dialog;

namespace LRCmd
{
  using DialogVec = QVector<Dialog>;

  class CmdBase : public QUndoCommand
  {
    public:
      CmdBase(DialogVec &model);
    protected:
      DialogVec &mModel;
      i32 mDialog, mWord;
  };

  class MergeToPrevLine : public CmdBase
  {
    public:
      MergeToPrevLine(DialogVec &model, i32 iDialog, i32 iWord, i32 iPrevDialog);
      void undo() override;
      void redo() override;
    private:
      i32 mPrevDialog;
      QChar mDelimMovedTail;
      u64 mPrevDuration, mCurrBegin, mCurrDuration;
      bool mCurrDestroyed;
  };

  class MergeToNextLine : public CmdBase
  {
    public:
      MergeToNextLine(DialogVec &model, i32 iDialog, i32 iWord, i32 iNextDialog);
      void undo() override;
      void redo() override;
    private:
      i32 mNextDialog, mMoveCount;
      QChar mDelimMovedTail;
      u64 mNextDuration, mNextBegin, mCurrDuration;
      bool mCurrDestroyed;
  };

  class SplitToNextLine : public CmdBase
  {
    public:
      SplitToNextLine(DialogVec &model, i32 iDialog, i32 iWord);
      void undo() override;
      void redo() override;
    private:
      i32 mNextDialog;
      QChar mDelimMovedTail;
      u64 mCurrDuration;
  };
}
