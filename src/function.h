#pragma once

#include <Python.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "line.h"

class BaseFunction {
 public:
  BaseFunction(std::string name) : name_(std::move(name)) {}

  const std::string& name() const { return name_; }
  void add_elapsed_internal(const std::chrono::nanoseconds& time);
  const auto& overhead() const { return internal_time_; }

  void add_call() { ++n_calls_; }
  size_t n_calls() const { return n_calls_; }

 private:
  size_t n_calls_ = 0;
  std::string name_;
  std::chrono::nanoseconds internal_time_ = duration(0);
};

class Function : public BaseFunction {
 public:
  Function(std::string name, std::vector<std::string> lines, PyCodeObject*);

  PyCodeObject* const code() const noexcept { return code_; }
  size_t n_lines() const { return lines_.size(); }
  const auto& lines() const { return lines_; }

  LineRecord& line(size_t line_number) { return lines_.at(line_number); }

 private:
  PyCodeObject* code_;
  std::vector<LineRecord> lines_;
};
