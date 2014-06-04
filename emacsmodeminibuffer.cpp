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

#include "emacsmodeminibuffer.h"

#include <QLabel>
#include <QLineEdit>

namespace EmacsMode{
namespace Internal {

MiniBuffer::MiniBuffer()
  : m_label(new QLabel(this))
  , m_lastMessageLevel(MessageInfo)
{
  m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

  addWidget(m_label);

  m_hideTimer.setSingleShot(true);
  m_hideTimer.setInterval(8000);
  connect(&m_hideTimer, SIGNAL(timeout()), SLOT(hide()));
}

void MiniBuffer::setContents(const QString &contents, int messageLevel)
{
  if (contents.isEmpty())
  {
    m_hideTimer.start();
  }
  else
  {
    m_hideTimer.stop();
    show();

    m_label->setText(contents);

    QString css;
    if (messageLevel == MessageError)
    {
      css = QLatin1String("border:1px solid rgba(255,255,255,150);"
                          "background-color:rgba(255,0,0,100);");
    } else if (messageLevel == MessageWarning) {
      css = QLatin1String("border:1px solid rgba(255,255,255,120);"
                          "background-color:rgba(255,255,0,20);");
    } else if (messageLevel == MessageShowCmd) {
      css = QLatin1String("border:1px solid rgba(255,255,255,120);"
                          "background-color:rgba(100,255,100,30);");
    }
    m_label->setStyleSheet(QString::fromLatin1(
                             "*{border-radius:2px;padding-left:4px;padding-right:4px;%1}").arg(css));
  }

  setCurrentWidget(m_label);

  m_lastMessageLevel = messageLevel;
}

QSize MiniBuffer::sizeHint() const
{
  QSize size = QWidget::sizeHint();
  return size;
}

}
}

