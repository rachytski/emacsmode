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

#include <utils/savedaction.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace EmacsMode {
namespace Internal {

enum EmacsModeSettingsCode
{
  ConfigUseEmacsMode,
  ConfigTabStop,
  ConfigShiftWidth,
  ConfigExpandTab
};

class EmacsModeSettings : public QObject
{
public:
  EmacsModeSettings();
  ~EmacsModeSettings();
  void insertItem(int code, Utils::SavedAction *item,
                  const QString &longname = QString(),
                  const QString &shortname = QString());

  Utils::SavedAction *item(int code);
  Utils::SavedAction *item(const QString &name);

  void readSettings(QSettings *settings);
  void writeSettings(QSettings *settings);

private:
  QHash<int, Utils::SavedAction *> m_items;
  QHash<QString, int> m_nameToCode;
  QHash<int, QString> m_codeToName;
};

EmacsModeSettings *theEmacsModeSettings();
Utils::SavedAction *theEmacsModeSetting(int code);

} // namespace Internal
} // namespace EmacsMode


