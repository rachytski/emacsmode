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

#include "minibuffer.hpp"

#include <QLabel>
#include <QLineEdit>

namespace EmacsMode{
namespace Internal {

MiniBuffer::MiniBuffer()
  : label_(new QLabel(this))
  , lastMessageLevel_(MessageInfo)
{
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  addWidget(label_);

  hideTimer_.setSingleShot(true);
  hideTimer_.setInterval(8000);
  connect(&hideTimer_, SIGNAL(timeout()), SLOT(hide()));
}

void MiniBuffer::setContents(const QString &contents, int messageLevel)
{
  if (contents.isEmpty())
  {
    hideTimer_.start();
  }
  else
  {
    hideTimer_.stop();
    show();

    label_->setText(contents);

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
    label_->setStyleSheet(QString::fromLatin1(
                             "*{border-radius:2px;padding-left:4px;padding-right:4px;%1}").arg(css));
  }

  setCurrentWidget(label_);

  lastMessageLevel_ = messageLevel;
}

QSize MiniBuffer::sizeHint() const
{
  QSize size = QWidget::sizeHint();
  return size;
}

}
}

