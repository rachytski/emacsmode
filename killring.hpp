#pragma once

#include <QStringList>

class KillRing
{
private:
  QStringList killRing_;
  unsigned pos_ = 0;
  const unsigned maxSize_;
public:
  KillRing(const unsigned maxSize = 60);
  void push(QString line);
  void appendTop(QString line);
  QString const& current() const;
  void advance();
  void clear();
};

