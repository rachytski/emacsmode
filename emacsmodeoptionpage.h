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

#include <QLatin1String>
#include <QPointer>
#include <coreplugin/dialogs/ioptionspage.h>
#include <utils/savedaction.h>
#include "ui_emacsmodeoptions.h"
#include "emacsmodeactions.h"

typedef QLatin1String _;

namespace EmacsMode {
namespace Internal {

using namespace Core;

class EmacsModeOptionPage : public IOptionsPage
{
  Q_OBJECT

public:

  static const char * INSTALL_HANDLER;
  static const char * SETTINGS_CATEGORY;
  static const char * SETTINGS_CATEGORY_EMACSMODE_ICON;
  static const char * SETTINGS_ID;

  EmacsModeOptionPage();

  QWidget *widget();
  void apply();
  void finish();

private:
  friend class DebuggerPlugin;
  QPointer<QWidget> m_widget;
  Ui::EmacsModeOptionPage m_ui;
  Utils::SavedActionSet m_group;
};

//const char *EMACSMODE_CONTEXT = "EmacsMode";
}
}
