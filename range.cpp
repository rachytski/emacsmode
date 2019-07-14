#include "range.hpp"

namespace EmacsMode {
namespace Internal {

Range::Range()
  : beginPos(-1), endPos(-1), rangemode(RangeCharMode)
{}

Range::Range(int b, int e, RangeMode m)
  : beginPos(qMin(b, e)), endPos(qMax(b, e)), rangemode(m)
{}

QString Range::toString() const
{
  return QString::fromLatin1("%1-%2 (mode: %3)").arg(beginPos).arg(endPos)
      .arg(rangemode);
}

bool Range::isValid() const
{
  return beginPos >= 0 && endPos >= 0;
}

QDebug operator<<(QDebug ts, const Range &range)
{
  return ts << '[' << range.beginPos << ',' << range.endPos << ']';
}

}}