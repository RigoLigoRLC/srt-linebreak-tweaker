
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
  curr.UpdatedWidth();
  prev.UpdatedWidth();
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
    prev.UpdatedWidth();
    mModel.removeAt(mDialog);
    mCurrDestroyed = true;
  }
  else
  {
    u64 timeDelta = (mWord + 1.0) / currSize * curr.duration;
    curr.begin += timeDelta;
    curr.duration -= timeDelta;
    prev.duration += timeDelta;
    curr.UpdatedWidth();
    prev.UpdatedWidth();
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

//  curr.words.last().delim = mDelimMovedTail;

  for(int i = 0; i < mMoveCount; i++)
  {
    curr.words.append(next.words[0]);
    next.words.removeAt(0);
  }

  curr.words.last().delim = '\0';
  curr.UpdatedWidth();
  next.UpdatedWidth();
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

  if(mWord == 0)
  {
    next.duration += curr.duration;
    next.begin -= curr.duration;
    mModel.removeAt(mDialog);
    mCurrDestroyed = true;
    next.UpdatedWidth();
  }
  else
  {
    mDelimMovedTail = curr.words.last().delim;
    curr.words.last().delim = '\0';

    u64 timeDelta = ((f64)currSize - mWord) / currSize * curr.duration;
    curr.duration -= timeDelta;
    next.begin -= timeDelta;
    next.duration += timeDelta;
    curr.UpdatedWidth();
    next.UpdatedWidth();
  }
}

//
// SplitToNextLine
//


LRCmd::SplitToNextLine::SplitToNextLine(DialogVec &model, i32 iDialog, i32 iWord) :
  CmdBase(model)
{
  setText("Split to new line after");
  mDialog = iDialog;
  mWord = iWord;
}

void LRCmd::SplitToNextLine::undo()
{
  auto &curr = mModel[mDialog], &next = mModel[mDialog + 1];
  auto &currwords = curr.words, &nextwords = next.words;

  currwords.append(nextwords);
  curr.duration = mCurrDuration;
  curr.UpdatedWidth();
  mModel.removeAt(mDialog + 1);
}

void LRCmd::SplitToNextLine::redo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  i32 currSize = currwords.size();
  mCurrDuration = curr.duration;

  u64 timeDelta = ((f64)currSize - mWord) / currSize * curr.duration;
  curr.duration -= timeDelta;

  Dialog newdialog {
    .begin = curr.end(),
    .duration = timeDelta
  };
  newdialog.UpdatedWidth();

  auto &newwords = newdialog.words;

  for(int i = mWord; i < currSize; i++)
  {
    newwords.append(currwords[mWord]);
    currwords.removeAt(mWord);
  }

  mModel.insert(mDialog + 1, newdialog);
  curr.UpdatedWidth();
}
