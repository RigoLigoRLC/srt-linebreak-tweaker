
#include <commands.h>
#include <reorganizer.h>
#include <QDebug>

LRCmd::CmdBase::CmdBase(DialogVec &model) : mModel(model) { }

//
// MergeToPrevLine
//


LRCmd::MergeToPrevLine::MergeToPrevLine(DialogVec &model, i32 iDialog, i32 iWord, i32 iPrevDialog) :
  CmdBase(model)
{
  setText("Merge to previous line");
  mDialog = iDialog;
  mWord = iWord;
  mPrevDialog = iPrevDialog;
  mCurrDestroyed = false;
}

void LRCmd::MergeToPrevLine::undo()
{
  if(mCurrDestroyed)
  {
    mModel.insert(mDialog, Dialog {
                    .type = Dialog::Real
                  });
  }
  auto &curr = mModel[mDialog], &prev = mModel[mPrevDialog];
  prev.duration = mPrevDuration;
  curr.begin = mCurrBegin;
  curr.duration = mCurrDuration;

  auto begin = prev.words.size() - mWord - 1;
  for(int i = 0; i <= mWord; i++)
  {
    curr.words.insert(i, prev.words[begin]);
    prev.words.removeAt(begin);
  }

  curr.words.last().delim = mDelimMovedTail;
  prev.words.last().delim = '\0';
}

void LRCmd::MergeToPrevLine::redo()
{
  auto &curr = mModel[mDialog], &prev = mModel[mPrevDialog];
  auto currSize = curr.words.size();
  mPrevDuration = prev.duration;
  mCurrDuration = curr.duration;
  mCurrBegin = curr.begin;

  for(int i = 0; i <= mWord; i++)
  {
    prev.words.append(curr.words[0]);
    curr.words.removeAt(0);
  }

  mDelimMovedTail = prev.words.last().delim;
  prev.words.last().delim = '\0';

  if(curr.words.size() == 0)
  {
    prev.duration += curr.duration;
    mModel.removeAt(mDialog);
    mCurrDestroyed = true;
  }
  else
  {
    u64 timeDelta = (mWord + 1.0) / currSize * curr.duration;
    curr.begin += timeDelta;
    curr.duration -= timeDelta;
    prev.duration += timeDelta;
  }
}

//
// MergeToNextLine
//


LRCmd::MergeToNextLine::MergeToNextLine(DialogVec &model, i32 iDialog, i32 iWord, i32 iNextDialog) :
  CmdBase(model)
{
  setText("Merge to next line");
  mDialog = iDialog;
  mWord = iWord;
  mNextDialog = iNextDialog;
  mCurrDestroyed = false;
  mMoveCount = 0;
}

void LRCmd::MergeToNextLine::undo()
{
  if(mCurrDestroyed)
  {
    mModel.insert(mDialog, Dialog {
                    .type = Dialog::Real,
                    .begin = mModel[mDialog].begin
                  });
  }
  auto &curr = mModel[mDialog], &next = mModel[mNextDialog];
  next.begin = mNextBegin;
  next.duration = mNextDuration;
  curr.duration = mCurrDuration;

  curr.words.last().delim = mDelimMovedTail;

  for(int i = 0; i < mMoveCount; i++)
  {
    curr.words.append(next.words[0]);
    next.words.removeAt(0);
  }

  curr.words.last().delim = '\0';
}

void LRCmd::MergeToNextLine::redo()
{
  auto &curr = mModel[mDialog], &next = mModel[mNextDialog];
  auto currSize = curr.words.size();
  mCurrDuration = curr.duration;
  mNextDuration = next.duration;
  mNextBegin = next.begin;

  curr.words.last().delim = ' ';

  mMoveCount = curr.words.size() - mWord;
  for(int i = 0; i < mMoveCount; i++)
  {
    next.words.insert(i, curr.words[mWord]);
    curr.words.removeAt(mWord);
  }

  mDelimMovedTail = curr.words.last().delim;
  curr.words.last().delim = '\0';

  if(mWord == 0)
  {
    next.duration += curr.duration;
    next.begin -= curr.duration;
    mModel.removeAt(mDialog);
    mCurrDestroyed = true;
  }
  else
  {
    u64 timeDelta = ((f64)currSize - mWord) / currSize * curr.duration;
    curr.duration -= timeDelta;
    next.begin -= timeDelta;
    next.duration += timeDelta;
  }
}
