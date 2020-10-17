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

#pragma once

#include "emacsmodesettings.h"
#include "shortcut.hpp"
#include "pluginstate.hpp"

#include <QtCore/QObject>

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QtGui/QTextBlock>

namespace EmacsMode {
namespace Internal {

enum EventResult
{
  EventHandled,
  EventUnhandled,
  EventPassedToCore
};

class Range;

class EmacsModeHandler : public QObject
{
  Q_OBJECT

public:
  EmacsModeHandler(QWidget *widget, QObject *parent = 0);

  QWidget *widget();

signals:
  void commandBufferChanged(const QString &msg, int messageLevel);

  void writeFileRequested(bool *handled,
                          const QString &fileName, const QString &contents);
  void writeAllRequested(QString *error);
  void indentRegionRequested(int beginLine, int endLine, QChar typedChar);

public slots:
  void onContentsChanged(int position, int charsRemoved, int charsAdded);
  void onUndoCommandAdded();

private:
  bool eventFilter(QObject *ob, QEvent *ev);

public:  
  
  void setCurrentFileName(const QString &fileName);
  void installEventFilter(); 
  
  // Convenience
  void setupWidget();
  void restoreWidget(int tabSize);
  
  void showMessage(MessageLevel level, QString const& msg);  
  
public:  
  EventResult handleEvent(QKeyEvent *ev);
  bool wantsOverride(QKeyEvent *ev);

  void init();

  bool atEndOfLine() const;

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

  QTextBlock block() const;

  void indentRegion();
  void indentRegionWithCharacter(QChar lastTyped);

  void moveToStartOfLine();
  void moveToEndOfLine();
  void moveUp(int n = 1) { m_tc.movePosition(QTextCursor::Up, m_moveMode, n); }
  void moveDown(int n = 1) { m_tc.movePosition(QTextCursor::Down, m_moveMode, n); }
  void moveRight(int n = 1) { m_tc.movePosition(QTextCursor::Right, m_moveMode, n); }
  void moveLeft(int n = 1) { m_tc.movePosition(QTextCursor::Left, m_moveMode, n); }
  void newLine(){m_tc.insertBlock();}
  void backspace(){m_tc.deletePreviousChar();}
  void insertBackSlash(){m_tc.insertText(QString::fromLatin1("\\"));}
  void insertStraightDelim(){m_tc.insertText(QString::fromLatin1("|"));}
  void setAnchor(int position) { m_anchor = position; }
  void setPosition(int position) { m_tc.setPosition(position, QTextCursor::MoveAnchor); }

  void selectRange(int beginLine, int endLine);

  int lineNumber(const QTextBlock &block) const;

  QTextDocument *document() const;

  void moveToFirstNonBlankOnLine();
  void moveToFirstNonBlankOnLine(QTextCursor *tc);
  void moveToNonBlankOnLine(QTextCursor *tc);
  QChar const firstNonBlankOnLine(int line);

  void updateMiniBuffer();
  //    void updateSelection();
  QWidget *editor() const;
  void beginEditBlock() { m_tc.beginEditBlock(); }
  void beginEditBlock(int pos) { setUndoPosition(pos); beginEditBlock(); }
  void endEditBlock() { m_tc.endEditBlock(); }
  void joinPreviousEditBlock() { m_tc.joinPreviousEditBlock(); }
  int cursorLine() const;
  int physicalCursorColumn() const;

public:
  QTextEdit *m_textedit = nullptr;
  QPlainTextEdit *m_plaintextedit = nullptr;

  QTextCursor m_tc;
  int m_anchor = 0;

  QString m_currentFileName;

  int anchor() const { return m_anchor; }
  int position() const { return m_tc.position(); }

  QString selectText(const Range &range) const;

  // undo handling
  void undo();
  void redo();
  void setUndoPosition(int pos);
  
  bool m_recordCursorPosition = false;
  QMap<int, int> m_undoCursorPosition; // revision -> position

  QVariant config(int code) const { return theEmacsModeSetting(code)->value(); }
  bool hasConfig(int code) const { return config(code).toBool(); }
  bool hasConfig(int code, const char *value) const // FIXME
  { return config(code).toString().contains(QString::fromLatin1(value)); }

  typedef std::list<EmacsMode::Shortcut> TShortcutList;
  TShortcutList m_shortcuts;
  TShortcutList m_partialShortcuts;

  QTextCursor::MoveMode m_moveMode = QTextCursor::MoveAnchor;

  void setMoveMode(QTextCursor::MoveMode moveMode);
  void anchorCurrentPos();

  bool m_isAppendingKillBuffer = false;
  void setKillBufferAppending(bool flag);

  void cleanKillRing();
  void killLine();
  void killSymbol();

  void yank();

  void copySelected();
  void killSelected();

  void saveToFile(QString const & fileName);
  void saveCurrentFile();

  void commentOutRegion();
  void uncommentRegion();

  static PluginState pluginState;
};

} // namespace Internal
} // namespace EmacsMode

