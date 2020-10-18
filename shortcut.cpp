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

#include "shortcut.hpp"
#include <QString>
#include <QStringList>

namespace EmacsMode
{

Shortcut::Shortcut(Qt::KeyboardModifiers mods, std::vector<int> keys, Action action)
  : mods_(mods), keys_(std::move(keys)), action_(action)
{}

Shortcut::Shortcut(char const * s, Action action)
    : action_(std::move(action))
{
  QStringList l = QString::fromLatin1(s).split(QString::fromLocal8Bit("|"));
  QString keys = l.at(l.size() - 1).toLower();

  for (int i = 0; i < l.size(); ++i)
  {
    QString key = l.at(i).toUpper();
    if (key == QString::fromLocal8Bit("<CONTROL>"))
      mods_ |= Qt::ControlModifier;
    else if (key == QString::fromLocal8Bit("<META>"))
    {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
      mods_ |= Qt::ControlModifier;
#else
      mods_ |= Qt::MetaModifier;
#endif
    }
    else if (key == QString::fromLocal8Bit("<SHIFT>"))
      mods_ |= Qt::ShiftModifier;
    else if (key == QString::fromLocal8Bit("<ALT>"))
      mods_ |= Qt::AltModifier;
    else if (key == QString::fromLocal8Bit("<TAB>"))
      keys_.push_back(Qt::Key_Tab);
    else if (key == QString::fromLocal8Bit("<SPACE>"))
      keys_.push_back(Qt::Key_Space);
    else if (key == QString::fromLocal8Bit("<UNDERSCORE>"))
      keys_.push_back(Qt::Key_Underscore);
    else if (key == QString::fromLocal8Bit("<ESC>"))
      keys_.push_back(Qt::Key_Escape);
    else if (key == QString::fromLocal8Bit("<SLASH>"))
      keys_.push_back(Qt::Key_Slash);
    else
      keys_.push_back(key.at(0).toLatin1() - 'A' + Qt::Key_A);
  }
}

Shortcut::Shortcut()
{}


bool Shortcut::isEmpty() const
{
  return keys_.size() != 0;
}

bool Shortcut::isAccepted(QKeyEvent * kev) const
{
  int key = kev->key();
  Qt::KeyboardModifiers mods = kev->modifiers();
  return ((mods == mods_) && (key == keys_.front()));
}

bool Shortcut::hasFollower(QKeyEvent * kev) const
{
  return (isAccepted(kev) && (keys_.size() > 1));
}

Shortcut const Shortcut::getFollower(QKeyEvent * kev) const
{
  if (hasFollower(kev))
  {
    std::vector<int> keys;
    std::copy(++keys_.begin(), keys_.end(), std::back_inserter(keys));
    return Shortcut(mods_, std::move(keys), action_);
  }
  return Shortcut();
}

Action::Id Shortcut::actionId() const {
  return action_.id();
}

void Shortcut::exec() const
{
  action_.exec();
}

}
