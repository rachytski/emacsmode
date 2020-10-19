/**************************************************************************
**
** Copyright (c) 2014 Siarhei Rachytski (siarhei.rachytski@gmail.com)
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/

#include "emacsmodehandler.hpp"
#include <functional>

//
// ATTENTION:
//
// 1 Please do not add any direct dependencies to other Qt Creator code here.
//   Instead emit signals and let the EmacsModePlugin channel the information to
//   Qt Creator. The idea is to keep this file here in a "clean" state that
//   allows easy reuse with any QTextEdit or QPlainTextEdit derived class.
//
// 3 Some conventions:
//
//   Use 1 based line numbers and 0 based column numbers. Even though
//   the 1 based line are not nice it matches vim's and QTextEdit's 'line'
//   concepts.
//
//   Do not pass QTextCursor etc around unless really needed. Convert
//   early to  line/column.
//
//   There is always a "current" cursor (tc_). A current "region of interest"
//   spans between anchor_ (== anchor()) and  tc_.position() (== position())
//   The value of tc_.anchor() is not used.
//

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtCore/QtAlgorithms>
#include <QtCore/QStack>

#include <QApplication>
#include <QtGui/QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocumentFragment>
#include <QTextEdit>

#include <climits>
#include <ctype.h>

#include "pluginstate.hpp"
#include "action.hpp"
#include "range.hpp"

using namespace Utils;

namespace EmacsMode {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// EmacsModeHandler
//
///////////////////////////////////////////////////////////////////////

#define EDITOR(s) (textedit_ ? textedit_->s : plaintextedit_->s)

const int ParagraphSeparator = 0x00002029;

using namespace Qt;

PluginState EmacsModeHandler::pluginState;

///////////////////////////////////////////////////////////////////////
//
// EmacsModeHandler
//
///////////////////////////////////////////////////////////////////////

EmacsModeHandler::EmacsModeHandler(QWidget *widget, QObject *parent)
  : QObject(parent)
{
  textedit_ = qobject_cast<QTextEdit *>(widget);
  plaintextedit_ = qobject_cast<QPlainTextEdit *>(widget);
  init();
  
  if (editor()) {
    connect(EDITOR(document()), SIGNAL(contentsChange(int,int,int)),
            SLOT(onContentsChanged(int,int,int)));
    connect(EDITOR(document()), SIGNAL(undoCommandAdded()), SLOT(onUndoCommandAdded()));
  }
}

bool EmacsModeHandler::eventFilter(QObject *ob, QEvent *ev)
{
  bool active = theEmacsModeSetting(ConfigUseEmacsMode)->value().toBool();

  if (active && ev->type() == QEvent::KeyPress && ob == editor()) {
    QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
    EventResult res = handleEvent(kev);
    return res == EventHandled;
  }

  if (active && ev->type() == QEvent::ShortcutOverride && ob == editor()) {
    QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
    if (wantsOverride(kev)) {
      ev->accept(); // accepting means "don't run the shortcuts"
      return true;
    }
    return true;
  }

  return QObject::eventFilter(ob, ev);
}

void EmacsModeHandler::startNewKillBufferEntryIfNecessary() {
  if ((lastActionId_ != Action::Id::KillLine) &&
      (lastActionId_ != Action::Id::KillSymbol) &&
      (lastActionId_ != Action::Id::KillSelected)) {
    pluginState.killRing_.push("");
  }
}

bool EmacsModeHandler::atEndOfLine() const {
  return tc_.atBlockEnd() && tc_.block().length() > 1;
}

QTextBlock EmacsModeHandler::block() const { 
  return tc_.block();
}

void EmacsModeHandler::setCurrentFileName(const QString &fileName)
{
  currentFileName_ = fileName;
}

QWidget *EmacsModeHandler::widget()
{
  return editor();
}

QWidget *EmacsModeHandler::editor() const
{
  return textedit_
      ? static_cast<QWidget *>(textedit_)
      : static_cast<QWidget *>(plaintextedit_);
}

void EmacsModeHandler::beginEditBlock() {
  tc_.beginEditBlock();
}

void EmacsModeHandler::beginEditBlock(int pos) {
  setUndoPosition(pos);
  beginEditBlock();
}

void EmacsModeHandler::endEditBlock() {
  tc_.endEditBlock();
}

void EmacsModeHandler::joinPreviousEditBlock() {
  tc_.joinPreviousEditBlock();
}

int EmacsModeHandler::anchor() const {
  return anchor_;
}

int EmacsModeHandler::position() const {
  return tc_.position();
}

QVariant EmacsModeHandler::config(int code) const {
  return theEmacsModeSetting(code)->value();
}

bool EmacsModeHandler::hasConfig(int code) const {
  return config(code).toBool();
}

bool EmacsModeHandler::hasConfig(int code, const char *value) const {
  return config(code).toString().contains(QString::fromLatin1(value));
}

QTextDocument *EmacsModeHandler::document() const { 
  return EDITOR(document()); 
}

void EmacsModeHandler::setupWidget()
{
  updateMiniBuffer();
}

void EmacsModeHandler::restoreWidget(int tabSize)
{
  EDITOR(setOverwriteMode(false));
}

void EmacsModeHandler::onContentsChanged(int position, int charsRemoved, int charsAdded)
{
  if (recordCursorPosition_)
  {
    setUndoPosition(position);
    recordCursorPosition_ = false;
  }
}

void EmacsModeHandler::onUndoCommandAdded()
{
  recordCursorPosition_ = true;
}

void EmacsModeHandler::init()
{
  cleanKillRing();

  shortcuts_.push_back(Shortcut("<META>|p", Action(Action::Id::MoveUp, std::bind(&EmacsModeHandler::moveUpAction, this, 1))));
  shortcuts_.push_back(Shortcut("<META>|n", Action(Action::Id::MoveDown, std::bind(&EmacsModeHandler::moveDownAction, this, 1))));
  shortcuts_.push_back(Shortcut("<META>|f", Action(Action::Id::MoveRight, std::bind(&EmacsModeHandler::moveRightAction, this, 1))));
  shortcuts_.push_back(Shortcut("<META>|b", Action(Action::Id::MoveLeft, std::bind(&EmacsModeHandler::moveLeftAction, this, 1))));
  shortcuts_.push_back(Shortcut("<META>|m", Action(Action::Id::NewLine, std::bind(&EmacsModeHandler::newLineAction, this))));
  shortcuts_.push_back(Shortcut("<META>|h", Action(Action::Id::Backspace, std::bind(&EmacsModeHandler::backspaceAction, this))));
  shortcuts_.push_back(Shortcut("<META>|e", Action(Action::Id::MoveToEndOfLine, std::bind(&EmacsModeHandler::moveToEndOfLineAction, this))));
  shortcuts_.push_back(Shortcut("<META>|a", Action(Action::Id::MoveToStartOfLine, std::bind(&EmacsModeHandler::moveToStartOfLineAction, this))));
  shortcuts_.push_back(Shortcut("<META>|<SHIFT>|<UNDERSCORE>", Action(Action::Id::Undo, std::bind(&EmacsModeHandler::undoAction, this))));
  shortcuts_.push_back(Shortcut("<TAB>", Action(Action::Id::IndentRegion, std::bind(&EmacsModeHandler::indentRegionAction, this))));
  shortcuts_.push_back(Shortcut("<META>|<SPACE>", Action(Action::Id::StartSelection, std::bind(&EmacsModeHandler::startSelectionAction, this))));
  shortcuts_.push_back(Shortcut("<ESC>|<ESC>", Action(Action::Id::CancelCurrentCommand, std::bind(&EmacsModeHandler::cancelCurrentCommandAction, this))));
  shortcuts_.push_back(Shortcut("<META>|w", Action(Action::Id::KillSelected, std::bind(&EmacsModeHandler::killSelectedAction, this))));
  shortcuts_.push_back(Shortcut("<ALT>|w", Action(Action::Id::CopySelected, std::bind(&EmacsModeHandler::copySelectedAction, this))));
  shortcuts_.push_back(Shortcut("<META>|<SLASH>", Action(Action::Id::InsertBackSlash, std::bind(&EmacsModeHandler::insertBackSlashAction, this))));
  shortcuts_.push_back(Shortcut("<CONTROL>|<SLASH>", Action(Action::Id::InsertStraightDelim, std::bind(&EmacsModeHandler::insertStraightDelimAction, this))));
  shortcuts_.push_back(Shortcut("<META>|k", Action(Action::Id::KillLine, std::bind(&EmacsModeHandler::killLineAction, this))));
  shortcuts_.push_back(Shortcut("<META>|d", Action(Action::Id::KillSymbol, std::bind(&EmacsModeHandler::killSymbolAction, this))));
  shortcuts_.push_back(Shortcut("<META>|y", Action(Action::Id::YankCurrent, std::bind(&EmacsModeHandler::yankCurrentAction, this))));
  shortcuts_.push_back(Shortcut("<ALT>|y", Action(Action::Id::YankNext, std::bind(&EmacsModeHandler::yankNextAction, this))));
  shortcuts_.push_back(Shortcut("<META>|x|s", Action(Action::Id::SaveCurrentBuffer, std::bind(&EmacsModeHandler::saveCurrentFileAction, this))));
  shortcuts_.push_back(Shortcut("<META>|i|c", Action(Action::Id::CommentOutRegion, std::bind(&EmacsModeHandler::commentOutRegionAction, this))));
  shortcuts_.push_back(Shortcut("<META>|i|u", Action(Action::Id::UncommentRegion, std::bind(&EmacsModeHandler::uncommentRegionAction, this))));
}

void EmacsModeHandler::saveCurrentFileAction()
{
  saveToFile(currentFileName_);
}

void EmacsModeHandler::moveToFirstNonBlankOnLine()
{
  moveToFirstNonBlankOnLine(&tc_);
}

void EmacsModeHandler::moveToFirstNonBlankOnLine(QTextCursor *tc)
{
  tc->setPosition(tc->block().position(), QTextCursor::KeepAnchor);
  moveToNonBlankOnLine(tc);
}

void EmacsModeHandler::moveToNonBlankOnLine(QTextCursor *tc)
{
  QTextDocument *doc = tc->document();
  const QTextBlock block = tc->block();
  const int maxPos = block.position() + block.length() - 1;
  int i = tc->position();
  while (doc->characterAt(i).isSpace() && i < maxPos)
    ++i;
  tc->setPosition(i, QTextCursor::KeepAnchor);
}

QChar const EmacsModeHandler::firstNonBlankOnLine(int line)
{
  QString const s = lineContents(line);
  int i = 0;
  while (i < s.size() && s.at(i).isSpace())
    ++i;
  if (i != s.size())
    return s.at(i);
  else
    return QChar();
}

void EmacsModeHandler::commentOutRegionAction()
{
  int beginLine = lineForPosition(tc_.anchor());
  int endLine = lineForPosition(tc_.position());

  if (tc_.atBlockStart() && tc_.atBlockEnd())
    if (beginLine > endLine)
      ++endLine;
    else
      --endLine;

  if (beginLine > endLine)
    qSwap(beginLine, endLine);

  /// find out whether we need to uncomment it
  int firstPos = firstPositionInLine(beginLine);

  beginEditBlock(firstPos);

  for (int line = beginLine; line <= endLine; ++line) {
    setPosition(firstPositionInLine(line));
    tc_.insertText(QString::fromLatin1("//"));
  }
  endEditBlock();

  setPosition(firstPos);
  //    moveToFirstNonBlankOnLine();
}

void EmacsModeHandler::uncommentRegionAction()
{
  int beginLine = lineForPosition(tc_.anchor());
  int endLine = lineForPosition(tc_.position());

  if (tc_.atBlockStart() && tc_.atBlockEnd())
    if (beginLine > endLine)
      ++endLine;
    else
      --endLine;

  if (beginLine > endLine)
    qSwap(beginLine, endLine);


  /// find out whether we need to uncomment it
  int firstPos = firstPositionInLine(beginLine);

  beginEditBlock(firstPos);

  for (int line = beginLine; line <= endLine; ++line) {
    setPosition(firstPositionInLine(line));
    tc_.deleteChar();
    tc_.deleteChar();
  }
  endEditBlock();

  setPosition(firstPos);
  //    moveToFirstNonBlankOnLine();
}

int EmacsModeHandler::lineNumber(const QTextBlock &block) const
{
  if (block.isVisible())
    return block.firstLineNumber() + 1;

  // Folded block has line number of the nearest previous visible line.
  QTextBlock block2 = block;
  while (block2.isValid() && !block2.isVisible())
    block2 = block2.previous();
  return block2.firstLineNumber() + 1;
}

QString EmacsModeHandler::selectText(const Range &range) const
{
  if (range.rangemode_ == RangeCharMode) {
    QTextCursor tc(document());
    tc.setPosition(range.beginPos_, QTextCursor::MoveAnchor);
    tc.setPosition(range.endPos_, QTextCursor::KeepAnchor);
    return tc.selection().toPlainText();
  }
  if (range.rangemode_ == RangeLineMode) {
    QTextCursor tc(document());
    int firstPos = firstPositionInLine(lineForPosition(range.beginPos_));
    int lastLine = lineForPosition(range.endPos_);
    int lastDocumentLine = document()->blockCount();
    bool endOfDoc = lastLine == lastDocumentLine;
    int lastPos = endOfDoc ? lastPositionInDocument() : firstPositionInLine(lastLine + 1);
    tc.setPosition(firstPos, QTextCursor::MoveAnchor);
    tc.setPosition(lastPos, QTextCursor::KeepAnchor);
    QString contents = tc.selection().toPlainText();
    return contents;// + QLatin1String(endOfDoc ? "\n" : "");
  }
  // FIXME: Performance?
  int beginLine = lineForPosition(range.beginPos_);
  int endLine = lineForPosition(range.endPos_);
  int beginColumn = 0;
  int endColumn = INT_MAX;
  if (range.rangemode_ == RangeBlockMode) {
    int column1 = range.beginPos_ - firstPositionInLine(beginLine);
    int column2 = range.endPos_ - firstPositionInLine(endLine);
    beginColumn = qMin(column1, column2);
    endColumn = qMax(column1, column2);
  }
  int len = endColumn - beginColumn + 1;
  QString contents;
  QTextBlock block = document()->findBlockByNumber(beginLine - 1);
  for (int i = beginLine; i <= endLine && block.isValid(); ++i) {
    QString line = block.text();
    if (range.rangemode_ == RangeBlockMode) {
      line = line.mid(beginColumn, len);
      if (line.size() < len)
        line += QString(len - line.size(), QLatin1Char(' '));
    }
    contents += line;
    if (!contents.endsWith(QLatin1Char('\n')))
      contents += QLatin1Char('\n');
    block = block.next();
  }
  //qDebug() << "SELECTED: " << contents;
  return contents;
}

void EmacsModeHandler::saveToFile(QString const & fileName)
{
  int beginLine = 0;
  int endLine = linesInDocument();
  QFile file1(fileName);
  bool exists = file1.exists();
  if (file1.open(QIODevice::ReadWrite))
  {
    file1.close();
    QTextCursor tc = tc_;
    Range range(firstPositionInLine(beginLine),
                firstPositionInLine(endLine), RangeLineMode);
    QString contents = selectText(range);
    tc_ = tc;
    bool handled = false;
    emit writeFileRequested(&handled, fileName, contents);
    // nobody cared, so act ourselves
    if (!handled) {
      QFile::remove(fileName);
      QFile file2(fileName);
      if (file2.open(QIODevice::ReadWrite)) {
        QTextStream ts(&file2);
        ts << contents;
        ts.flush();
      } else {
        qDebug() << QString::fromLatin1("Cannot open file '%1' for writing").arg(fileName);
        showMessage(MessageError, EmacsModeHandler::tr
                    ("Cannot open file '%1' for writing").arg(fileName));
      }
    }
    // check result by reading back
    QFile file3(fileName);
    file3.open(QIODevice::ReadOnly);
    QByteArray ba = file3.readAll();
    showMessage(MessageInfo, EmacsModeHandler::tr("\"%1\" %2 %3L, %4C written")
                .arg(fileName).arg(exists ? QString::fromLatin1(" ") : QString::fromLatin1(" [New] "))
                .arg(ba.count('\n')).arg(ba.size()));
  }
  else
  {
    showMessage(MessageError, EmacsModeHandler::tr
                ("Cannot open file '%1' for reading").arg(fileName));
  }
}

void EmacsModeHandler::copySelectedAction()
{
  pluginState.killRing_.push("");
  pluginState.killRing_.appendTop(tc_.selectedText());
  anchorCurrentPos();
  setMoveMode(QTextCursor::MoveAnchor);
}

void EmacsModeHandler::killSelectedAction()
{
  startNewKillBufferEntryIfNecessary();
  pluginState.killRing_.appendTop(tc_.selectedText());
  tc_.removeSelectedText();
  anchorCurrentPos();
  setMoveMode(QTextCursor::MoveAnchor);
}

void EmacsModeHandler::killSymbolAction()
{
  startNewKillBufferEntryIfNecessary();

  tc_.setPosition(tc_.position(), QTextCursor::MoveAnchor);
  tc_.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  pluginState.killRing_.appendTop(tc_.selectedText());

  tc_.removeSelectedText();
}

void EmacsModeHandler::cleanKillRing()
{
  pluginState.killRing_.clear();
}

void EmacsModeHandler::yankCurrentAction()
{
  if (!pluginState.killRing_.empty()) {
    startYankPosition_ = tc_.position();
    tc_.insertText(pluginState.killRing_.current());
    endYankPosition_ = tc_.position();
  }
}

void EmacsModeHandler::yankNextAction() {
  if ((lastActionId_ == Action::Id::YankCurrent) || ((lastActionId_ == Action::Id::YankNext) && isValidYankChain_)) {
    tc_.setPosition(startYankPosition_, QTextCursor::MoveAnchor);
    tc_.setPosition(endYankPosition_, QTextCursor::KeepAnchor);
    tc_.removeSelectedText();
    pluginState.killRing_.advance();
    isValidYankChain_ = true;
    yankCurrentAction();
  } else {
    showMessage(MessageError, EmacsModeHandler::tr("Previous command wasn't yank!"));
  }
}

void EmacsModeHandler::killLineAction()
{
  startNewKillBufferEntryIfNecessary();
  bool isEndOfLine = (atEndOfLine() || (tc_.block().length() == 1));

  if (!isEndOfLine)
  {
    tc_.setPosition(tc_.position(), QTextCursor::MoveAnchor);
    tc_.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    pluginState.killRing_.appendTop(tc_.selectedText());
  }
  else
  {
    tc_.setPosition(tc_.position(), QTextCursor::MoveAnchor);
    tc_.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    pluginState.killRing_.appendTop(tc_.selectedText());
  }
  tc_.removeSelectedText();
}

void EmacsModeHandler::setMoveMode(QTextCursor::MoveMode moveMode)
{
  moveMode_ = moveMode;
}

void EmacsModeHandler::startSelectionAction() {
  anchorCurrentPos();
  setMoveMode(QTextCursor::KeepAnchor);
}

void EmacsModeHandler::cancelCurrentCommandAction() {
  setMoveMode(QTextCursor::MoveAnchor);
  anchorCurrentPos();
  lastActionId_ = Action::Id::Null;
}

void EmacsModeHandler::anchorCurrentPos()
{
  tc_.setPosition(tc_.position(), QTextCursor::MoveAnchor);
}

bool EmacsModeHandler::wantsOverride(QKeyEvent *ev)
{
  TShortcutList shortcuts = partialShortcuts_.empty() ? shortcuts_ : partialShortcuts_;
  for (TShortcutList::const_iterator it = shortcuts.begin(); it != shortcuts.end(); ++it)
    if (it->isAccepted(ev))
      return true;
  return false;
}

EventResult EmacsModeHandler::handleEvent(QKeyEvent *ev)
{
  tc_ = EDITOR(textCursor());
  if (partialShortcuts_.empty())
    partialShortcuts_ = shortcuts_;

  TShortcutList newPartialShortcuts;
  bool executed = false;
  bool isAccepted = false;

  for (TShortcutList::const_iterator it = partialShortcuts_.begin(); it != partialShortcuts_.end(); ++it)
  {
    if (it->isAccepted(ev))
    {
      isAccepted = true;
      if (it->hasFollower(ev))
        newPartialShortcuts.push_back(it->getFollower(ev));
      else
      {
        it->exec();
        lastActionId_ = it->actionId();
        executed = true;
        partialShortcuts_.clear();
        break;
      }
    }
  }

  if (!executed)
    partialShortcuts_ = newPartialShortcuts;

  EDITOR(setTextCursor(tc_));

  return isAccepted ? EventHandled : EventPassedToCore;
}

void EmacsModeHandler::installEventFilter()
{
  EDITOR(installEventFilter(this));
}

void EmacsModeHandler::setUndoPosition(int pos)
{
  undoCursorPosition_[tc_.document()->availableUndoSteps()] = pos;
}

void EmacsModeHandler::moveToEndOfLineAction()
{
  // does not work for "hidden" documents like in the autotests
  tc_.movePosition(QTextCursor::EndOfLine, moveMode_);
}

void EmacsModeHandler::moveToStartOfLineAction()
{
  // does not work for "hidden" documents like in the autotests
  tc_.movePosition(QTextCursor::StartOfLine, moveMode_);
}

void EmacsModeHandler::updateMiniBuffer()
{
  if (!textedit_ && !plaintextedit_)
    return;

  QString msg;
  MessageLevel messageLevel = MessageInfo;

  if (!pluginState.currentMessage_.isEmpty()) {
    msg = pluginState.currentMessage_;
    pluginState.currentMessage_.clear();
    messageLevel = pluginState.currentMessageLevel_;
  }

  emit commandBufferChanged(msg, messageLevel);
}

int EmacsModeHandler::cursorLine() const
{
  return lineForPosition(position()) - 1;
}

int EmacsModeHandler::physicalCursorColumn() const
{
  return position() - block().position();
}

void EmacsModeHandler::showMessage(MessageLevel level, const QString &msg)
{
  pluginState.currentMessage_ = msg;
  pluginState.currentMessageLevel_ = level;
  updateMiniBuffer();
}

void EmacsModeHandler::moveUpAction(int n) {
  tc_.movePosition(QTextCursor::Up, moveMode_, n);
}

void EmacsModeHandler::moveDownAction(int n) {
  tc_.movePosition(QTextCursor::Down, moveMode_, n);
}

void EmacsModeHandler::moveRightAction(int n) {
  tc_.movePosition(QTextCursor::Right, moveMode_, n);
}

void EmacsModeHandler::moveLeftAction(int n) {
  tc_.movePosition(QTextCursor::Left, moveMode_, n);
}

void EmacsModeHandler::newLineAction() {
  tc_.insertBlock();
}

void EmacsModeHandler::backspaceAction() {
  tc_.deletePreviousChar();
}

void EmacsModeHandler::insertBackSlashAction() {
  tc_.insertText(QString::fromLatin1("\\"));
}

void EmacsModeHandler::insertStraightDelimAction() {
  tc_.insertText(QString::fromLatin1("|"));
}

void EmacsModeHandler::setAnchor(int position) {
  anchor_ = position;
}

void EmacsModeHandler::setPosition(int position) {
  tc_.setPosition(position, QTextCursor::MoveAnchor);
}


void EmacsModeHandler::selectRange(int beginLine, int endLine)
{
  if (beginLine == -1)
    beginLine = cursorLineInDocument();
  if (endLine == -1)
    endLine = cursorLineInDocument();
  if (beginLine > endLine)
    qSwap(beginLine, endLine);
  setAnchor(firstPositionInLine(beginLine));
  if (endLine == linesInDocument())
    setPosition(lastPositionInLine(endLine));
  else
    setPosition(firstPositionInLine(endLine + 1));
}

void EmacsModeHandler::indentRegionAction()
{
  int beginLine = lineForPosition(tc_.anchor());
  indentRegionWithCharacter(firstNonBlankOnLine(beginLine));
}

void EmacsModeHandler::indentRegionWithCharacter(QChar typedChar)
{
  //int savedPos = anchor();
  int beginLine = lineForPosition(tc_.anchor());
  int endLine = lineForPosition(tc_.position());
  if (beginLine > endLine)
    qSwap(beginLine, endLine);

  emit indentRegionRequested(beginLine, endLine, typedChar);
}

int EmacsModeHandler::cursorLineInDocument() const
{
  return tc_.block().blockNumber();
}

int EmacsModeHandler::linesInDocument() const
{
  return tc_.isNull() ? 0 : tc_.document()->blockCount();
}

int EmacsModeHandler::lastPositionInDocument() const
{
  QTextBlock block = tc_.document()->lastBlock();
  return block.position() + block.length() - 1;
}

QString EmacsModeHandler::lineContents(int line) const
{
  return tc_.document()->findBlockByNumber(line - 1).text();
}

void EmacsModeHandler::setLineContents(int line, const QString &contents) const
{
  QTextBlock block = tc_.document()->findBlockByNumber(line - 1);
  QTextCursor tc = tc_;
  tc.setPosition(block.position());
  tc.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
  tc.removeSelectedText();
  tc.insertText(contents);
}

int EmacsModeHandler::firstPositionInLine(int line) const
{
  return tc_.document()->findBlockByNumber(line - 1).position();
}

int EmacsModeHandler::lastPositionInLine(int line) const
{
  QTextBlock block = tc_.document()->findBlockByNumber(line - 1);
  return block.position() + block.length() - 1;
}

int EmacsModeHandler::lineForPosition(int pos) const
{
  QTextCursor tc = tc_;
  tc.setPosition(pos);
  return tc.block().blockNumber() + 1;
}

void EmacsModeHandler::undoAction()
{
  int current = tc_.document()->availableUndoSteps();
  int currentPos = -1;
  if (undoCursorPosition_.contains(current))
    currentPos = undoCursorPosition_[current];

  EDITOR(undo());

  if (currentPos != -1)
    tc_.setPosition(currentPos);

  int rev = tc_.document()->availableUndoSteps();
  if (current == rev)
    showMessage(MessageInfo, EmacsModeHandler::tr("Already at oldest change"));
  else
    showMessage(MessageInfo, QString());

  /*
  if (undoCursorPosition_.contains(rev))
    tc_.setPosition(undoCursorPosition_[rev]);
*/}

void EmacsModeHandler::redoAction()
{
  int current = tc_.document()->availableUndoSteps();
  //endEditBlock();
  EDITOR(redo());
  //beginEditBlock();
  int rev = tc_.document()->availableUndoSteps();
  if (rev == current)
    showMessage(MessageInfo, EmacsModeHandler::tr("Already at newest change"));
  else
    showMessage(MessageInfo, QString());

  if (undoCursorPosition_.contains(rev))
    tc_.setPosition(undoCursorPosition_[rev]);
}

} // namespace Internal
} // namespace EmacsMode

#include "moc_emacsmodehandler.cpp"
