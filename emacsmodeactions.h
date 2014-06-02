#ifndef EMACSMODE_ACTIONS_H
#define EMACSMODE_ACTIONS_H

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

#endif // EmacsMode_ACTTIONS_H

