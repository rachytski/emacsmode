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
//   There is always a "current" cursor (m_tc). A current "region of interest"
//   spans between m_anchor (== anchor()) and  m_tc.position() (== position())
//   The value of m_tc.anchor() is not used.
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

#define EDITOR(s) (m_textedit ? m_textedit->s : m_plaintextedit->s)

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
  m_textedit = qobject_cast<QTextEdit *>(widget);
  m_plaintextedit = qobject_cast<QPlainTextEdit *>(widget);
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
  return m_tc.atBlockEnd() && m_tc.block().length() > 1; 
}

QTextBlock EmacsModeHandler::block() const { 
  return m_tc.block(); 
}

void EmacsModeHandler::setCurrentFileName(const QString &fileName)
{
  m_currentFileName = fileName;
}

QWidget *EmacsModeHandler::widget()
{
  return editor();
}

QWidget *EmacsModeHandler::editor() const
{
  return m_textedit
      ? static_cast<QWidget *>(m_textedit)
      : static_cast<QWidget *>(m_plaintextedit);
}

void EmacsModeHandler::beginEditBlock() {
  m_tc.beginEditBlock();
}

void EmacsModeHandler::beginEditBlock(int pos) {
  setUndoPosition(pos);
  beginEditBlock();
}

void EmacsModeHandler::endEditBlock() {
  m_tc.endEditBlock();
}

void EmacsModeHandler::joinPreviousEditBlock() {
  m_tc.joinPreviousEditBlock();
}

int EmacsModeHandler::anchor() const {
  return m_anchor;
}

int EmacsModeHandler::position() const {
  return m_tc.position();
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
  if (m_recordCursorPosition)
  {
    setUndoPosition(position);
    m_recordCursorPosition = false;
  }
}

void EmacsModeHandler::onUndoCommandAdded()
{
  m_recordCursorPosition = true;
}

void EmacsModeHandler::init()
{
  cleanKillRing();

  m_shortcuts.push_back(Shortcut("<META>|p", Action(Action::Id::MoveUp, std::bind(&EmacsModeHandler::moveUpAction, this, 1))));
  m_shortcuts.push_back(Shortcut("<META>|n", Action(Action::Id::MoveDown, std::bind(&EmacsModeHandler::moveDownAction, this, 1))));
  m_shortcuts.push_back(Shortcut("<META>|f", Action(Action::Id::MoveRight, std::bind(&EmacsModeHandler::moveRightAction, this, 1))));
  m_shortcuts.push_back(Shortcut("<META>|b", Action(Action::Id::MoveLeft, std::bind(&EmacsModeHandler::moveLeftAction, this, 1))));
  m_shortcuts.push_back(Shortcut("<META>|m", Action(Action::Id::NewLine, std::bind(&EmacsModeHandler::newLineAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|h", Action(Action::Id::Backspace, std::bind(&EmacsModeHandler::backspaceAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|e", Action(Action::Id::MoveToEndOfLine, std::bind(&EmacsModeHandler::moveToEndOfLineAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|a", Action(Action::Id::MoveToStartOfLine, std::bind(&EmacsModeHandler::moveToStartOfLineAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|<SHIFT>|<UNDERSCORE>", Action(Action::Id::Undo, std::bind(&EmacsModeHandler::undoAction, this))));
  m_shortcuts.push_back(Shortcut("<TAB>", Action(Action::Id::IndentRegion, std::bind(&EmacsModeHandler::indentRegionAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|<SPACE>", Action(Action::Id::StartSelection, std::bind(&EmacsModeHandler::startSelectionAction, this))));
  m_shortcuts.push_back(Shortcut("<ESC>|<ESC>", Action(Action::Id::CancelCurrentCommand, std::bind(&EmacsModeHandler::cancelCurrentCommandAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|w", Action(Action::Id::KillSelected, std::bind(&EmacsModeHandler::killSelectedAction, this))));
  m_shortcuts.push_back(Shortcut("<ALT>|w", Action(Action::Id::CopySelected, std::bind(&EmacsModeHandler::copySelectedAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|<SLASH>", Action(Action::Id::InsertBackSlash, std::bind(&EmacsModeHandler::insertBackSlashAction, this))));
  m_shortcuts.push_back(Shortcut("<CONTROL>|<SLASH>", Action(Action::Id::InsertStraightDelim, std::bind(&EmacsModeHandler::insertStraightDelimAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|k", Action(Action::Id::KillLine, std::bind(&EmacsModeHandler::killLineAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|d", Action(Action::Id::KillSymbol, std::bind(&EmacsModeHandler::killSymbolAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|y", Action(Action::Id::YankCurrent, std::bind(&EmacsModeHandler::yankCurrentAction, this))));
  m_shortcuts.push_back(Shortcut("<ALT>|y", Action(Action::Id::YankNext, std::bind(&EmacsModeHandler::yankNextAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|x|s", Action(Action::Id::SaveCurrentBuffer, std::bind(&EmacsModeHandler::saveCurrentFileAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|i|c", Action(Action::Id::CommentOutRegion, std::bind(&EmacsModeHandler::commentOutRegionAction, this))));
  m_shortcuts.push_back(Shortcut("<META>|i|u", Action(Action::Id::UncommentRegion, std::bind(&EmacsModeHandler::uncommentRegionAction, this))));
}

void EmacsModeHandler::saveCurrentFileAction()
{
  saveToFile(m_currentFileName);
}

void EmacsModeHandler::moveToFirstNonBlankOnLine()
{
  moveToFirstNonBlankOnLine(&m_tc);
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
  int beginLine = lineForPosition(m_tc.anchor());
  int endLine = lineForPosition(m_tc.position());

  if (m_tc.atBlockStart() && m_tc.atBlockEnd())
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
    m_tc.insertText(QString::fromLatin1("//"));
  }
  endEditBlock();

  setPosition(firstPos);
  //    moveToFirstNonBlankOnLine();
}

void EmacsModeHandler::uncommentRegionAction()
{
  int beginLine = lineForPosition(m_tc.anchor());
  int endLine = lineForPosition(m_tc.position());

  if (m_tc.atBlockStart() && m_tc.atBlockEnd())
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
    m_tc.deleteChar();
    m_tc.deleteChar();
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
  if (range.rangemode == RangeCharMode) {
    QTextCursor tc(document());
    tc.setPosition(range.beginPos, QTextCursor::MoveAnchor);
    tc.setPosition(range.endPos, QTextCursor::KeepAnchor);
    return tc.selection().toPlainText();
  }
  if (range.rangemode == RangeLineMode) {
    QTextCursor tc(document());
    int firstPos = firstPositionInLine(lineForPosition(range.beginPos));
    int lastLine = lineForPosition(range.endPos);
    int lastDocumentLine = document()->blockCount();
    bool endOfDoc = lastLine == lastDocumentLine;
    int lastPos = endOfDoc ? lastPositionInDocument() : firstPositionInLine(lastLine + 1);
    tc.setPosition(firstPos, QTextCursor::MoveAnchor);
    tc.setPosition(lastPos, QTextCursor::KeepAnchor);
    QString contents = tc.selection().toPlainText();
    return contents;// + QLatin1String(endOfDoc ? "\n" : "");
  }
  // FIXME: Performance?
  int beginLine = lineForPosition(range.beginPos);
  int endLine = lineForPosition(range.endPos);
  int beginColumn = 0;
  int endColumn = INT_MAX;
  if (range.rangemode == RangeBlockMode) {
    int column1 = range.beginPos - firstPositionInLine(beginLine);
    int column2 = range.endPos - firstPositionInLine(endLine);
    beginColumn = qMin(column1, column2);
    endColumn = qMax(column1, column2);
  }
  int len = endColumn - beginColumn + 1;
  QString contents;
  QTextBlock block = document()->findBlockByNumber(beginLine - 1);
  for (int i = beginLine; i <= endLine && block.isValid(); ++i) {
    QString line = block.text();
    if (range.rangemode == RangeBlockMode) {
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
    QTextCursor tc = m_tc;
    Range range(firstPositionInLine(beginLine),
                firstPositionInLine(endLine), RangeLineMode);
    QString contents = selectText(range);
    m_tc = tc;
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
  pluginState.killRing_.appendTop(m_tc.selectedText());
  anchorCurrentPos();
  setMoveMode(QTextCursor::MoveAnchor);
}

void EmacsModeHandler::killSelectedAction()
{
  startNewKillBufferEntryIfNecessary();
  pluginState.killRing_.appendTop(m_tc.selectedText());
  m_tc.removeSelectedText();
  anchorCurrentPos();
  setMoveMode(QTextCursor::MoveAnchor);
}

void EmacsModeHandler::killSymbolAction()
{
  startNewKillBufferEntryIfNecessary();

  m_tc.setPosition(m_tc.position(), QTextCursor::MoveAnchor);
  m_tc.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  pluginState.killRing_.appendTop(m_tc.selectedText());

  m_tc.removeSelectedText();
}

void EmacsModeHandler::cleanKillRing()
{
  pluginState.killRing_.clear();
}

void EmacsModeHandler::yankCurrentAction()
{
  if (!pluginState.killRing_.empty()) {
    startYankPosition_ = m_tc.position();
    m_tc.insertText(pluginState.killRing_.current());
    endYankPosition_ = m_tc.position();
  }
}

void EmacsModeHandler::yankNextAction() {
  if ((lastActionId_ == Action::Id::YankCurrent) || ((lastActionId_ == Action::Id::YankNext) && isValidYankChain_)) {
    m_tc.setPosition(startYankPosition_, QTextCursor::MoveAnchor);
    m_tc.setPosition(endYankPosition_, QTextCursor::KeepAnchor);
    m_tc.removeSelectedText();
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
  bool isEndOfLine = (atEndOfLine() || (m_tc.block().length() == 1));

  if (!isEndOfLine)
  {
    m_tc.setPosition(m_tc.position(), QTextCursor::MoveAnchor);
    m_tc.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    pluginState.killRing_.appendTop(m_tc.selectedText());
  }
  else
  {
    m_tc.setPosition(m_tc.position(), QTextCursor::MoveAnchor);
    m_tc.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    pluginState.killRing_.appendTop(m_tc.selectedText());
  }
  m_tc.removeSelectedText();
}

void EmacsModeHandler::setMoveMode(QTextCursor::MoveMode moveMode)
{
  m_moveMode = moveMode;
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
  m_tc.setPosition(m_tc.position(), QTextCursor::MoveAnchor);
}

bool EmacsModeHandler::wantsOverride(QKeyEvent *ev)
{
  TShortcutList shortcuts = m_partialShortcuts.empty() ? m_shortcuts : m_partialShortcuts;
  for (TShortcutList::const_iterator it = shortcuts.begin(); it != shortcuts.end(); ++it)
    if (it->isAccepted(ev))
      return true;
  return false;
}

EventResult EmacsModeHandler::handleEvent(QKeyEvent *ev)
{
  m_tc = EDITOR(textCursor());
  if (m_partialShortcuts.empty())
    m_partialShortcuts = m_shortcuts;

  TShortcutList newPartialShortcuts;
  bool executed = false;
  bool isAccepted = false;

  for (TShortcutList::const_iterator it = m_partialShortcuts.begin(); it != m_partialShortcuts.end(); ++it)
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
        m_partialShortcuts.clear();
        break;
      }
    }
  }

  if (!executed)
    m_partialShortcuts = newPartialShortcuts;

  EDITOR(setTextCursor(m_tc));

  return isAccepted ? EventHandled : EventPassedToCore;
}

void EmacsModeHandler::installEventFilter()
{
  EDITOR(installEventFilter(this));
}

void EmacsModeHandler::setUndoPosition(int pos)
{
  m_undoCursorPosition[m_tc.document()->availableUndoSteps()] = pos;
}

void EmacsModeHandler::moveToEndOfLineAction()
{
  // does not work for "hidden" documents like in the autotests
  m_tc.movePosition(QTextCursor::EndOfLine, m_moveMode);
}

void EmacsModeHandler::moveToStartOfLineAction()
{
  // does not work for "hidden" documents like in the autotests
  m_tc.movePosition(QTextCursor::StartOfLine, m_moveMode);
}

void EmacsModeHandler::updateMiniBuffer()
{
  if (!m_textedit && !m_plaintextedit)
    return;

  QString msg;
  MessageLevel messageLevel = MessageInfo;

  if (!pluginState.currentMessage.isEmpty()) {
    msg = pluginState.currentMessage;
    pluginState.currentMessage.clear();
    messageLevel = pluginState.currentMessageLevel;
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
  pluginState.currentMessage = msg;
  pluginState.currentMessageLevel = level;
  updateMiniBuffer();
}

void EmacsModeHandler::moveUpAction(int n) {
  m_tc.movePosition(QTextCursor::Up, m_moveMode, n);
}

void EmacsModeHandler::moveDownAction(int n) {
  m_tc.movePosition(QTextCursor::Down, m_moveMode, n);
}

void EmacsModeHandler::moveRightAction(int n) {
  m_tc.movePosition(QTextCursor::Right, m_moveMode, n);
}

void EmacsModeHandler::moveLeftAction(int n) {
  m_tc.movePosition(QTextCursor::Left, m_moveMode, n);
}

void EmacsModeHandler::newLineAction() {
  m_tc.insertBlock();
}

void EmacsModeHandler::backspaceAction() {
  m_tc.deletePreviousChar();
}

void EmacsModeHandler::insertBackSlashAction() {
  m_tc.insertText(QString::fromLatin1("\\"));
}

void EmacsModeHandler::insertStraightDelimAction() {
  m_tc.insertText(QString::fromLatin1("|"));
}

void EmacsModeHandler::setAnchor(int position) {
  m_anchor = position;
}

void EmacsModeHandler::setPosition(int position) {
  m_tc.setPosition(position, QTextCursor::MoveAnchor);
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
  int beginLine = lineForPosition(m_tc.anchor());
  indentRegionWithCharacter(firstNonBlankOnLine(beginLine));
}

void EmacsModeHandler::indentRegionWithCharacter(QChar typedChar)
{
  //int savedPos = anchor();
  int beginLine = lineForPosition(m_tc.anchor());
  int endLine = lineForPosition(m_tc.position());
  if (beginLine > endLine)
    qSwap(beginLine, endLine);

  emit indentRegionRequested(beginLine, endLine, typedChar);
}

int EmacsModeHandler::cursorLineInDocument() const
{
  return m_tc.block().blockNumber();
}

int EmacsModeHandler::linesInDocument() const
{
  return m_tc.isNull() ? 0 : m_tc.document()->blockCount();
}

int EmacsModeHandler::lastPositionInDocument() const
{
  QTextBlock block = m_tc.document()->lastBlock();
  return block.position() + block.length() - 1;
}

QString EmacsModeHandler::lineContents(int line) const
{
  return m_tc.document()->findBlockByNumber(line - 1).text();
}

void EmacsModeHandler::setLineContents(int line, const QString &contents) const
{
  QTextBlock block = m_tc.document()->findBlockByNumber(line - 1);
  QTextCursor tc = m_tc;
  tc.setPosition(block.position());
  tc.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
  tc.removeSelectedText();
  tc.insertText(contents);
}

int EmacsModeHandler::firstPositionInLine(int line) const
{
  return m_tc.document()->findBlockByNumber(line - 1).position();
}

int EmacsModeHandler::lastPositionInLine(int line) const
{
  QTextBlock block = m_tc.document()->findBlockByNumber(line - 1);
  return block.position() + block.length() - 1;
}

int EmacsModeHandler::lineForPosition(int pos) const
{
  QTextCursor tc = m_tc;
  tc.setPosition(pos);
  return tc.block().blockNumber() + 1;
}

void EmacsModeHandler::undoAction()
{
  int current = m_tc.document()->availableUndoSteps();
  int currentPos = -1;
  if (m_undoCursorPosition.contains(current))
    currentPos = m_undoCursorPosition[current];

  EDITOR(undo());

  if (currentPos != -1)
    m_tc.setPosition(currentPos);

  int rev = m_tc.document()->availableUndoSteps();
  if (current == rev)
    showMessage(MessageInfo, EmacsModeHandler::tr("Already at oldest change"));
  else
    showMessage(MessageInfo, QString());

  /*
  if (m_undoCursorPosition.contains(rev))
    m_tc.setPosition(m_undoCursorPosition[rev]);
*/}

void EmacsModeHandler::redoAction()
{
  int current = m_tc.document()->availableUndoSteps();
  //endEditBlock();
  EDITOR(redo());
  //beginEditBlock();
  int rev = m_tc.document()->availableUndoSteps();
  if (rev == current)
    showMessage(MessageInfo, EmacsModeHandler::tr("Already at newest change"));
  else
    showMessage(MessageInfo, QString());

  if (m_undoCursorPosition.contains(rev))
    m_tc.setPosition(m_undoCursorPosition[rev]);
}

} // namespace Internal
} // namespace EmacsMode

#include "moc_emacsmodehandler.cpp"
