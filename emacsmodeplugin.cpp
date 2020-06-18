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

#include "minibuffer.hpp"
#include "emacsmodesettings.h"
#include "emacsmodehandler.h"
#include "emacsmodeoptionpage.h"
#include "ui_emacsmodeoptions.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/find/findplugin.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/id.h>
#include <coreplugin/statusbarmanager.h>

#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>
#include <texteditor/tabsettings.h>
#include <texteditor/indenter.h>

#include <utils/qtcassert.h>

#include <extensionsystem/pluginmanager.h>

#include <QDebug>
#include <QObject>

using namespace TextEditor;
using namespace Core;

namespace EmacsMode {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// EmacsModePluginPrivate
//
///////////////////////////////////////////////////////////////////////

class EmacsModePluginRunData;

class EmacsModePluginPrivate : public QObject
{
  Q_OBJECT

public:
  EmacsModePluginPrivate();
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

  void resetCommandBuffer();
  void showCommandBuffer(const QString &contents, int messageLevel);

  void indentRegion(int beginBlock, int endBlock, QChar typedChar);

  void writeSettings();
  void readSettings();

private:
  EmacsModePlugin *q;
  QHash<IEditor *, EmacsModeHandler *> m_editorToHandler;

  MiniBuffer *m_miniBuffer = nullptr;
  EmacsModePluginRunData *m_runData = nullptr;
};

class EmacsModePluginRunData
{
public:
    EmacsModeOptionPage optionsPage;
};

EmacsModePluginPrivate::EmacsModePluginPrivate()
{
  m_runData = new EmacsModePluginRunData();
}

EmacsModePluginPrivate::~EmacsModePluginPrivate()
{
}

void EmacsModePluginPrivate::onCoreAboutToClose()
{
  // Don't attach to editors anymore.
  disconnect(EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
             this, SLOT(editorOpened(Core::IEditor*)));
}

void EmacsModePluginPrivate::aboutToShutdown()
{
    delete m_runData;
    m_runData = nullptr;

    StatusBarManager::destroyStatusBarWidget(m_miniBuffer);
    m_miniBuffer = nullptr;
}

bool EmacsModePluginPrivate::initialize()
{
  m_runData = new EmacsModePluginRunData;
  Context globalcontext(Core::Constants::C_GLOBAL);

  readSettings();

  connect(ICore::instance(), SIGNAL(coreAboutToClose()), this, SLOT(onCoreAboutToClose()));

  // EditorManager
  connect(EditorManager::instance(), SIGNAL(editorAboutToClose(Core::IEditor*)),
          this, SLOT(editorAboutToClose(Core::IEditor*)));
  connect(EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
          this, SLOT(editorOpened(Core::IEditor*)));

  connect(theEmacsModeSetting(ConfigUseEmacsMode), SIGNAL(valueChanged(QVariant)),
          this, SLOT(setUseEmacsMode(QVariant)));

  return true;
}

void EmacsModePluginPrivate::writeSettings()
{
  QSettings *settings = ICore::settings();
  theEmacsModeSettings()->writeSettings(settings);
}

void EmacsModePluginPrivate::readSettings()
{
  QSettings *settings = ICore::settings();
  theEmacsModeSettings()->readSettings(settings);
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
           << "EMACSMODE: " << theEmacsModeSetting(ConfigUseEmacsMode)->value();

  EmacsModeHandler *handler = new EmacsModeHandler(widget, 0);
  // the handler might have triggered the deletion of the editor:
  // make sure that it can return before being deleted itself
  new DeferredDeleter(widget, handler);
  m_editorToHandler[editor] = handler;

  connect(handler, SIGNAL(commandBufferChanged(QString,int)),
          SLOT(showCommandBuffer(QString, int)));
  connect(handler, SIGNAL(indentRegionRequested(int,int,QChar)),
          SLOT(indentRegion(int,int,QChar)));

  connect(ICore::instance(), SIGNAL(saveSettingsRequested()),
          SLOT(writeSettings()));

  handler->setCurrentFileName(editor->document()->filePath().toString());
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
    foreach (IEditor *editor, m_editorToHandler.keys()) {
      if (TextDocument *textDocument =
          qobject_cast<TextDocument *>(editor->document())) {
        m_editorToHandler[editor]->restoreWidget(textDocument->tabSettings().m_tabSize);
      }
    }
  }
}

void EmacsModePluginPrivate::indentRegion(int beginBlock, int endBlock,
                                          QChar typedChar)
{
  EmacsModeHandler *handler = qobject_cast<EmacsModeHandler *>(sender());
  if (!handler)
    return;

  TextEditorWidget *bt = qobject_cast<TextEditorWidget *>(handler->widget());
  if (!bt)
    return;

  TabSettings tabSettings;
  tabSettings.m_indentSize = theEmacsModeSetting(ConfigShiftWidth)->value().toInt();
  tabSettings.m_tabSize = theEmacsModeSetting(ConfigTabStop)->value().toInt();
  tabSettings.m_tabPolicy = theEmacsModeSetting(ConfigExpandTab)->value().toBool()
      ? TabSettings::SpacesOnlyTabPolicy : TabSettings::TabsOnlyTabPolicy;

  QTextDocument *doc = bt->document();
  QTextBlock startBlock = doc->findBlockByNumber(beginBlock - 1);

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
      bt->textDocument()->indenter()->indentBlock(block, typedChar, tabSettings);
    }
    block = block.next();
  }
}

void EmacsModePluginPrivate::quitEmacsMode()
{
  theEmacsModeSetting(ConfigUseEmacsMode)->setValue(false);
}

void EmacsModePluginPrivate::resetCommandBuffer()
{
  showCommandBuffer(_(""), 0);
}

void EmacsModePluginPrivate::showCommandBuffer(QString const& contents, int messageLevel)
{
  QTC_ASSERT(m_miniBuffer, return);
  m_miniBuffer->setContents(contents, messageLevel);
}


///////////////////////////////////////////////////////////////////////
//
// EmacsModePlugin
//
///////////////////////////////////////////////////////////////////////

EmacsModePlugin::EmacsModePlugin()
  : d(new EmacsModePluginPrivate())
{}

EmacsModePlugin::~EmacsModePlugin()
{
  delete d;
  d = nullptr;
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
  d->m_miniBuffer = new MiniBuffer;
  StatusBarManager::addStatusBarWidget(d->m_miniBuffer, StatusBarManager::LastLeftAligned);
}

} // namespace Internal
} // namespace EmacsMode

#include "emacsmodeplugin.moc"
