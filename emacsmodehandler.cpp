/**************************************************************************
**
** Copyright (c) 2014 Siarhei Rachytski
**
** Contact: Siarhei Rachytski (siarhei.rachytski@gmail.com)
**
** Commercial Usage
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

#include "emacsmodehandler.h"
#include <functional>

//
// ATTENTION:
//
// 1 Please do not add any direct dependencies to other Qt Creator code here.
//   Instead emit signals and let the EmacsModePlugin channel the information to
//   Qt Creator. The idea is to keep this file here in a "clean" state that
//   allows easy reuse with any QTextEdit or QPlainTextEdit derived class.
//
// 2 There are a few auto tests located in ../../../tests/auto/EmacsMode.
//   Commands that are covered there are marked as "// tested" below.
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

using namespace Utils;

namespace EmacsMode {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// EmacsModeHandler
//
///////////////////////////////////////////////////////////////////////

#define StartOfLine     QTextCursor::StartOfLine
#define EndOfLine       QTextCursor::EndOfLine
#define MoveAnchor      QTextCursor::MoveAnchor
#define KeepAnchor      QTextCursor::KeepAnchor
#define Up              QTextCursor::Up
#define Down            QTextCursor::Down
#define Right           QTextCursor::Right
#define Left            QTextCursor::Left
#define EndOfDocument   QTextCursor::End
#define StartOfDocument QTextCursor::Start

#define EDITOR(s) (m_textedit ? m_textedit->s : m_plaintextedit->s)

const int ParagraphSeparator = 0x00002029;

using namespace Qt;

enum EventResult
{
    EventHandled,
    EventUnhandled,
    EventPassedToCore
};

Range::Range()
    : beginPos(-1), endPos(-1), rangemode(RangeCharMode)
{}

Range::Range(int b, int e, RangeMode m)
    : beginPos(qMin(b, e)), endPos(qMax(b, e)), rangemode(m)
{}

QString Range::toString() const
{
    return QString::fromLatin1("%1-%2 (mode: %3)").arg(beginPos).arg(endPos)
        .arg(rangemode);
}

bool Range::isValid() const
{
    return beginPos >= 0 && endPos >= 0;
}

QDebug operator<<(QDebug ts, const Range &range)
{
    return ts << '[' << range.beginPos << ',' << range.endPos << ']';
}

class EmacsModeHandler::Private
{
public:

    friend class EmacsModeHandler;


    Private(EmacsModeHandler *parent, QWidget *widget);

    EventResult handleEvent(QKeyEvent *ev);
    bool wantsOverride(QKeyEvent *ev);

    void installEventFilter();

    void init();

    bool atEndOfLine() const
        { return m_tc.atBlockEnd() && m_tc.block().length() > 1; }

    int lastPositionInDocument() const; // last valid pos in doc
    int firstPositionInLine(int line) const; // 1 based line, 0 based pos
    int lastPositionInLine(int line) const; // 1 based line, 0 based pos
    int lineForPosition(int pos) const;  // 1 based line, 0 based pos
    QString lineContents(int line) const; // 1 based line
    void setLineContents(int line, const QString &contents) const; // 1 based line

    int linesInDocument() const;

    // all zero-based counting
    int cursorLineInDocument() const;
    int firstVisibleLineInDocument() const;

    QTextBlock block() const { return m_tc.block(); }

    void indentRegion(QChar lastTyped = QChar());

    void moveToStartOfLine();
    void moveToEndOfLine();
    void moveUp(int n = 1) { m_tc.movePosition(Up, m_moveMode, n); }
    void moveDown(int n = 1) { m_tc.movePosition(Down, m_moveMode, n); }
    void moveRight(int n = 1) { m_tc.movePosition(Right, m_moveMode, n); }
    void moveLeft(int n = 1) { m_tc.movePosition(Left, m_moveMode, n); }
    void newLine(){m_tc.insertBlock();}
    void backspace(){m_tc.deletePreviousChar();}
    void insertBackSlash(){m_tc.insertText(QString::fromAscii("\\"));}
    void insertStraightDelim(){m_tc.insertText(QString::fromAscii("|"));}
    void setAnchor(int position) { m_anchor = position; }
    void setPosition(int position) { m_tc.setPosition(position, MoveAnchor); }

    void selectRange(int beginLine, int endLine);

    int lineNumber(const QTextBlock &block) const;

    QTextDocument *document() const { return EDITOR(document()); }

    void moveToFirstNonBlankOnLine();
    void moveToFirstNonBlankOnLine(QTextCursor *tc);
    void moveToNonBlankOnLine(QTextCursor *tc);

    void showMessage(MessageLevel level, const QString &msg);
    void notImplementedYet();
    void updateMiniBuffer();
//    void updateSelection();
    QWidget *editor() const;
    void beginEditBlock() { m_tc.beginEditBlock(); }
    void beginEditBlock(int pos) { setUndoPosition(pos); beginEditBlock(); }
    void endEditBlock() { m_tc.endEditBlock(); }
    void joinPreviousEditBlock() { m_tc.joinPreviousEditBlock(); }
    int cursorLine() const;
    int physicalCursorColumn() const;

    void setupWidget();
    void restoreWidget(int tabSize);

public:
    QTextEdit *m_textedit;
    QPlainTextEdit *m_plaintextedit;

    EmacsModeHandler *q;
    QTextCursor m_tc;
    int m_anchor;

    QString m_currentFileName;

    int anchor() const { return m_anchor; }
    int position() const { return m_tc.position(); }

    QString selectText(const Range &range) const;

    // undo handling
    void undo();
    void redo();
    void setUndoPosition(int pos);
    QMap<int, int> m_undoCursorPosition; // revision -> position

    QVariant config(int code) const { return theEmacsModeSetting(code)->value(); }
    bool hasConfig(int code) const { return config(code).toBool(); }
    bool hasConfig(int code, const char *value) const // FIXME
        { return config(code).toString().contains(QString::fromAscii(value)); }

    typedef std::list<EmacsMode::Shortcut> TShortcutList;
    TShortcutList m_shortcuts;
    TShortcutList m_partialShortcuts;

    QTextCursor::MoveMode m_moveMode;

    void setMoveMode(QTextCursor::MoveMode moveMode);
    void anchorCurrentPos();

    QStringList m_killBuffer;

    void cleanKillBuffer();
    void killLine();

    void yank();

    void copySelected();
    void killSelected();

    bool m_isAppendingKillBuffer;
    void setKillBufferAppending(bool flag);

    void saveToFile(QString const & fileName);
    void saveCurrentFile();

    void commentOutRegion();
    void uncommentRegion();

    void miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos);

    // Data shared among all editors.
    static struct GlobalData
    {
        GlobalData()
            : currentMessageLevel(MessageInfo)
        {
        }

        // Current mini buffer message.
        QString currentMessage;
        MessageLevel currentMessageLevel;
        QString currentCommand;
    } g;
};


EmacsModeHandler::Private::GlobalData EmacsModeHandler::Private::g;


EmacsModeHandler::Private::Private(EmacsModeHandler *parent, QWidget *widget)
{
    q = parent;
    m_textedit = qobject_cast<QTextEdit *>(widget);
    m_plaintextedit = qobject_cast<QPlainTextEdit *>(widget);
    init();
}

void EmacsModeHandler::Private::setupWidget()
{
  updateMiniBuffer();
}

void EmacsModeHandler::Private::restoreWidget(int tabSize)
{
  EDITOR(setOverwriteMode(false));
}

void EmacsModeHandler::Private::init()
{
    m_anchor = 0;

    m_moveMode = QTextCursor::MoveAnchor;
    m_isAppendingKillBuffer = false;
    cleanKillBuffer();

    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|p")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveUp, this, 1))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|n")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveDown, this, 1))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|f")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveRight, this, 1))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|b")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveLeft, this, 1))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|m")
                            .addFn(std::bind(&EmacsModeHandler::Private::newLine, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|h")
                            .addFn(std::bind(&EmacsModeHandler::Private::backspace, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|e")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveToEndOfLine, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|a")
                            .addFn(std::bind(&EmacsModeHandler::Private::moveToStartOfLine, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|<SHIFT>|<UNDERSCORE>")
                            .addFn(std::bind(&EmacsModeHandler::Private::undo, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<TAB>")
                            .addFn(std::bind(&EmacsModeHandler::Private::indentRegion, this, QChar::fromAscii('{')))
                            .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|<SPACE>")
                            .addFn(std::bind(&EmacsModeHandler::Private::anchorCurrentPos, this))
                            .addFn(std::bind(&EmacsModeHandler::Private::setMoveMode, this, QTextCursor::KeepAnchor)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<ESC>|<ESC>")
                            .addFn(std::bind(&EmacsModeHandler::Private::setMoveMode, this, QTextCursor::MoveAnchor))
                            .addFn(std::bind(&EmacsModeHandler::Private::anchorCurrentPos, this)));

    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|w").addFn(std::bind(&EmacsModeHandler::Private::killSelected, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<ALT>|w").addFn(std::bind(&EmacsModeHandler::Private::copySelected, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|<SLASH>").addFn(std::bind(&EmacsModeHandler::Private::insertBackSlash, this))
                                                           .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<CONTROL>|<SLASH>").addFn(std::bind(&EmacsModeHandler::Private::insertStraightDelim, this))
                                                                   .addFn(std::bind(&EmacsModeHandler::Private::setKillBufferAppending, this, false)));
//    m_shortcuts.push_back(Emacs::Shortcut("<ALT>|w").addFn(std::bind(&EmacsModeHandler::Private::copySelected, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|k").addFn(std::bind(&EmacsModeHandler::Private::killLine, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|y").addFn(std::bind(&EmacsModeHandler::Private::yank, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|x|s").addFn(std::bind(&EmacsModeHandler::Private::saveCurrentFile, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|i|c").addFn(std::bind(&EmacsModeHandler::Private::commentOutRegion, this)));
    m_shortcuts.push_back(EmacsMode::Shortcut("<META>|i|u").addFn(std::bind(&EmacsModeHandler::Private::uncommentRegion, this)));
}

void EmacsModeHandler::Private::saveCurrentFile()
{
    saveToFile(m_currentFileName);
}

void EmacsModeHandler::Private::moveToFirstNonBlankOnLine()
{
    moveToFirstNonBlankOnLine(&m_tc);
}

void EmacsModeHandler::Private::moveToFirstNonBlankOnLine(QTextCursor *tc)
{
    tc->setPosition(tc->block().position(), KeepAnchor);
    moveToNonBlankOnLine(tc);
}

void EmacsModeHandler::Private::moveToNonBlankOnLine(QTextCursor *tc)
{
    QTextDocument *doc = tc->document();
    const QTextBlock block = tc->block();
    const int maxPos = block.position() + block.length() - 1;
    int i = tc->position();
    while (doc->characterAt(i).isSpace() && i < maxPos)
        ++i;
    tc->setPosition(i, KeepAnchor);
}

void EmacsModeHandler::Private::commentOutRegion()
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
        m_tc.insertText(QString::fromAscii("//"));
    }
    endEditBlock();

    setPosition(firstPos);
//    moveToFirstNonBlankOnLine();
}

void EmacsModeHandler::Private::uncommentRegion()
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

int EmacsModeHandler::Private::lineNumber(const QTextBlock &block) const
{
    if (block.isVisible())
        return block.firstLineNumber() + 1;

    // Folded block has line number of the nearest previous visible line.
    QTextBlock block2 = block;
    while (block2.isValid() && !block2.isVisible())
        block2 = block2.previous();
    return block2.firstLineNumber() + 1;
}

QString EmacsModeHandler::Private::selectText(const Range &range) const
{
    if (range.rangemode == RangeCharMode) {
        QTextCursor tc(document());
        tc.setPosition(range.beginPos, MoveAnchor);
        tc.setPosition(range.endPos, KeepAnchor);
        return tc.selection().toPlainText();
    }
    if (range.rangemode == RangeLineMode) {
        QTextCursor tc(document());
        int firstPos = firstPositionInLine(lineForPosition(range.beginPos));
        int lastLine = lineForPosition(range.endPos);
        int lastDocumentLine = document()->blockCount();
        bool endOfDoc = lastLine == lastDocumentLine;
        int lastPos = endOfDoc ? lastPositionInDocument() : firstPositionInLine(lastLine + 1);
        tc.setPosition(firstPos, MoveAnchor);
        tc.setPosition(lastPos, KeepAnchor);
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
    QTextBlock block = document()->findBlockByLineNumber(beginLine - 1);
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

void EmacsModeHandler::Private::saveToFile(QString const & fileName)
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
        emit q->writeFileRequested(&handled, fileName, contents);
        // nobody cared, so act ourselves
        if (!handled) {
            QFile::remove(fileName);
            QFile file2(fileName);
            if (file2.open(QIODevice::ReadWrite)) {
              QTextStream ts(&file2);
              ts << contents;
              ts.flush();
            } else {
              qDebug() << QString::fromAscii("Cannot open file '%1' for writing").arg(fileName);
                showMessage(MessageError, EmacsModeHandler::tr
                   ("Cannot open file '%1' for writing").arg(fileName));
            }
        }
        // check result by reading back
        QFile file3(fileName);
        file3.open(QIODevice::ReadOnly);
        QByteArray ba = file3.readAll();
        showMessage(MessageInfo, EmacsModeHandler::tr("\"%1\" %2 %3L, %4C written")
            .arg(fileName).arg(exists ? QString::fromAscii(" ") : QString::fromAscii(" [New] "))
            .arg(ba.count('\n')).arg(ba.size()));
    }
    else
    {
        showMessage(MessageError, EmacsModeHandler::tr
            ("Cannot open file '%1' for reading").arg(fileName));
    }
}

void EmacsModeHandler::Private::setKillBufferAppending(bool flag)
{
    m_isAppendingKillBuffer = flag;
}

void EmacsModeHandler::Private::copySelected()
{
    if (!m_isAppendingKillBuffer)
        m_killBuffer.clear();
    m_isAppendingKillBuffer = true;
    m_killBuffer.append(m_tc.selectedText());
    anchorCurrentPos();
    setKillBufferAppending(true);
    setMoveMode(MoveAnchor);
}

void EmacsModeHandler::Private::killSelected()
{
    if (!m_isAppendingKillBuffer)
        m_killBuffer.clear();
    m_isAppendingKillBuffer = true;
    m_killBuffer.append(m_tc.selectedText());
    m_tc.removeSelectedText();
    anchorCurrentPos();
    setKillBufferAppending(true);
    setMoveMode(MoveAnchor);
}

void EmacsModeHandler::Private::cleanKillBuffer()
{
    m_killBuffer.clear();
}

void EmacsModeHandler::Private::yank()
{
    m_tc.insertText(m_killBuffer.join(QString::fromAscii("")));
}

void EmacsModeHandler::Private::killLine()
{
    if (!m_isAppendingKillBuffer)
        m_killBuffer.clear();
    m_isAppendingKillBuffer = true;
    bool isEndOfLine = (atEndOfLine() || (m_tc.block().length() == 1));

    if (!isEndOfLine)
    {
      m_tc.setPosition(m_tc.position(), MoveAnchor);
      m_tc.movePosition(QTextCursor::EndOfLine, KeepAnchor);
      m_killBuffer.append(m_tc.selectedText());
    }
    else
    {
      m_tc.setPosition(m_tc.position(), MoveAnchor);
      m_tc.movePosition(QTextCursor::NextCharacter, KeepAnchor);
      m_killBuffer.append(m_tc.selectedText());
    }
    m_tc.removeSelectedText();
}

void EmacsModeHandler::Private::setMoveMode(QTextCursor::MoveMode moveMode)
{
    m_moveMode = moveMode;
}

void EmacsModeHandler::Private::anchorCurrentPos()
{
    m_tc.setPosition(m_tc.position(), MoveAnchor);
}

bool EmacsModeHandler::Private::wantsOverride(QKeyEvent *ev)
{
  TShortcutList shortcuts = m_partialShortcuts.empty() ? m_shortcuts : m_partialShortcuts;
  for (TShortcutList::const_iterator it = shortcuts.begin(); it != shortcuts.end(); ++it)
    if (it->isAccepted(ev))
      return true;
  return false;
}

EventResult EmacsModeHandler::Private::handleEvent(QKeyEvent *ev)
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

void EmacsModeHandler::Private::installEventFilter()
{
    EDITOR(installEventFilter(q));
}

void EmacsModeHandler::Private::setUndoPosition(int pos)
{
    m_undoCursorPosition[m_tc.document()->availableUndoSteps()] = pos;
}

void EmacsModeHandler::Private::moveToEndOfLine()
{
    // does not work for "hidden" documents like in the autotests
    m_tc.movePosition(EndOfLine, m_moveMode);
}

void EmacsModeHandler::Private::moveToStartOfLine()
{
    // does not work for "hidden" documents like in the autotests
    m_tc.movePosition(StartOfLine, m_moveMode);
}

void EmacsModeHandler::Private::updateMiniBuffer()
{
    if (!m_textedit && !m_plaintextedit)
        return;

    QString msg;
    int cursorPos = -1;
    int anchorPos = -1;
    MessageLevel messageLevel = MessageInfo;

    if (!g.currentMessage.isEmpty()) {
        msg = g.currentMessage;
        g.currentMessage.clear();
        messageLevel = g.currentMessageLevel;
    }

    emit q->commandBufferChanged(msg, cursorPos, anchorPos, messageLevel, q);

    int linesInDoc = linesInDocument();
    int l = cursorLine();
    QString status;
    const QString pos = QString::fromLatin1("%1,%2")
        .arg(l + 1).arg(physicalCursorColumn() + 1);

    if (linesInDoc != 0)
        status = EmacsModeHandler::tr("%1%2%").arg(pos, -10).arg(l * 100 / linesInDoc, 4);
    else
        status = EmacsModeHandler::tr("%1All").arg(pos, -10);
    emit q->statusDataChanged(status);
}

void EmacsModeHandler::Private::miniBufferTextEdited(const QString &text, int cursorPos,
    int anchorPos)
{
  emit q->commandBufferChanged(text, cursorPos, anchorPos, 0, q);
}

int EmacsModeHandler::Private::cursorLine() const
{
    return lineForPosition(position()) - 1;
}

int EmacsModeHandler::Private::physicalCursorColumn() const
{
    return position() - block().position();
}

void EmacsModeHandler::Private::showMessage(MessageLevel level, const QString &msg)
{
    //qDebug() << "MSG: " << msg;
    g.currentMessage = msg;
    g.currentMessageLevel = level;
    updateMiniBuffer();
}

void EmacsModeHandler::Private::notImplementedYet()
{
    qDebug() << "Not implemented in EmacsMode";
    showMessage(MessageError, EmacsModeHandler::tr("Not implemented in EmacsMode"));
    updateMiniBuffer();
}

void EmacsModeHandler::Private::selectRange(int beginLine, int endLine)
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

void EmacsModeHandler::Private::indentRegion(QChar typedChar)
{
    //int savedPos = anchor();
    int beginLine = lineForPosition(m_tc.anchor());
    int endLine = lineForPosition(m_tc.position());
    if (beginLine > endLine)
        qSwap(beginLine, endLine);

    emit q->indentRegion(beginLine, endLine, typedChar);

    setPosition(firstPositionInLine(beginLine));
    moveToFirstNonBlankOnLine();
}

int EmacsModeHandler::Private::cursorLineInDocument() const
{
    return m_tc.block().blockNumber();
}

int EmacsModeHandler::Private::linesInDocument() const
{
    return m_tc.isNull() ? 0 : m_tc.document()->blockCount();
}

int EmacsModeHandler::Private::lastPositionInDocument() const
{
    QTextBlock block = m_tc.document()->lastBlock();
    return block.position() + block.length() - 1;
}

QString EmacsModeHandler::Private::lineContents(int line) const
{
    return m_tc.document()->findBlockByNumber(line - 1).text();
}

void EmacsModeHandler::Private::setLineContents(int line, const QString &contents) const
{
    QTextBlock block = m_tc.document()->findBlockByNumber(line - 1);
    QTextCursor tc = m_tc;
    tc.setPosition(block.position());
    tc.setPosition(block.position() + block.length() - 1, KeepAnchor);
    tc.removeSelectedText();
    tc.insertText(contents);
}

int EmacsModeHandler::Private::firstPositionInLine(int line) const
{
    return m_tc.document()->findBlockByNumber(line - 1).position();
}

int EmacsModeHandler::Private::lastPositionInLine(int line) const
{
    QTextBlock block = m_tc.document()->findBlockByNumber(line - 1);
    return block.position() + block.length() - 1;
}

int EmacsModeHandler::Private::lineForPosition(int pos) const
{
    QTextCursor tc = m_tc;
    tc.setPosition(pos);
    return tc.block().blockNumber() + 1;
}

QWidget *EmacsModeHandler::Private::editor() const
{
    return m_textedit
        ? static_cast<QWidget *>(m_textedit)
        : static_cast<QWidget *>(m_plaintextedit);
}

void EmacsModeHandler::Private::undo()
{
    int current = m_tc.document()->availableUndoSteps();
    EDITOR(undo());
    int rev = m_tc.document()->availableUndoSteps();
    if (current == rev)
        showMessage(MessageInfo, EmacsModeHandler::tr("Already at oldest change"));
    else
        showMessage(MessageInfo, QString());

    if (m_undoCursorPosition.contains(rev))
        m_tc.setPosition(m_undoCursorPosition[rev]);
}

void EmacsModeHandler::Private::redo()
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

///////////////////////////////////////////////////////////////////////
//
// EmacsModeHandler
//
///////////////////////////////////////////////////////////////////////

EmacsModeHandler::EmacsModeHandler(QWidget *widget, QObject *parent)
    : QObject(parent), d(new Private(this, widget))
{}

EmacsModeHandler::~EmacsModeHandler()
{
    delete d;
}

bool EmacsModeHandler::eventFilter(QObject *ob, QEvent *ev)
{
    bool active = theEmacsModeSetting(ConfigUseEmacsMode)->value().toBool();

    if (active && ev->type() == QEvent::KeyPress && ob == d->editor()) {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
        EventResult res = d->handleEvent(kev);
        return res == EventHandled;
    }

    if (active && ev->type() == QEvent::ShortcutOverride && ob == d->editor()) {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
        if (d->wantsOverride(kev)) {
            ev->accept(); // accepting means "don't run the shortcuts"
            return true;
        }
        return true;
    }

    return QObject::eventFilter(ob, ev);
}

void EmacsModeHandler::installEventFilter()
{
    d->installEventFilter();
}

void EmacsModeHandler::setupWidget()
{
    d->setupWidget();
}

void EmacsModeHandler::restoreWidget(int tabSize)
{
    d->restoreWidget(tabSize);
}

void EmacsModeHandler::setCurrentFileName(const QString &fileName)
{
   d->m_currentFileName = fileName;
}

void EmacsModeHandler::showMessage(MessageLevel level, const QString &msg)
{
    d->showMessage(level, msg);
}

void EmacsModeHandler::miniBufferTextEdited(QString const& text, int cursorPos, int anchorPos)
{
  d->miniBufferTextEdited(text, cursorPos, anchorPos);
}

QWidget *EmacsModeHandler::widget()
{
    return d->editor();
}

} // namespace Internal
} // namespace EmacsMode

#include "moc_emacsmodehandler.cpp"
