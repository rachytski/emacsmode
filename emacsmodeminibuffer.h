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

#pragma once

#include <QStackedWidget>
#include <QTimer>

#include "emacsmodehandler.h"

class QLabel;
class QLineEdit;
class QObject;

namespace EmacsMode {
namespace Internal {

class MiniBuffer : public QStackedWidget
{
    Q_OBJECT

public:
    MiniBuffer();

    void setContents(const QString &contents, int cursorPos, int anchorPos,
                     int messageLevel, QObject *eventFilter);

    QSize sizeHint() const;

signals:
    void edited(const QString &text, int cursorPos, int anchorPos);

private slots:
    void changed();

private:
    QLabel *m_label;
    QLineEdit *m_edit;
    QObject *m_eventFilter;
    QTimer m_hideTimer;
    int m_lastMessageLevel;
};

}
}
