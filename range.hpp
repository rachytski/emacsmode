#pragma once

#include <QString>
#include <QDebug>

namespace EmacsMode {
namespace Internal {

enum RangeMode
{
  // Reordering first three enum items here will break
  // compatibility with clipboard format stored by Vim.
  RangeCharMode,
  RangeLineMode,
  RangeBlockMode,
  RangeLineModeExclusive,
  RangeBlockAndTailMode // Ctrl-v for D and X
};

struct Range
{
  Range();
  Range(int b, int e, RangeMode m = RangeCharMode);
  QString toString() const;
  bool isValid() const;

  int beginPos;
  int endPos;
  RangeMode rangemode;
};

QDebug operator<<(QDebug ts, const Range &range);

}}