#include "killring.hpp"

KillRing::KillRing(const unsigned maxSize)
    : maxSize_(maxSize)
{}

void KillRing::push(QString line) {
  killRing_.push_front(std::move(line));
}

void KillRing::appendTop(QString line) {
  if (killRing_.empty())
      push("");

  killRing_.front().append(std::move(line));
}

QString const& KillRing::current() const {
  return killRing_.at(pos_);
}

void KillRing::advance() {
  pos_ += 1;
  if (pos_ >= killRing_.size()) {
      pos_ = 0;
  }
}

void KillRing::clear() {
  killRing_.clear();
}


