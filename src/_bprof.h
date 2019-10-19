#pragma once

#include <Python.h>
#include <frameobject.h>

#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>

#include "function.h"
#include "frame.h"


enum class Instruction {
  kOrigin,
  kLine,
  kCall,
  kReturn,
  kException,
  kCCall,
  kCReturn,
  kCException,
  kInvalid,
};

class Module {
 public:
  using clock = std::chrono::high_resolution_clock;
  using Time = clock::rep;
  using time_point = clock::time_point;
  Module(PyObject*);
  ~Module();
  void start();
  void stop();
  PyObject* dump(const char*);

  void profile(int what, PyFrameObject* frame, PyObject* arg);
  void profile_call(PyFrameObject*);
  void profile_return(PyFrameObject*);
  void profile_c_call(PyFrameObject*, PyObject*);
  void profile_c_return(PyFrameObject*);
  void profile_line(PyFrameObject*);

  void finish_origin(PyFrameObject*);
  void finish_line(PyFrameObject*);
  void finish_call(PyFrameObject*);
  void finish_return(PyFrameObject*);
  void finish_exception(PyFrameObject*);
  void finish_ccall(PyFrameObject*);
  void finish_creturn(PyFrameObject*);
  void finish_cexception(PyFrameObject*);

  void emplace_frame(PyFrameObject*);
  void pop_frame();

  Function& add_function(PyFrameObject*);
  BaseFunction& add_c_function(std::string);

  duration elapsed();
  const auto& functions() const { return functions_; }
  const auto& c_functions() const { return c_functions_; }

 private:
  std::vector<std::string> get_lines(
      PyFrameObject* lines, size_t* line_start=nullptr);

  PyObject* parent_;
  std::unordered_map<PyCodeObject*, Function> functions_;
  std::unordered_map<std::string, BaseFunction> c_functions_;
  std::stack<FrameState> frame_stack_;
  PyObject* inspect_;
  Instruction last_instruction_ = Instruction::kInvalid;
  time_point last_instruction_start_;
  time_point last_instruction_end_;
  std::string last_c_name_;
};
