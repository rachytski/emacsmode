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
