#pragma once

#include <Python.h>

#include "common.h"

#include <chrono>
#include <set>
#include <string>

class LineState {
 public:
  void add_internal(const duration& dur) { internal_ += dur; }
  void add_external(const duration& dur) { external_ += dur; }

  const duration& internal() const { return internal_; }
  const duration& external() const { return external_; }

  void add_call() { ++n_calls_; }
  size_t n_calls() const { return n_calls_; }

  void add_function_key(PyCodeObject* key) { function_keys_.emplace(key); }
  // void add_c_function_key(std::string key) { c_function_keys_.emplace(std::move(key)); }

  LineState& operator+=(const LineState& rhs) {
    n_calls_ += rhs.n_calls_;
    internal_ += rhs.internal_;
    external_ += rhs.external_;
    return *this;
  }

 private:
  size_t n_calls_ = 0;
  duration internal_ = duration(0);
  duration external_ = duration(0);
  std::set<PyCodeObject*> function_keys_;
  // std::set<std::string> c_function_keys_;
};

class LineRecord : public LineState {
 public:
  LineRecord(std::string line) : line_(std::move(line)) {}
  LineRecord(LineState state, std::string line)
      : LineState(std::move(state)), line_(std::move(line)) {}

  const std::string& text() const { return line_; }

 private:
  std::string line_;
};

