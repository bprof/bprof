#include <Python.h>
#include <frameobject.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <map>

using duration = std::chrono::nanoseconds;

std::string PyFrame_GetName(PyCodeObject* code) {
  const char* method_name_char = PyUnicode_AsUTF8AndSize(code->co_name, NULL);
  return std::string(method_name_char);
}
std::string PyFrame_GetName(PyFrameObject* frame) {
  return PyFrame_GetName(frame->f_code);
}

static int profile_func(PyObject*, PyFrameObject*, int, PyObject*);
static int trace_func(PyObject*, PyFrameObject*, int, PyObject*);

class BaseFunction {
 public:
  BaseFunction(std::string name) : name_(std::move(name)) {}

  const std::string& name() const { return name_; }
  void add_elapsed_internal(const std::chrono::nanoseconds& time);
  const auto& overhead() const { return internal_time_; }

 private:
  std::string name_;
  std::chrono::nanoseconds internal_time_ = duration(0);
};

class Function : public BaseFunction {
 public:
  Function(std::string name, std::vector<std::string> lines, PyCodeObject*);

  PyCodeObject* const code() const noexcept { return code_; }
  size_t n_lines() const { return lines_.size(); }
  const auto& lines() const { return lines_; }

  void add_elapsed(size_t line_number, const std::chrono::nanoseconds& time);
  void add_elapsed_internal(size_t line_number, const std::chrono::nanoseconds& time);
  using BaseFunction::add_elapsed_internal;

  const auto& line_time() const { return line_time_; }
  const auto& line_time_internal() const { return line_time_internal_; }

 private:
  PyCodeObject* code_;
  std::vector<std::string> lines_;
  std::vector<std::chrono::nanoseconds> line_time_;
  std::vector<std::chrono::nanoseconds> line_time_internal_;
};

void Function::add_elapsed(size_t line_number, const std::chrono::nanoseconds& time) {
  line_time_.at(line_number) += time;
}

void Function::add_elapsed_internal(size_t line_number, const std::chrono::nanoseconds& time) {
  line_time_internal_.at(line_number) += time;
}

void BaseFunction::add_elapsed_internal(const duration& time) {
  internal_time_ += time;
  std::cout << "Internal: " << internal_time_.count()/1e9 << std::endl;
}


template<> struct std::hash<Function> {
  using argument_type = Function;
  using result_type = std::size_t;

  result_type operator()(argument_type const& function) const noexcept {
    return reinterpret_cast<result_type>(function.code());
  }
};

Function::Function(
    std::string name, std::vector<std::string> lines, PyCodeObject* code)
      : BaseFunction(std::move(name)), code_(code), lines_(std::move(lines)) {
  auto n_lines = lines_.size();
  line_time_.resize(n_lines);
  line_time_internal_.resize(n_lines);
}

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

class LineState {
 public:
  void add_internal(const duration& dur) { internal_ += dur; }
  void add_external(const duration& dur) { external_ += dur; }

  const duration& internal() const { return internal_; }
  const duration& external() const { return external_; }

 private:
  duration internal_;
  duration external_;
};

class FrameState {
 public:
  FrameState(PyCodeObject* code, size_t n_lines, size_t starting_line) 
      : starting_line_(starting_line), function_key_(code) {
    lines_.resize(n_lines);
  }
  PyCodeObject* key() const { return function_key_; }

  duration total_time() const;
  LineState& current_line() { return lines_.at(current_line_-starting_line_-1); }
  void set_current_line(size_t current_line) { current_line_ = current_line; }

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

duration FrameState::total_time() const {
  auto result = duration(0);
  for (auto&& line : lines_) {
    result += line.internal();
    result += line.external();
  }
  return result;
}

class Module {
 public:
  using clock = std::chrono::high_resolution_clock;
  using Time = clock::rep;
  using time_point = clock::time_point;
  Module(PyObject*);
  void start();
  void stop();
  unsigned long long dump(const char*);

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

Module::Module(PyObject* m) : parent_(m) {
  inspect_ = PyImport_ImportModule("inspect");
}

duration Module::elapsed() {
  return std::chrono::duration_cast<duration>(
      last_instruction_end_ - last_instruction_start_);
}

void Module::emplace_frame(PyFrameObject* frame) {
  size_t starting_line = 0;
  auto lines = get_lines(frame, &starting_line);
  frame_stack_.emplace(frame->f_code, lines.size(), starting_line);
}

void Module::start() {
  last_instruction_ = Instruction::kOrigin;

  PyEval_SetProfile(profile_func, parent_);
  PyEval_SetTrace(trace_func, parent_);
}

void Module::finish_origin(PyFrameObject* frame) {
}

void Module::stop() {
  PyEval_SetProfile(NULL, NULL);
  PyEval_SetTrace(NULL, NULL);
}

unsigned long long Module::dump(const char* path) {
  for (auto&& pair : functions_) {
    Function& function = pair.second;
    std::cout << "Name: " << function.name() << ", "
      << function.overhead().count()/1e9 << std::endl;
    auto&& lines = function.lines();
    for (size_t i=0; i < lines.size(); ++i) {
      auto&& external = function.line_time().at(i);
      auto&& internal = function.line_time_internal().at(i);
      auto&& line = lines.at(i);
      auto total = external + internal;
      std::cout << total.count()/1e9 << '(' << internal.count()/1e9 << '/'
	<< external.count()/1e9
	<< ')' << ": " << line;
    }
  }

  for (auto&& pair : c_functions_) {
    BaseFunction& function = pair.second;
    std::cout << "Name: " << function.name() << ", " <<
      function.overhead().count()/1e9 << std::endl;
  }

  return 0;
}

void Module::profile(int what, PyFrameObject* frame, PyObject* arg) {
  last_instruction_end_ = clock::now();

  switch (last_instruction_) {
    case Instruction::kOrigin:
      finish_origin(frame);
      break;
    case Instruction::kLine:
      finish_line(frame);
      break;
    case Instruction::kCall:
      finish_call(frame);
      break;
    case Instruction::kReturn:
      finish_return(frame);
      break;
    case Instruction::kException:
      break;
    case Instruction::kCCall:
      finish_ccall(frame);
      break;
    case Instruction::kCReturn:
      finish_creturn(frame);
      break;
    case Instruction::kCException:
      break;
    case Instruction::kInvalid:
      break;
    default:
      throw std::runtime_error("Should not get here");
  }

  switch (what) {
    case PyTrace_LINE:
      profile_line(frame);
      break;
    case PyTrace_CALL:
      profile_call(frame);
      break;
    case PyTrace_RETURN:
      profile_return(frame);
      break;
    case PyTrace_C_CALL:
      profile_c_call(frame, arg);
      break;
    case PyTrace_C_RETURN:
      profile_c_return(frame);
      break;
    case PyTrace_EXCEPTION:
      break;
    case PyTrace_C_EXCEPTION:
      profile_c_return(frame);
      break;
    case PyTrace_OPCODE:
      break;
    default:
      throw std::runtime_error("Should not get here");
  }
  last_instruction_start_ = clock::now();
}

void Module::profile_call(PyFrameObject* frame) {
  add_function(frame);
  frame->f_trace_opcodes = 0;
  emplace_frame(frame);
  last_instruction_ = Instruction::kCall;
}

void Module::finish_call(PyFrameObject* frame) {
  functions_.at(frame->f_code).add_elapsed_internal(elapsed());
}

void Module::profile_line(PyFrameObject* frame) {
  last_instruction_ = Instruction::kLine;

  if (frame_stack_.empty()) {
    return;
  }

  auto line_number = PyFrame_GetLineNumber(frame);
  frame_stack_.top().set_current_line(line_number);
}

void Module::profile_c_call(PyFrameObject* frame, PyObject* arg) {
  PyObject* name = PyObject_Str(arg);
  Py_ssize_t size;
  const char* name_char = PyUnicode_AsUTF8AndSize(name, &size);
  last_c_name_ = std::string(name_char, size);
  add_c_function(last_c_name_);
  last_instruction_ = Instruction::kCCall;
}

void Module::finish_ccall(PyFrameObject* frame) {
  c_functions_.at(last_c_name_).add_elapsed_internal(elapsed());
  frame_stack_.top().current_line().add_external(elapsed());
}

void Module::finish_line(PyFrameObject* frame) {
  if (frame_stack_.empty()) {
    return;
  }
  frame_stack_.top().current_line().add_internal(elapsed());
}

void Module::profile_return(PyFrameObject* frame) {
  last_instruction_ = Instruction::kReturn;
}

void Module::finish_return(PyFrameObject*) {
  frame_stack_.top().add_internal(elapsed());
  pop_frame();
}

void Module::profile_c_return(PyFrameObject* frame) {
  last_instruction_ = Instruction::kCReturn;
}

void Module::finish_creturn(PyFrameObject*) {
  frame_stack_.top().add_internal(elapsed());
}

void Module::pop_frame() {
  FrameState& frame = frame_stack_.top();
  Function& function = functions_.at(frame.key());
  function.add_elapsed_internal(frame.internal());
  size_t i = 0;
  for (auto&& line : frame.lines()) {
    function.add_elapsed(i, line.external());
    function.add_elapsed_internal(i, line.internal());
  }
  auto total = frame.total_time();

  frame_stack_.pop();

  if (!frame_stack_.empty()) {
    frame_stack_.top().current_line().add_external(total);
  }
}

std::vector<std::string> Module::get_lines(
    PyFrameObject* frame, size_t* line_start) {
  PyCodeObject* code = frame->f_code;
  PyObject* method_name_py = PyUnicode_FromString("getsourcelines");

  PyObject* result = PyObject_CallMethodObjArgs(inspect_, method_name_py, (PyObject*)code, NULL);
  PyObject* lines_py = PyTuple_GetItem(result, 0);

  if (line_start != nullptr) {
    PyObject* line_start_py = PyTuple_GetItem(result, 1);
    *line_start = PyLong_AsUnsignedLongLong(line_start_py);
    Py_XDECREF(line_start_py);
  }

  auto n_lines = PyList_Size(lines_py);
  std::vector<std::string> lines;
  lines.reserve(n_lines);
  for (decltype(n_lines) i=1; i < n_lines; ++i) {
    PyObject* line_py = PyList_GetItem(lines_py, i);
    Py_ssize_t size;
    const char* line_char = PyUnicode_AsUTF8AndSize(line_py, &size);
    lines.emplace_back(std::string(line_char, size));
  }
  Py_DECREF(result);
  Py_DECREF(method_name_py);
  return lines;
}

Function& Module::add_function(PyFrameObject* frame) {
  PyCodeObject* code = frame->f_code;
  if (functions_.count(code) != 0) {
    return functions_.at(code);
  }
  auto lines = get_lines(frame);
  auto pair = 
    functions_.emplace(
	code, Function(PyFrame_GetName(frame), std::move(lines), code));
  return pair.first->second;
}

BaseFunction& Module::add_c_function(std::string name) {
  auto pair = c_functions_.emplace(name, BaseFunction(name));
  return pair.first->second;
}

static int
profile_func(PyObject* obj, PyFrameObject* frame, int what, PyObject *arg) {
  Module* mod = (Module*)PyModule_GetState(obj);
  mod->profile(what, frame, arg);
  return 0;
}

static int
trace_func(PyObject* obj, PyFrameObject* frame, int what, PyObject *arg) {
  // We are uninterested in these events
  if (what != PyTrace_LINE) {
    return 0;
  }
  return profile_func(obj, frame, what, arg);
}

static int
module_exec(PyObject *m)
{
  new(PyModule_GetState(m)) Module(m);
  return 0;
}

static PyObject*
module_start(PyObject* m, PyObject*) {
  Module* mod = (Module*)PyModule_GetState(m);
  mod->start();
  Py_RETURN_NONE;
}

static PyObject*
module_stop(PyObject* m, PyObject*) {
  Module* mod = (Module*)PyModule_GetState(m);
  mod->stop();
  Py_RETURN_NONE;
}

static PyObject*
module_dump(PyObject* m, PyObject* path) {
  Module* mod = (Module*)PyModule_GetState(m);

  PyObject* bytes;
  if (!PyArg_ParseTuple(path, "O&", PyUnicode_FSConverter, &bytes)) {
    return NULL;
  }

  char* bytes_data = PyBytes_AsString(bytes);
  if (bytes_data == NULL) {
    return NULL;
  }

  return PyLong_FromUnsignedLongLong(mod->dump(bytes_data));
}

PyDoc_STRVAR(module_doc,
"This is the C++ implementation.");

static PyMethodDef module_methods[] = {
    {"start", module_start, METH_NOARGS,
        PyDoc_STR("start() -> None")},
    {"stop", module_stop, METH_NOARGS,
        PyDoc_STR("stop() -> None")},
    {"dump", module_dump, METH_VARARGS,
        PyDoc_STR("dump() -> None")},
    {NULL,              NULL}           /* sentinel */
};

static void module_free(void* m) {
  Module* mod = (Module*)PyModule_GetState((PyObject*)m);
  if (mod != NULL) {
    mod->~Module();
  }
}

static struct PyModuleDef_Slot module_slots[] = {
  {Py_mod_exec, (void*)module_exec},
  {0, NULL},
};

static struct PyModuleDef module = {
  PyModuleDef_HEAD_INIT,
  "_bprof",
  module_doc,
  sizeof(Module),
  module_methods,
  module_slots,
  NULL,
  NULL,
  module_free
};

PyMODINIT_FUNC
PyInit__bprof(void)
{
  return PyModuleDef_Init(&module);
}
