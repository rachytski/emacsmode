#pragma once

#include <QStringList>
#include <QString>

#include "killring.hpp"

namespace EmacsMode {
namespace Internal {

// message levels sorted by severity
enum MessageLevel
{
  MessageInfo,    // result of a command
  MessageWarning, // warning
  MessageError,   // error
  MessageShowCmd  // partial command
};

// Data shared among all editors.
struct PluginState
{
  PluginState()
    : currentMessageLevel(MessageInfo)
  {
  }

  // Current mini buffer message.
  QString currentMessage;
  MessageLevel currentMessageLevel;
  QString currentCommand;

  KillRing killRing_;
};

}
}
