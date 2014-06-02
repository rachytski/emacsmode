#include "emacsmodeoptionpage.h"

#include <coreplugin/icore.h>


namespace EmacsMode {
namespace Internal {

  const char * EmacsModeOptionPage::INSTALL_HANDLER                = "TextEditor.EmacsModeHandler";
  const char * EmacsModeOptionPage::SETTINGS_CATEGORY              = "D.EmacsMode";
  const char * EmacsModeOptionPage::SETTINGS_CATEGORY_EMACSMODE_ICON = ":/core/images/category_fakevim.png";
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
