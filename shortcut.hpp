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

#include <vector>
#include <Qt>
#include <QKeyEvent>
#include "action.hpp"

namespace EmacsMode
{

class Shortcut
{
private:

  Qt::KeyboardModifiers mods_;
  std::vector<int> keys_;
  Action action_;

public:

  Shortcut();
  Shortcut(char const * s, Action action);
  Shortcut(Qt::KeyboardModifiers, std::vector<int> keys, Action action);

  void exec() const;

  Action::Id actionId() const;
  bool isEmpty() const;
  bool isAccepted(QKeyEvent * kev) const;
  bool hasFollower(QKeyEvent * kev) const;
  Shortcut const getFollower(QKeyEvent * kev) const;
};
}
