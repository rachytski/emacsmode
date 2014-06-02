/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef EMACSMODE_HANDLER_H
#define EMACSMODE_HANDLER_H

#include "emacsmodeactions.h"
#include "emacsmodeshortcut.h"
#include <list>

#include <QtCore/QObject>
#include <QTextEdit>

namespace EmacsMode {
namespace Internal {

// message levels sorted by severity
enum MessageLevel
{
    MessageMode,    // show current mode (format "-- %1 --")
    MessageCommand, // show last Ex command or search
    MessageInfo,    // result of a command
    MessageWarning, // warning
    MessageError,   // error
    MessageShowCmd  // partial command
};

enum RangeMode
{
    // Reordering first three enum items here will break
    // compatibility with clipboard format stored by Vim.
    RangeCharMode,         // v
    RangeLineMode,         // V
    RangeBlockMode,        // Ctrl-v
    RangeLineModeExclusive,
    RangeBlockAndTailMode // Ctrl-v for D and X
};

struct Range
{
    Range();
    Range(int b, int e, RangeMode m = RangeCharMode);
    QString toString() const;
    bool isValid() const;

    int beginPos;
    int endPos;
    RangeMode rangemode;
};

class EmacsModeHandler : public QObject
{
    Q_OBJECT

public:
    EmacsModeHandler(QWidget *widget, QObject *parent = 0);
    ~EmacsModeHandler();

    QWidget *widget();

public slots:
    void setCurrentFileName(const QString &fileName);
    void showMessage(MessageLevel level, QString const& msg);
    void miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos);

    void installEventFilter();

    // Convenience
    void setupWidget();
    void restoreWidget(int tabSize);

signals:
    void commandBufferChanged(const QString &msg, int cursorPos,
        int anchorPos, int messageLevel, QObject *eventFilter);

    void statusDataChanged(const QString &msg);
    void selectionChanged(const QList<QTextEdit::ExtraSelection> &selection);
    void writeFileRequested(bool *handled,
        const QString &fileName, const QString &contents);
    void writeAllRequested(QString *error);
    void moveToMatchingParenthesis(bool *moved, bool *forward, QTextCursor *cursor);
    void indentRegion(int beginLine, int endLine, QChar typedChar);
    void completionRequested();
    void windowCommandRequested(int key);
    void findRequested(bool reverse);
    void findNextRequested(bool reverse);
    void handleExCommandRequested(const QString &cmd);

public:
    class Private;

private:
    bool eventFilter(QObject *ob, QEvent *ev);

    Private *d;
};

} // namespace Internal
} // namespace EmacsMode

#endif // EMACSMODE_H
