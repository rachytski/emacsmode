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

#include "emacsmodesettings.h"

// Please do not add any direct dependencies to other Qt Creator code  here. 
// Instead emit signals and let the EmacsModePlugin channel the information to
// Qt Creator. The idea is to keep this file here in a "clean" state that
// allows easy reuse with any QTextEdit or QPlainTextEdit derived class.


#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtCore/QtAlgorithms>
#include <QtCore/QCoreApplication>
#include <QtCore/QStack>

using namespace Utils;

///////////////////////////////////////////////////////////////////////
//
// EmacsModeSettingsF
//
///////////////////////////////////////////////////////////////////////

namespace EmacsMode {
namespace Internal {

EmacsModeSettings::EmacsModeSettings()
{}

EmacsModeSettings::~EmacsModeSettings()
{
  qDeleteAll(m_items);
}

void EmacsModeSettings::insertItem(int code, SavedAction *item,
                                   const QString &longName, const QString &shortName)
{
  QTC_ASSERT(!m_items.contains(code), qDebug() << code << item->toString(); return);
  m_items[code] = item;
  if (!longName.isEmpty()) {
    m_nameToCode[longName] = code;
    m_codeToName[code] = longName;
  }
  if (!shortName.isEmpty()) {
    m_nameToCode[shortName] = code;
  }
}

void EmacsModeSettings::readSettings(QSettings *settings)
{
  foreach (SavedAction *item, m_items)
    item->readSettings(settings);
}

void EmacsModeSettings::writeSettings(QSettings *settings)
{
  foreach (SavedAction *item, m_items)
    item->writeSettings(settings);
}

SavedAction *EmacsModeSettings::item(int code)
{
  QTC_ASSERT(m_items.value(code, 0), qDebug() << "CODE: " << code; return 0);
  return m_items.value(code, 0);
}

SavedAction *EmacsModeSettings::item(const QString &name)
{
  return m_items.value(m_nameToCode.value(name, -1), 0);
}

EmacsModeSettings *theEmacsModeSettings()
{
  static EmacsModeSettings *instance = 0;
  if (instance)
    return instance;

  instance = new EmacsModeSettings;

  SavedAction *item = 0;

  const QString group = QLatin1String("EmacsMode");
  item = new SavedAction(instance);
  item->setText(QCoreApplication::translate("EmacsMode::Internal", "Toggle Emacs-style editing"));
  item->setSettingsKey(group, QLatin1String("UseEmacsMode"));
  item->setCheckable(true);
  item->setValue(true);
  instance->insertItem(ConfigUseEmacsMode, item);

  item = new SavedAction(instance);
  item->setDefaultValue(4);
  item->setSettingsKey(group, QLatin1String("TabStop"));
  instance->insertItem(ConfigTabStop, item, QLatin1String("tabstop"), QLatin1String("ts"));

  item = new SavedAction(instance);
  item->setDefaultValue(4);
  item->setSettingsKey(group, QLatin1String("ShiftWidth"));
  instance->insertItem(ConfigShiftWidth, item, QLatin1String("shiftwidth"), QLatin1String("sw"));

  item = new SavedAction(instance);
  item->setDefaultValue(false);
  item->setSettingsKey(group, QLatin1String("ExpandTabs"));
  instance->insertItem(ConfigExpandTab, item, QLatin1String("expandtabs"), QLatin1String("et"));

  return instance;
}

SavedAction *theEmacsModeSetting(int code)
{
  return theEmacsModeSettings()->item(code);
}

} // namespace Internal
} // namespace EmacsMode

