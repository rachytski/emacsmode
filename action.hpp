#pragma once

#include <functional>

namespace EmacsMode {

class Action
{
public:
  enum class Id {
    Null,
    MoveUp,
    MoveDown,
    MoveRight,
    MoveLeft,
    NewLine,
    Backspace,
    MoveToEndOfLine,
    MoveToStartOfLine,
    Undo,
    IndentRegion,
    StartSelection,
    CancelCurrentCommand,
    KillSelected,
    CopySelected,
    InsertBackSlash,
    InsertStraightDelim,
    KillLine,
    KillSymbol,
    YankCurrent,
    YankNext,
    SaveCurrentBuffer,
    CommentOutRegion,
    UncommentRegion
  };

private:
  Id id_ = Id::Null;
  std::function<void()> fn_;
public:
  Action() = default;
  Action(Id type, std::function<void()> fn);
  void exec() const;
  Id id() const;
};

}
