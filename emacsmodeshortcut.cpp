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

#include "emacsmodeshortcut.h"
#include <QString>
#include <QStringList>

namespace EmacsMode
{
Shortcut::Shortcut(Qt::KeyboardModifiers mods, std::vector<int> const & keys, fn_list_t const & fnList)
  : m_mods(mods), m_keys(keys), m_fnList(fnList)
{}

Shortcut::Shortcut(char const * s)
{
  QStringList l = QString::fromLatin1(s).split(QString::fromLocal8Bit("|"));
  QString keys = l.at(l.size() - 1).toLower();

  for (int i = 0; i < l.size(); ++i)
  {
    QString key = l.at(i).toUpper();
    if (key == QString::fromLocal8Bit("<CONTROL>"))
      m_mods |= Qt::ControlModifier;
    else if (key == QString::fromLocal8Bit("<META>"))
    {
#ifdef _WIN32
      m_mods |= Qt::ControlModifier;
#else
      m_mods |= Qt::MetaModifier;
#endif
    }
    else if (key == QString::fromLocal8Bit("<SHIFT>"))
      m_mods |= Qt::ShiftModifier;
    else if (key == QString::fromLocal8Bit("<ALT>"))
      m_mods |= Qt::AltModifier;
    else if (key == QString::fromLocal8Bit("<TAB>"))
      m_keys.push_back(Qt::Key_Tab);
    else if (key == QString::fromLocal8Bit("<SPACE>"))
      m_keys.push_back(Qt::Key_Space);
    else if (key == QString::fromLocal8Bit("<UNDERSCORE>"))
      m_keys.push_back(Qt::Key_Underscore);
    else if (key == QString::fromLocal8Bit("<ESC>"))
      m_keys.push_back(Qt::Key_Escape);
    else if (key == QString::fromLocal8Bit("<SLASH>"))
      m_keys.push_back(Qt::Key_Slash);
    else
      m_keys.push_back(key.at(0).toLatin1() - 'A' + Qt::Key_A);
  }
}

Shortcut::Shortcut()
{}


bool Shortcut::isEmpty() const
{
  return m_keys.size() != 0;
}

bool Shortcut::isAccepted(QKeyEvent * kev) const
{
  int key = kev->key();
  Qt::KeyboardModifiers mods = kev->modifiers();
  return ((mods == m_mods) && (key == m_keys.front()));
}

bool Shortcut::hasFollower(QKeyEvent * kev) const
{
  return (isAccepted(kev) && (m_keys.size() > 1));
}

Shortcut const Shortcut::getFollower(QKeyEvent * kev) const
{
  if (hasFollower(kev))
  {
    std::vector<int> keys;
    std::copy(++m_keys.begin(), m_keys.end(), std::back_inserter(keys));
    return Shortcut(m_mods, keys, m_fnList);
  }
  return Shortcut();
}

Shortcut & Shortcut::addFn(std::function<void()> fn)
{
  m_fnList.push_back(fn);
  return *this;
}

void Shortcut::exec() const
{
  for (fn_list_t::const_iterator it = m_fnList.begin(); it != m_fnList.end(); ++it)
    (*it)();
}

}
