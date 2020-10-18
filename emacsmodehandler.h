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

struct Range;

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

  void startNewKillBufferEntryIfNecessary();

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

  void indentRegionAction();
  void indentRegionWithCharacter(QChar lastTyped);

  void moveToStartOfLineAction();
  void moveToEndOfLineAction();
  void moveUpAction(int n = 1);
  void moveDownAction(int n = 1);
  void moveRightAction(int n = 1);
  void moveLeftAction(int n = 1);
  void newLineAction();
  void backspaceAction();
  void insertBackSlashAction();
  void insertStraightDelimAction();
  void setAnchor(int position);
  void setPosition(int position);

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
  void beginEditBlock();
  void beginEditBlock(int pos);
  void endEditBlock();
  void joinPreviousEditBlock();
  int cursorLine() const;
  int physicalCursorColumn() const;

  void anchorCurrentPos();

public:
  QTextEdit *m_textedit = nullptr;
  QPlainTextEdit *m_plaintextedit = nullptr;

  QTextCursor m_tc;
  int m_anchor = 0;

  QString m_currentFileName;

  int anchor() const;
  int position() const;

  QString selectText(const Range &range) const;

  // undo handling
  void undoAction();
  void redoAction();
  void setUndoPosition(int pos);
  
  bool m_recordCursorPosition = false;
  QMap<int, int> m_undoCursorPosition; // revision -> position

  QVariant config(int code) const;
  bool hasConfig(int code) const;
  bool hasConfig(int code, const char *value) const; // FIXME

  typedef std::list<EmacsMode::Shortcut> TShortcutList;
  TShortcutList m_shortcuts;
  TShortcutList m_partialShortcuts;
  Action::Id lastActionId_ = Action::Id::Null;

  QTextCursor::MoveMode m_moveMode = QTextCursor::MoveAnchor;

  void setMoveMode(QTextCursor::MoveMode moveMode);
  void startSelectionAction();

  void cleanKillRing();
  void killLineAction();
  void killSymbolAction();

  void yankAction();

  void copySelectedAction();
  void killSelectedAction();

  void saveToFile(QString const & fileName);
  void saveCurrentFileAction();

  void commentOutRegionAction();
  void uncommentRegionAction();

  void cancelCurrentCommandAction();

  static PluginState pluginState;
};

} // namespace Internal
} // namespace EmacsMode

