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

#ifndef EMACSMODEPLUGIN_H
#define EMACSMODEPLUGIN_H

#include <coreplugin/dialogs/ioptionspage.h>
#include <extensionsystem/iplugin.h>

namespace EmacsMode {
namespace Internal {

class EmacsModeHandler;
class EmacsModePluginPrivate;

class EmacsModePlugin : public ExtensionSystem::IPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "EmacsMode.json")

public:
  EmacsModePlugin();
  ~EmacsModePlugin();

private:
  // implementation of ExtensionSystem::IPlugin
  bool initialize(const QStringList &arguments, QString *errorMessage);
  ShutdownFlag aboutToShutdown();
  void extensionsInitialized();

private:
  friend class EmacsModePluginPrivate;
  EmacsModePluginPrivate *d;
};

} // namespace Internal
} // namespace EmacsMode

#endif // EMACSMODEPLUGIN_H
