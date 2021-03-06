
#pragma once

#include "reorganizer.h"
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
  };

  class SplitToPrevLine : public CmdBase
  {
    public:
      SplitToPrevLine(DialogVec &model, i32 iDialog, i32 iWord);
      void undo() override;
      void redo() override;
    private:
      i32 mPrevDialog;
      QChar mDelimMovedTail;
  };

  class ChangeWord : public CmdBase
  {
    public:
      ChangeWord(DialogVec &model, i32 iDialog, i32 iWord, DiscreteWord &word);
      void undo() override;
      void redo() override;
    private:
      DiscreteWord mChangeWord, mOrigWord;
  };

  class InsertWords : public CmdBase
  {
    public:
      InsertWords(DialogVec &model, i32 iDialog, i32 iWord, QVector<DiscreteWord> &insertion);
      void undo() override;
      void redo() override;
    private:
      QVector<DiscreteWord> mInsertedWords;
  };

  class RemoveWord : public CmdBase
  {
    public:
      RemoveWord(DialogVec &model, i32 iDialog, i32 iWord);
      void undo() override;
      void redo() override;
    private:
      DiscreteWord mRemovedWord;
  };
}
