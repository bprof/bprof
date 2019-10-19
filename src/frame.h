#pragma once

#include <vector>

#include "common.h"
#include "line.h"

class FrameState {
 public:
  FrameState(PyCodeObject* code, size_t n_lines, size_t starting_line) 
      : starting_line_(starting_line), function_key_(code) {
    lines_.resize(n_lines);
  }
  PyCodeObject* key() const { return function_key_; }

  duration total_time() const;
  LineState& current_line() { return lines_.at(current_line_-starting_line_-1); }
  LineState& set_current_line(size_t line_number) {
    current_line_ = line_number;
    return current_line();
  }

  void add_internal(const duration& dur) { internal_ += dur; }
  const duration& internal() const { return internal_; }

  const auto& lines() const { return lines_; }

 private:
  size_t starting_line_ = 0;
  size_t current_line_ = 0;
  PyCodeObject* function_key_;
  std::vector<LineState> lines_;
  duration internal_ = duration(0);
};
