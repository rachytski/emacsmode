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

#include "emacsmodeoptionpage.h"

#include <coreplugin/icore.h>


namespace EmacsMode {
namespace Internal {

  const char * EmacsModeOptionPage::INSTALL_HANDLER                = "TextEditor.EmacsModeHandler";
  const char * EmacsModeOptionPage::SETTINGS_CATEGORY              = "D.EmacsMode";
  const char * EmacsModeOptionPage::SETTINGS_CATEGORY_EMACSMODE_ICON = ":/emacsmode/category_icon.png";
  const char * EmacsModeOptionPage::SETTINGS_ID                    = "A.General";

  EmacsModeOptionPage::EmacsModeOptionPage()
  {
    setId(SETTINGS_ID);
    setDisplayName(tr("General"));
    setCategory(SETTINGS_CATEGORY);
    setDisplayCategory(tr("EmacsMode"));
    setCategoryIcon(_(SETTINGS_CATEGORY_EMACSMODE_ICON));
  }

  QWidget *EmacsModeOptionPage::widget()
  {
      if (!m_widget) {
          m_widget = new QWidget;
          m_ui.setupUi(m_widget);

          m_group.clear();
          m_group.insert(theEmacsModeSetting(ConfigUseEmacsMode),
                         m_ui.checkBoxUseEmacsMode);

          m_group.insert(theEmacsModeSetting(ConfigShiftWidth),
                         m_ui.spinBoxShiftWidth);

          m_group.insert(theEmacsModeSetting(ConfigTabStop),
                         m_ui.spinBoxTabStop);

          m_group.insert(theEmacsModeSetting(ConfigExpandTab),
                         m_ui.checkBoxExpandTabs);
      }
      return m_widget;
  }

  void EmacsModeOptionPage::apply()
  {
      m_group.apply(ICore::settings());
  }

  void EmacsModeOptionPage::finish()
  {
      m_group.finish();
      delete m_widget;
  }

}
}
