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

#ifndef EMACS_SHORTCUT_H
#define EMACS_SHORTCUT_H

#include <functional>
#include <list>
#include <vector>
#include <Qt>
#include <QKeyEvent>

namespace EmacsMode
{
class Shortcut
{
private:

  Qt::KeyboardModifiers m_mods;
  std::vector<int> m_keys;

  typedef std::list<std::function<void()> > fn_list_t;
  fn_list_t m_fnList;

public:

  Shortcut();
  Shortcut(char const * s);
  Shortcut(Qt::KeyboardModifiers, std::vector<int> const & , fn_list_t const & );

  Shortcut & addFn(std::function<void()> fn);

  void exec() const;

  bool isEmpty() const;
  bool isAccepted(QKeyEvent * kev) const;
  bool hasFollower(QKeyEvent * kev) const;
  Shortcut const getFollower(QKeyEvent * kev) const;
};
}

#endif // EMACS_SHORTCUT_H
