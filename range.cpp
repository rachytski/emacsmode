#include "range.hpp"

namespace EmacsMode {
namespace Internal {

Range::Range()
  : beginPos_(-1), endPos_(-1), rangemode_(RangeCharMode)
{}

Range::Range(int b, int e, RangeMode m)
  : beginPos_(qMin(b, e)), endPos_(qMax(b, e)), rangemode_(m)
{}

QString Range::toString() const
{
  return QString::fromLatin1("%1-%2 (mode: %3)").arg(beginPos_).arg(endPos_)
      .arg(rangemode_);
}

bool Range::isValid() const
{
  return beginPos_ >= 0 && endPos_ >= 0;
}

QDebug operator<<(QDebug ts, const Range &range)
{
  return ts << '[' << range.beginPos_ << ',' << range.endPos_ << ']';
}

}}
