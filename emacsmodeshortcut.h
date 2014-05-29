#ifndef EMACS_SHORTCUT_H
#define EMACS_SHORTCUT_H

#include <functional>
#include <list>
#include <vector>
#include <Qt>
#include <QKeyEvent>

namespace EmacsMode
{
  class Shortcut
  {
  private:

    Qt::KeyboardModifiers m_mods;
    std::vector<int> m_keys;

    typedef std::list<std::function<void()> > TFnList;
    TFnList m_fnList;

  public:

    Shortcut();
    Shortcut(char const * s);
    Shortcut(Qt::KeyboardModifiers, std::vector<int> const & , TFnList const & );

    Shortcut & addFn(std::function<void()> fn);

    void exec() const;

    bool isEmpty() const;
    bool isAccepted(QKeyEvent * kev) const;
    bool hasFollower(QKeyEvent * kev) const;
    Shortcut const getFollower(QKeyEvent * kev) const;
  };
}

#endif // EMACS_SHORTCUT_H
