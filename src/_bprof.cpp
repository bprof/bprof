#include "_bprof.h"

std::string PyCode_GetName(PyCodeObject* code) {
  Py_ssize_t size;
  const char* method_name_char = PyUnicode_AsUTF8AndSize(code->co_name, &size);
  return std::string(method_name_char, size);
}
std::string PyFrame_GetName(PyFrameObject* frame) {
  return PyCode_GetName(frame->f_code);
}

static int profile_func(PyObject*, PyFrameObject*, int, PyObject*);
static int trace_func(PyObject*, PyFrameObject*, int, PyObject*);

Module::Module(PyObject* m) : parent_(m) {
  inspect_ = PyImport_ImportModule("inspect");
  if (inspect_ == NULL) {
    throw std::runtime_error("Could not import `inspect'");
  }
}

Module::~Module() {
  Py_XDECREF(inspect_);
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

PyObject* CreateFunctionDict(const BaseFunction& function) {
  PyObject* function_py = PyDict_New();
  PyObject* n_calls = PyLong_FromUnsignedLongLong(function.n_calls());
  const auto& name = function.name();
  PyObject* name_py = PyUnicode_DecodeUTF8(name.data(), name.size(), NULL);
  PyObject* internal = PyLong_FromUnsignedLongLong(function.overhead().count());
  
  PyDict_SetItemString(function_py, "name", name_py);
  Py_DECREF(name_py);
  PyDict_SetItemString(function_py, "n_calls", n_calls);
  Py_DECREF(n_calls);
  PyDict_SetItemString(function_py, "internal_ns", internal);
  Py_DECREF(internal);

  return function_py;
}

PyObject* Module::dump(const char* path) {
  PyObject* functions = PyDict_New();
  for (auto&& function_pair : functions_) {
    Function& function = function_pair.second;
    PyObject* function_py = CreateFunctionDict(function);

    const auto& lines = function.lines();
    PyObject* lines_py = PyList_New(lines.size());
    size_t j = 0;
    for (auto&& line : lines) {
      const auto& line_str = line.text();
      PyObject* line_dict = PyDict_New();
      PyObject* line_str_py = 
	PyUnicode_DecodeUTF8(line_str.data(), line_str.size(), NULL);
      PyObject* line_n_calls = PyLong_FromUnsignedLongLong(line.n_calls());
      PyObject* line_internal =
	PyLong_FromUnsignedLongLong(line.internal().count());
      PyObject* line_external =
	PyLong_FromUnsignedLongLong(line.external().count());

      PyDict_SetItemString(line_dict, "line_str", line_str_py);
      Py_DECREF(line_str_py);
      PyDict_SetItemString(line_dict, "n_calls", line_n_calls);
      Py_DECREF(line_n_calls);
      PyDict_SetItemString(line_dict, "internal_ns", line_internal);
      Py_DECREF(line_internal);
      PyDict_SetItemString(line_dict, "external_ns", line_external);
      Py_DECREF(line_external);

      PyList_SET_ITEM(lines_py, j++, line_dict);
    }
    PyDict_SetItemString(function_py, "lines", lines_py);
    Py_DECREF(lines_py);

    PyObject* key = PyLong_FromUnsignedLongLong(
	reinterpret_cast<size_t>(function_pair.first));
    PyDict_SetItem(functions, key, function_py);
    Py_DECREF(key);
    Py_DECREF(function_py);
  }

  PyObject* c_functions = PyDict_New();
  for (auto&& function_pair : c_functions_) {
    PyObject* function_py = CreateFunctionDict(function_pair.second);
    auto& name_str = function_pair.first;
    PyObject* name = PyUnicode_DecodeUTF8(name_str.data(), name_str.size(), NULL);
    PyDict_SetItem(c_functions, name, function_py);
    Py_DECREF(name);
    Py_DECREF(function_py);
  }

  PyObject* result = PyDict_New();
  PyDict_SetItemString(result, "functions", functions);
  Py_DECREF(functions);
  PyDict_SetItemString(result, "c_functions", c_functions);
  Py_DECREF(c_functions);

  return result;
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
  auto& function = add_function(frame);
  function.add_call();
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
  auto& line = frame_stack_.top().set_current_line(line_number);
  line.add_call();
}

void Module::profile_c_call(PyFrameObject* frame, PyObject* arg) {
  PyObject* module = PyObject_GetAttrString(arg, "__module__");
  PyObject* qualname = PyObject_GetAttrString(arg, "__qualname__");
  PyObject* name = PyUnicode_FromFormat("<C-function %U.%U>", module, qualname);
  if (name == NULL) {
    throw std::runtime_error("Could not get C call name");
  }

  Py_ssize_t size;
  const char* name_char = PyUnicode_AsUTF8AndSize(name, &size);
  last_c_name_ = std::string(name_char, size);
  auto& c_function = add_c_function(last_c_name_);
  c_function.add_call();
  last_instruction_ = Instruction::kCCall;

  Py_DECREF(name);
  Py_DECREF(qualname);
  Py_DECREF(module);
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
    LineRecord& line_record = function.line(i++);
    line_record += line;
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
  if (method_name_py == NULL) {
    throw std::runtime_error("Could not create str");
  }

  PyObject* result = PyObject_CallMethodObjArgs(inspect_, method_name_py, (PyObject*)code, NULL);
  if (result == NULL) {
    Py_DECREF(method_name_py);
    throw std::runtime_error("Could not get `inspect.getsourcelines' method");
  }
  PyObject* lines_py = PyTuple_GetItem(result, 0);

  if (line_start != nullptr) {
    PyObject* line_start_py = PyTuple_GetItem(result, 1);
    *line_start = PyLong_AsUnsignedLongLong(line_start_py);
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
