#include "action.hpp"

namespace EmacsMode {



Action::Action(Id id, std::function<void()> fn)
    : id_(id), fn_(std::move(fn))
{
}

void Action::exec() const {
  fn_();
}

Action::Id Action::id() const {
  return id_;
}

}
