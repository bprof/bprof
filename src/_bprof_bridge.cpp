#include <Python.h>

#include "_bprof.h"

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

  return mod->dump(bytes_data);
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
  if (m == NULL) {
    return;
  }
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
