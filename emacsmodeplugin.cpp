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

#include "emacsmodeplugin.h"

#include "emacsmodeminibuffer.h"
#include "emacsmodeactions.h"
#include "emacsmodehandler.h"
#include "emacsmodeoptionpage.h"
#include "ui_emacsmodeoptions.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/commandmappings.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/find/findplugin.h>
#include <coreplugin/find/textfindconstants.h>
#include <coreplugin/find/ifindsupport.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/id.h>
#include <coreplugin/statusbarwidget.h>
#include <coreplugin/statusbarmanager.h>

#include <projectexplorer/projectexplorerconstants.h>

#include <texteditor/basetextdocumentlayout.h>
#include <texteditor/basetexteditor.h>
#include <texteditor/basetextmark.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/typingsettings.h>
#include <texteditor/tabsettings.h>
#include <texteditor/icodestylepreferences.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/indenter.h>
#include <texteditor/codeassist/basicproposalitem.h>
#include <texteditor/codeassist/basicproposalitemlistmodel.h>
#include <texteditor/codeassist/completionassistprovider.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/iassistinterface.h>
#include <texteditor/codeassist/genericproposal.h>

#include <utils/fancylineedit.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/pathchooser.h>
#include <utils/qtcoverride.h>
#include <utils/savedaction.h>
#include <utils/stylehelper.h>

#include <cpptools/cpptoolsconstants.h>

#include <extensionsystem/pluginmanager.h>

#include <QAbstractTableModel>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QtPlugin>
#include <QObject>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextStream>

#include <QDesktopServices>
#include <QItemDelegate>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidgetItem>

using namespace TextEditor;
using namespace Core;

namespace EmacsMode {
namespace Internal {



///////////////////////////////////////////////////////////////////////
//
// OptionPage
//
///////////////////////////////////////////////////////////////////////

typedef QMap<QString, QRegExp> ExCommandMap;
typedef QMap<int, QString> UserCommandMap;

///////////////////////////////////////////////////////////////////////
//
// EmacsModePluginPrivate
//
///////////////////////////////////////////////////////////////////////

class EmacsModePluginPrivate : public QObject
{
    Q_OBJECT

public:
    EmacsModePluginPrivate(EmacsModePlugin *);
    ~EmacsModePluginPrivate();
    friend class EmacsModePlugin;
    friend class EmacsModeExCommandsPage;
    friend class EmacsModeUserCommandsPage;
    friend class EmacsModeUserCommandsModel;

    bool initialize();
    void aboutToShutdown();

private slots:
    void onCoreAboutToClose();
    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    void setUseEmacsMode(const QVariant &value);
    void setUseEmacsModeInternal(bool on);
    void quitEmacsMode();
    void showSettingsDialog();

    void resetCommandBuffer();
    void showCommandBuffer(const QString &contents, int messageLevel);

    void indentRegion(int beginBlock, int endBlock, QChar typedChar);

    void writeSettings();
    void readSettings();

    void handleDelayedQuitAll(bool forced);
    void handleDelayedQuit(bool forced, Core::IEditor *editor);

    void switchToFile(int n);
    int currentFile() const;

signals:
    void delayedQuitRequested(bool forced, Core::IEditor *editor);
    void delayedQuitAllRequested(bool forced);

private:
    EmacsModePlugin *q;
    EmacsModeOptionPage *m_emacsModeOptionsPage;
    QHash<IEditor *, EmacsModeHandler *> m_editorToHandler;

    void triggerAction(const Id &id);
    void setActionChecked(const Id &id, bool check);

    ExCommandMap &exCommandMap() { return m_exCommandMap; }
    ExCommandMap &defaultExCommandMap() { return m_defaultExCommandMap; }
    ExCommandMap m_exCommandMap;
    ExCommandMap m_defaultExCommandMap;

    UserCommandMap &userCommandMap() { return m_userCommandMap; }
    UserCommandMap &defaultUserCommandMap() { return m_defaultUserCommandMap; }
    UserCommandMap m_userCommandMap;
    UserCommandMap m_defaultUserCommandMap;

    StatusBarWidget *m_statusBar;
};

EmacsModePluginPrivate::EmacsModePluginPrivate(EmacsModePlugin *plugin)
{
    q = plugin;
    m_emacsModeOptionsPage = 0;
    defaultExCommandMap()[_(CppTools::Constants::SWITCH_HEADER_SOURCE)] =
        QRegExp(_("^A$"));
    defaultExCommandMap()[_("Coreplugin.OutputPane.previtem")] =
        QRegExp(_("^(cN(ext)?|cp(revious)?)!?( (.*))?$"));
    defaultExCommandMap()[_("Coreplugin.OutputPane.nextitem")] =
        QRegExp(_("^cn(ext)?!?( (.*))?$"));
    defaultExCommandMap()[_(TextEditor::Constants::FOLLOW_SYMBOL_UNDER_CURSOR)] =
        QRegExp(_("^tag?$"));
    defaultExCommandMap()[_(Core::Constants::GO_BACK)] =
        QRegExp(_("^pop?$"));
    defaultExCommandMap()[_("QtCreator.Locate")] =
        QRegExp(_("^e$"));

    for (int i = 1; i < 10; ++i) {
        QString cmd = QString::fromLatin1(":echo User command %1 executed.<CR>");
        defaultUserCommandMap().insert(i, cmd.arg(i));
    }

    m_statusBar = 0;
}

EmacsModePluginPrivate::~EmacsModePluginPrivate()
{
    q->removeObject(m_emacsModeOptionsPage);
    delete m_emacsModeOptionsPage;
    m_emacsModeOptionsPage = 0;

    theEmacsModeSettings()->deleteLater();
}

void EmacsModePluginPrivate::onCoreAboutToClose()
{
    // Don't attach to editors anymore.
    disconnect(EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
        this, SLOT(editorOpened(Core::IEditor*)));
}

void EmacsModePluginPrivate::aboutToShutdown()
{
}

bool EmacsModePluginPrivate::initialize()
{
    Context globalcontext(Core::Constants::C_GLOBAL);

    m_emacsModeOptionsPage = new EmacsModeOptionPage;
    q->addObject(m_emacsModeOptionsPage);

    readSettings();

    Command *cmd = 0;
    cmd = ActionManager::registerAction(theEmacsModeSetting(ConfigUseEmacsMode),
        EmacsModeOptionPage::INSTALL_HANDLER, globalcontext, true);
    cmd->setDefaultKeySequence(QKeySequence(UseMacShortcuts ? tr("Meta+V,Meta+V") : tr("Alt+V,Alt+V")));

    ActionContainer *advancedMenu =
        ActionManager::actionContainer(Core::Constants::M_EDIT_ADVANCED);
    advancedMenu->addAction(cmd, Core::Constants::G_EDIT_EDITOR);

    connect(ICore::instance(), SIGNAL(coreAboutToClose()), this, SLOT(onCoreAboutToClose()));

    // EditorManager
    connect(EditorManager::instance(), SIGNAL(editorAboutToClose(Core::IEditor*)),
        this, SLOT(editorAboutToClose(Core::IEditor*)));
    connect(EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
        this, SLOT(editorOpened(Core::IEditor*)));

    connect(theEmacsModeSetting(ConfigUseEmacsMode), SIGNAL(valueChanged(QVariant)),
        this, SLOT(setUseEmacsMode(QVariant)));

    // Delayed operations.
    connect(this, SIGNAL(delayedQuitRequested(bool,Core::IEditor*)),
        this, SLOT(handleDelayedQuit(bool,Core::IEditor*)), Qt::QueuedConnection);
    connect(this, SIGNAL(delayedQuitAllRequested(bool)),
        this, SLOT(handleDelayedQuitAll(bool)), Qt::QueuedConnection);

    return true;
}

const char exCommandMapGroup[] = "EmacsModeExCommand";
const char userCommandMapGroup[] = "EmacsModeUserCommand";
const char reKey[] = "RegEx";
const char cmdKey[] = "Cmd";
const char idKey[] = "Command";

void EmacsModePluginPrivate::writeSettings()
{
    QSettings *settings = ICore::settings();

    theEmacsModeSettings()->writeSettings(settings);

    { // block
    settings->beginWriteArray(_(exCommandMapGroup));
    int count = 0;
    typedef ExCommandMap::const_iterator Iterator;
    const Iterator end = exCommandMap().constEnd();
    for (Iterator it = exCommandMap().constBegin(); it != end; ++it) {
        const QString id = it.key();
        const QRegExp re = it.value();

        if ((defaultExCommandMap().contains(id) && defaultExCommandMap()[id] != re)
            || (!defaultExCommandMap().contains(id) && !re.pattern().isEmpty())) {
            settings->setArrayIndex(count);
            settings->setValue(_(idKey), id);
            settings->setValue(_(reKey), re.pattern());
            ++count;
        }
    }
    settings->endArray();
    } // block

    { // block
    settings->beginWriteArray(_(userCommandMapGroup));
    int count = 0;
    typedef UserCommandMap::const_iterator Iterator;
    const Iterator end = userCommandMap().constEnd();
    for (Iterator it = userCommandMap().constBegin(); it != end; ++it) {
        const int key = it.key();
        const QString cmd = it.value();

        if ((defaultUserCommandMap().contains(key)
                && defaultUserCommandMap()[key] != cmd)
            || (!defaultUserCommandMap().contains(key) && !cmd.isEmpty())) {
            settings->setArrayIndex(count);
            settings->setValue(_(idKey), key);
            settings->setValue(_(cmdKey), cmd);
            ++count;
        }
    }
    settings->endArray();
    } // block
}

void EmacsModePluginPrivate::readSettings()
{
    QSettings *settings = ICore::settings();

    theEmacsModeSettings()->readSettings(settings);

    exCommandMap() = defaultExCommandMap();
    int size = settings->beginReadArray(_(exCommandMapGroup));
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        const QString id = settings->value(_(idKey)).toString();
        const QString re = settings->value(_(reKey)).toString();
        exCommandMap()[id] = QRegExp(re);
    }
    settings->endArray();

    userCommandMap() = defaultUserCommandMap();
    size = settings->beginReadArray(_(userCommandMapGroup));
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        const int id = settings->value(_(idKey)).toInt();
        const QString cmd = settings->value(_(cmdKey)).toString();
        userCommandMap()[id] = cmd;
    }
    settings->endArray();
}


void EmacsModePluginPrivate::showSettingsDialog()
{
    ICore::showOptionsDialog(EmacsModeOptionPage::SETTINGS_CATEGORY, EmacsModeOptionPage::SETTINGS_ID);
}

void EmacsModePluginPrivate::triggerAction(const Id &id)
{
    Core::Command *cmd = ActionManager::command(id);
    QTC_ASSERT(cmd, qDebug() << "UNKNOWN CODE: " << id.name(); return);
    QAction *action = cmd->action();
    QTC_ASSERT(action, return);
    action->trigger();
}

void EmacsModePluginPrivate::setActionChecked(const Id &id, bool check)
{
    Core::Command *cmd = ActionManager::command(id);
    QTC_ASSERT(cmd, return);
    QAction *action = cmd->action();
    QTC_ASSERT(action, return);
    QTC_ASSERT(action->isCheckable(), return);
    action->setChecked(!check); // trigger negates the action's state
    action->trigger();
}

// This class defers deletion of a child EmacsModeHandler using deleteLater().
class DeferredDeleter : public QObject
{
    Q_OBJECT

    EmacsModeHandler *m_handler;

public:
    DeferredDeleter(QObject *parent, EmacsModeHandler *handler)
        : QObject(parent), m_handler(handler)
    {}

    ~DeferredDeleter()
    {
        if (m_handler) {
            //m_handler->disconnectFromEditor();
            m_handler->deleteLater();
            m_handler = 0;
        }
    }
};

void EmacsModePluginPrivate::editorOpened(IEditor *editor)
{
    if (!editor)
        return;

    QWidget *widget = editor->widget();
    if (!widget)
        return;

    // we can only handle QTextEdit and QPlainTextEdit
    if (!qobject_cast<QTextEdit *>(widget) && !qobject_cast<QPlainTextEdit *>(widget))
        return;

    qDebug() << "OPENING: " << editor << editor->widget()
        << "MODE: " << theEmacsModeSetting(ConfigUseEmacsMode)->value();

    EmacsModeHandler *handler = new EmacsModeHandler(widget, 0);
    // the handler might have triggered the deletion of the editor:
    // make sure that it can return before being deleted itself
    new DeferredDeleter(widget, handler);
    m_editorToHandler[editor] = handler;

    connect(handler, SIGNAL(commandBufferChanged(QString,int,int,int,QObject*)),
        SLOT(showCommandBuffer(QString,int,int,int,QObject*)));
    connect(handler, SIGNAL(indentRegion(int,int,QChar)),
        SLOT(indentRegion(int,int,QChar)));

    connect(ICore::instance(), SIGNAL(saveSettingsRequested()),
        SLOT(writeSettings()));

    handler->setCurrentFileName(editor->document()->filePath());
    handler->installEventFilter();

    // pop up the bar
    if (theEmacsModeSetting(ConfigUseEmacsMode)->value().toBool()) {
       resetCommandBuffer();
       handler->setupWidget();
    }
}

void EmacsModePluginPrivate::editorAboutToClose(IEditor *editor)
{
    m_editorToHandler.remove(editor);
}

void EmacsModePluginPrivate::setUseEmacsMode(const QVariant &value)
{
    bool on = value.toBool();
    setUseEmacsModeInternal(on);
}

void EmacsModePluginPrivate::setUseEmacsModeInternal(bool on)
{
    if (on) {
        foreach (IEditor *editor, m_editorToHandler.keys())
            m_editorToHandler[editor]->setupWidget();
    } else {
        //ICore *core = ICore::instance();
        //core->updateAdditionalContexts(Context(),
        // Context(EmacsMode_CONTEXT));
//        resetCommandBuffer();
        foreach (IEditor *editor, m_editorToHandler.keys()) {
            if (BaseTextDocument *textDocument =
                    qobject_cast<BaseTextDocument *>(editor->document())) {
                m_editorToHandler[editor]->restoreWidget(textDocument->tabSettings().m_tabSize);
            }
        }
    }
}

void EmacsModePluginPrivate::handleDelayedQuit(bool forced, IEditor *editor)
{
    // This tries to simulate vim behaviour. But the models of vim and
    // Qt Creator core do not match well...
    if (EditorManager::hasSplitter())
        triggerAction(Core::Constants::REMOVE_CURRENT_SPLIT);
    else
        EditorManager::closeEditor(editor, !forced);
}

void EmacsModePluginPrivate::handleDelayedQuitAll(bool forced)
{
    triggerAction(Core::Constants::REMOVE_ALL_SPLITS);
    EditorManager::closeAllEditors(!forced);
}

void EmacsModePluginPrivate::indentRegion(int beginBlock, int endBlock,
      QChar typedChar)
{
    EmacsModeHandler *handler = qobject_cast<EmacsModeHandler *>(sender());
    if (!handler)
        return;

    BaseTextEditorWidget *bt = qobject_cast<BaseTextEditorWidget *>(handler->widget());
    if (!bt)
        return;

    TabSettings tabSettings;
    tabSettings.m_indentSize = theEmacsModeSetting(ConfigShiftWidth)->value().toInt();
    tabSettings.m_tabSize = theEmacsModeSetting(ConfigTabStop)->value().toInt();
    //tabSettings.m_tabPolicy = theEmacsModeSetting(ConfigExpandTab)->value().toBool()
    //        ? TabSettings::SpacesOnlyTabPolicy : TabSettings::TabsOnlyTabPolicy;
    tabSettings.m_tabPolicy = TabSettings::SpacesOnlyTabPolicy;


    QTextDocument *doc = bt->document();
    QTextBlock startBlock = doc->findBlockByNumber(beginBlock);

    // Record line lenghts for mark adjustments
    QVector<int> lineLengths(endBlock - beginBlock + 1);
    QTextBlock block = startBlock;

    for (int i = beginBlock; i <= endBlock; ++i) {
        lineLengths[i - beginBlock] = block.text().length();
        if (typedChar == 0 && block.text().simplified().isEmpty()) {
            // clear empty lines
            QTextCursor cursor(block);
            while (!cursor.atBlockEnd())
                cursor.deleteChar();
        } else {
            bt->baseTextDocument()->indenter()->indentBlock(doc, block, typedChar, tabSettings);
        }
        block = block.next();
    }
}

void EmacsModePluginPrivate::quitEmacsMode()
{
    theEmacsModeSetting(ConfigUseEmacsMode)->setValue(false);
}

int EmacsModePluginPrivate::currentFile() const
{
    IEditor *editor = EditorManager::currentEditor();
    if (!editor)
        return -1;
    return EditorManager::documentModel()->indexOfDocument(editor->document());
}

void EmacsModePluginPrivate::switchToFile(int n)
{
    int size = EditorManager::documentModel()->documentCount();
    QTC_ASSERT(size, return);
    n = n % size;
    if (n < 0)
        n += size;
    EditorManager::activateEditorForEntry(EditorManager::documentModel()->documents().at(n));
}

void EmacsModePluginPrivate::resetCommandBuffer()
{
  showCommandBuffer(_(""), 0);
}

void EmacsModePluginPrivate::showCommandBuffer(QString const& contents, int messageLevel)
{
  if (MiniBuffer *w = qobject_cast<MiniBuffer *>(m_statusBar->widget()))
      w->setContents(contents, messageLevel);
}


///////////////////////////////////////////////////////////////////////
//
// EmacsModePlugin
//
///////////////////////////////////////////////////////////////////////

EmacsModePlugin::EmacsModePlugin()
    : d(new EmacsModePluginPrivate(this))
{}

EmacsModePlugin::~EmacsModePlugin()
{
    delete d;
}

bool EmacsModePlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)
    return d->initialize();
}

ExtensionSystem::IPlugin::ShutdownFlag EmacsModePlugin::aboutToShutdown()
{
    d->aboutToShutdown();
    return SynchronousShutdown;
}

void EmacsModePlugin::extensionsInitialized()
{
    d->m_statusBar = new StatusBarWidget;
    d->m_statusBar->setWidget(new MiniBuffer);
    d->m_statusBar->setPosition(StatusBarWidget::LastLeftAligned);
    addAutoReleasedObject(d->m_statusBar);
}

} // namespace Internal
} // namespace EmacsMode

#include "emacsmodeplugin.moc"

Q_EXPORT_PLUGIN(EmacsMode::Internal::EmacsModePlugin)
