
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

  prev.words.last().delim = mDelimMovedTail;

  auto begin = prev.words.size() - mWord - 1;
  for(int i = 0; i <= mWord; i++)
  {
    curr.words.insert(i, prev.words[begin]);
    prev.words.removeAt(begin);
  }

  prev.words.last().delim = '\0';
  curr.UpdatedWidth();
  prev.UpdatedWidth();
  curr.UpdatedCompleteText();
  prev.UpdatedCompleteText();
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
    prev.UpdatedCompleteText();
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
    curr.UpdatedCompleteText();
    prev.UpdatedCompleteText();
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
  else
  {
    mModel[mDialog].words.last().delim = mDelimMovedTail;
  }
  auto &curr = mModel[mDialog], &next = mModel[mNextDialog];
  next.begin = mNextBegin;
  next.duration = mNextDuration;
  curr.duration = mCurrDuration;

  for(int i = 0; i < mMoveCount; i++)
  {
    curr.words.append(next.words[0]);
    next.words.removeAt(0);
  }

  curr.words.last().delim = '\0';
  curr.UpdatedWidth();
  next.UpdatedWidth();
  curr.UpdatedCompleteText();
  next.UpdatedCompleteText();
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
    next.UpdatedCompleteText();
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
    curr.UpdatedCompleteText();
    next.UpdatedCompleteText();
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
  curr.duration += next.duration;
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
  mModel.removeAt(mDialog + 1);
}

void LRCmd::SplitToNextLine::redo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  i32 currSize = currwords.size();

  u64 timeDelta = ((f64)currSize - mWord) / currSize * curr.duration;
  curr.duration -= timeDelta;

  Dialog newdialog {
    .begin = curr.end(),
    .duration = timeDelta
  };

  auto &newwords = newdialog.words;

  for(int i = mWord; i < currSize; i++)
  {
    newwords.append(currwords[mWord]);
    currwords.removeAt(mWord);
  }
  newdialog.UpdatedWidth();
  newdialog.UpdatedCompleteText();

  mModel.insert(mDialog + 1, newdialog);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

//
// SplitToPrevLine
//


LRCmd::SplitToPrevLine::SplitToPrevLine(DialogVec &model, i32 iDialog, i32 iWord) :
  CmdBase(model)
{
  setText("Split to previous line");
  mDialog = iDialog;
  mWord = iWord;
}

void LRCmd::SplitToPrevLine::undo()
{
  auto &prev = mModel[mDialog];
  auto &prevwords = prev.words;
  auto &curr = mModel[mDialog + 1];
  auto &currwords = curr.words;

  prevwords.last().delim = mDelimMovedTail;

  for(int i = 0; i < prevwords.size(); i++)
    currwords.insert(i, prevwords[i]);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
  curr.begin -= prev.duration;
  curr.duration += prev.duration;

  mModel.removeAt(mDialog);
}

void LRCmd::SplitToPrevLine::redo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  i32 currSize = currwords.size();

  u64 timeDelta = ((f64)mWord / currSize) * curr.duration;

  mDelimMovedTail = currwords[mWord].delim;

  Dialog newdialog {
    .begin = curr.begin,
    .duration = timeDelta
  };

  curr.duration -= timeDelta;
  curr.begin += timeDelta;

  auto &newwords = newdialog.words;
  for(int i = 0; i <= mWord; i++)
  {
    newwords.append(currwords[0]);
    currwords.remove(0);
  }
  newdialog.UpdatedWidth();
  newwords.last().delim = '\0';
  newdialog.UpdatedCompleteText();

  mModel.insert(mDialog, newdialog);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

//
// ChangeWord
//


LRCmd::ChangeWord::ChangeWord(DialogVec &model, i32 iDialog, i32 iWord, DiscreteWord &word) :
  CmdBase(model)
{
  mChangeWord = word;
  mDialog = iDialog;
  mWord = iWord;
}

void LRCmd::ChangeWord::undo()
{
  auto &curr = mModel[mDialog];
  curr.words[mWord] = mOrigWord;
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

void LRCmd::ChangeWord::redo()
{
  auto &curr = mModel[mDialog];
  mOrigWord = curr.words[mWord];
  mChangeWord.delim = mOrigWord.delim; // Preserve delimiter! This is important
  curr.words[mWord] = mChangeWord;
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

//
// InsertWords
//


LRCmd::InsertWords::InsertWords(DialogVec &model, i32 iDialog, i32 iWord, QVector<DiscreteWord> &insertion) :
  CmdBase(model)
{
  mInsertedWords = insertion;
  mDialog = iDialog;
  mWord = iWord;
}

void LRCmd::InsertWords::undo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  auto &ins = mInsertedWords;

  currwords.remove(mWord, ins.size());
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

void LRCmd::InsertWords::redo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  auto &ins = mInsertedWords;

  for(int i = 0; i < ins.size(); i++)
    currwords.insert(mWord + i, ins[i]);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

//
// RemoveWord
//


LRCmd::RemoveWord::RemoveWord(DialogVec &model, i32 iDialog, i32 iWord) :
  CmdBase(model)
{
  mDialog = iDialog;
  mWord = iWord;
}

void LRCmd::RemoveWord::undo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  currwords.insert(mWord, mRemovedWord);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}

void LRCmd::RemoveWord::redo()
{
  auto &curr = mModel[mDialog];
  auto &currwords = curr.words;
  mRemovedWord = currwords[mWord];
  currwords.removeAt(mWord);
  curr.UpdatedWidth();
  curr.UpdatedCompleteText();
}


