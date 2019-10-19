#include "frame.h"

duration FrameState::total_time() const {
  auto result = duration(0);
  for (auto&& line : lines_) {
    result += line.internal();
    result += line.external();
  }
  return result;
}
