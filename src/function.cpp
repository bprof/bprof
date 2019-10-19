#include "function.h"

void BaseFunction::add_elapsed_internal(const duration& time) {
  internal_time_ += time;
}

Function::Function(
    std::string name, std::vector<std::string> lines, PyCodeObject* code)
      : BaseFunction(std::move(name)), code_(code) {
  auto n_lines = lines_.size();
  lines_.reserve(n_lines);
  for (auto&& line : lines) {
    lines_.emplace_back(line);
  }
}
