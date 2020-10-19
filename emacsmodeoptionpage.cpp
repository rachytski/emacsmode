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

#include "emacsmodeoptionpage.hpp"

#include <coreplugin/icore.h>


namespace EmacsMode {
namespace Internal {

const char EmacsModeOptionPage::INSTALL_HANDLER[]                  = "TextEditor.EmacsModeHandler";
const char EmacsModeOptionPage::SETTINGS_CATEGORY[]                = "D.EmacsMode";
const char EmacsModeOptionPage::SETTINGS_CATEGORY_EMACSMODE_ICON[] = ":/emacsmode/category_icon.png";
const char EmacsModeOptionPage::SETTINGS_ID[]                      = "A.General";

EmacsModeOptionPage::EmacsModeOptionPage()
{
  setId(SETTINGS_ID);
  setDisplayName(tr("General"));
  setCategory(SETTINGS_CATEGORY);
  setDisplayCategory(tr("EmacsMode"));
  setCategoryIcon(Utils::Icon(SETTINGS_CATEGORY_EMACSMODE_ICON));
}

QWidget *EmacsModeOptionPage::widget()
{
  if (!widget_) {
    widget_ = new QWidget;
    ui_.setupUi(widget_);

    group_.clear();
    group_.insert(theEmacsModeSetting(ConfigUseEmacsMode),
                   ui_.checkBoxUseEmacsMode);

    group_.insert(theEmacsModeSetting(ConfigShiftWidth),
                   ui_.spinBoxShiftWidth);

    group_.insert(theEmacsModeSetting(ConfigTabStop),
                   ui_.spinBoxTabStop);

    group_.insert(theEmacsModeSetting(ConfigExpandTab),
                   ui_.checkBoxExpandTabs);
  }
  return widget_;
}

void EmacsModeOptionPage::apply()
{
  group_.apply(ICore::settings());
}

void EmacsModeOptionPage::finish()
{
  group_.finish();
  delete widget_;
}

}
}
