/*!
 * \file helpers.h
 * \brief Helpers for Cython wrappers
 */

#ifndef __HELPERS_H__
#define __HELPERS_H__

namespace pipeViewer {

    //! \brief Target-dependent pointer type for use by Cython
    typedef void* ptr_t;
}

#include <Python.h>

typedef struct {
  PyObject_HEAD
  void *ptr; // This is the pointer to the actual C++ instance
  void *ty;  // swig_type_info originally, but shouldn't matter
  int own;
  PyObject *next;
} SwigPyObject;

#ifndef ARGOS_VERSION
#define ARGOS_VERSION unknown
#endif

#endif // #ifndef __HELPERS_H__
