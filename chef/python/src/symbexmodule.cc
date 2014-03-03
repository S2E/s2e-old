/*
 * Copyright (C) 2014 EPFL.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Python.h>

#include "ConcolicSession.h"
#include "S2EGuest.h"
#include "SymbolicUtils.h"

#include <stdint.h>
#include <stdlib.h>

using namespace chef;

#define DEFAULT_MAX_FORKS        1000

#define DEFAULT_MIN_SEQ_SIZE        0
#define DEFAULT_MAX_SEQ_SIZE     (-1)

#define DEFAULT_MIN_INT_VALUE   (-128)
#define DEFAULT_MAX_INT_VALUE     127

/*== Globals =================================================================*/

static PyObject *SymbexError;

static S2EGuest *s2e_guest;
static ConcolicSession *concolic_session;
static SymbolicUtils *symbolic_utils;


/*== High-level functions ====================================================*/

PyDoc_STRVAR(symbex_symsequence_doc,
"symsequence(size, name) --> string object\n\
\n\
Mark the given sequence or collection as symbolic. Supported object types: \n\
str, unicode, list, dict, tuple.");

static PyObject *
symbex_symsequence(PyObject *self, PyObject *args) {
	PyObject *target = NULL;
	const char *name;
	int max_size = DEFAULT_MAX_SEQ_SIZE;
	int min_size = DEFAULT_MIN_SEQ_SIZE;

	if (!PyArg_ParseTuple(args, "Os|ii:symsequence", &target, &name,
			&max_size, &min_size)) {
		return NULL;
	}

	return concolic_session->MakeConcolicSequence(target, name, max_size,
			min_size);
}


/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_symint_doc,
"symint() -> int object\n\
\n\
Mark the given integer object as symbolic. Supported object types: int.");

static PyObject *
symbex_symint(PyObject *self, PyObject *args) {
	PyObject *target = NULL;
	const char *name;
	long max_value = DEFAULT_MAX_INT_VALUE;
	long min_value = DEFAULT_MIN_INT_VALUE;

	if (!PyArg_ParseTuple(args, "Os|ll:symint", &target, &name, &max_value,
			&min_value)) {
		return NULL;
	}

	return concolic_session->MakeConcolicInt(target, name, max_value,
			min_value);
}


/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_symtoconcrete_doc,
"symtoconcrete(obj) -> object\n\
\n\
Return a concretized version of the symbolic object passed as a parameter.");

static PyObject *
symbex_symtoconcrete(PyObject *self, PyObject *args) {
  const char *string;
  if (!PyArg_ParseTuple(args, "s:symtoconcrete", &string))
    return NULL;

  return PyString_FromString(symbolic_utils->ConcretizeString(string));
}


/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_concrete_doc,
"concrete(obj) -> object\n\
\n\
Concretize a symbolic object");

static PyObject *
symbex_concrete(PyObject *self, PyObject *args) {
	PyObject *target;

	if (!PyArg_ParseTuple(args, "O:concrete", &target)) {
		return NULL;
	}

	if (!s2e_guest->version()) {
		Py_INCREF(target);
		return target;
	}

	if (PyInt_Check(target)) {
		PyIntObject *int_target = (PyIntObject*)target;
		long value = int_target->ob_ival;
		s2e_guest->GetExample(&value, sizeof(value));

		return PyInt_FromLong(value);
	} else {
		Py_INCREF(target);
		return target;
	}
}


/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_killstate_doc,
"killstate(status, message)\n\
\n\
Terminate the current execution state.");

static PyObject *
symbex_killstate(PyObject *self, PyObject *args) {
  int status;
  const char *message;

  if (!PyArg_ParseTuple(args, "is:killstate", &status, &message))
    return NULL;

  symbolic_utils->KillState(status, message);

  Py_INCREF(Py_None);
  return Py_None;
}

/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_startconcolic_doc,
"startconcolic([max_time]) \n\
\n\
Mark the start of a concolic session.");

static void startconcolic_atexit() {
	concolic_session->EndConcolicSession(true);
}

static PyObject *
symbex_startconcolic(PyObject *self, PyObject *args) {
	uint32_t max_time = 0;
	unsigned char end_at_exit = 1;
	if (!PyArg_ParseTuple(args, "|Ib:startconcolic", &max_time, &end_at_exit)) {
		return NULL;
	}

	if (concolic_session->StartConcolicSession(false, max_time, false) != 0) {
		PyErr_SetString(SymbexError, "Could not start concolic session");
		return NULL;
	}

	// Register a state killing function at program exit
	if (end_at_exit && Py_AtExit(&startconcolic_atexit) == -1) {
		PyErr_SetString(SymbexError, "Could not register exit function");
		return NULL;
	}

	Py_RETURN_NONE;
}

/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_endconcolic_doc,
"endconcolic(is_error_path) \n\
\n\
Terminate the concolic session.");

static PyObject *
symbex_endconcolic(PyObject *self, PyObject *args) {
	unsigned char is_error_path;
	if (!PyArg_ParseTuple(args, "b:endconcolic", &is_error_path)) {
		return NULL;
	}

	if (concolic_session->EndConcolicSession(is_error_path) != 0) {
		PyErr_SetString(SymbexError, "Could not terminate concolic session");
		return NULL;
	}

	Py_RETURN_NONE;
}

/*----------------------------------------------------------------------------*/
#if 0
PyDoc_STRVAR(symbex_snapshotrun_doc,
"snapshotrun(func) -> a tuple\n\
\n\
Execute the given function in a machine snapshot. The current machine state\n\
is forked, and the function executes inside the fork. No effect of \n\
the function execution is visible in the original context.");


static PyObject *
symbex_snapshotrun(PyObject *self, PyObject *args) {
	PyObject *callable = NULL, *result = NULL;
	unsigned char stop_on_error = 1;
	uint32_t max_time = 0;
	unsigned char use_random_select = 0;
	unsigned char debug = 0;

	if (!PyArg_ParseTuple(args, "O|bIbb:snapshotrun", &callable,
			&stop_on_error, &max_time, &use_random_select, &debug)) {
		return NULL;
	}

	result = concolic_session->RunConcolic(callable, stop_on_error,
			max_time, use_random_select, debug);

	return result;
}
#endif

/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_assume_doc,
"assume(cond) \n\
\n\
Assume that the condition passed as argument is true.");


static PyObject *
symbex_assume(PyObject *self, PyObject *args) {
	unsigned char condition;

	if (!PyArg_ParseTuple(args, "b:assume", &condition)) {
		return NULL;
	}

	s2e_guest->Assume(condition);

	Py_RETURN_NONE;
}

/*----------------------------------------------------------------------------*/

PyDoc_STRVAR(symbex_assumeascii_doc,
"assumeascii(str) \n\
\n\
Assume that the string passed as argument doesn't contain characters beyond 0x7F");

static PyObject *
symbex_assumeascii(PyObject *self, PyObject *args) {
	PyStringObject *string_obj;
	Py_ssize_t i;

	if (!PyArg_ParseTuple(args, "O!:assumeascii", &PyString_Type, &string_obj)) {
		return NULL;
	}

	for (i = 0; i < Py_SIZE(string_obj); ++i) {
		s2e_guest->Assume(((unsigned char)string_obj->ob_sval[i]) < 0x80);
	}

	Py_RETURN_NONE;
}


/*----------------------------------------------------------------------------*/
#if 0
PyDoc_STRVAR(symbex_decodetc_doc,
"decodetc(str) \n\
\n\
Decode a set of test cases represented as a protobuf message");

static PyObject *
symbex_decodetc(PyObject *self, PyObject *args) {
	const char *test_cases;
	int length;

	if (!PyArg_ParseTuple(args, "s#:decodetc", &test_cases, &length)) {
		return NULL;
	}

	return concolic_session->DecodeTestCases(test_cases, length);
}
#endif

/*== Module Definition =======================================================*/

PyDoc_STRVAR(module_doc,
"Primitives for supporting symbolic execution inside the Python interpreter.");


static PyMethodDef SymbexMethods[] = {
	{ "symsequence", symbex_symsequence, METH_VARARGS, symbex_symsequence_doc },
	{ "symint", symbex_symint, METH_VARARGS, symbex_symint_doc },

	{ "symtoconcrete", symbex_symtoconcrete, METH_VARARGS,
			symbex_symtoconcrete_doc },
	{ "concrete", symbex_concrete, METH_VARARGS, symbex_concrete_doc },

	{ "killstate", symbex_killstate, METH_VARARGS, symbex_killstate_doc },

	{ "startconcolic", symbex_startconcolic, METH_VARARGS, symbex_startconcolic_doc },
	{ "endconcolic", symbex_endconcolic, METH_VARARGS, symbex_endconcolic_doc },
#if 0
	{ "snapshotrun", symbex_snapshotrun, METH_VARARGS, symbex_snapshotrun_doc },
#endif

	{ "assume", symbex_assume, METH_VARARGS, symbex_assume_doc },
	{ "assumeascii", symbex_assumeascii, METH_VARARGS, symbex_assumeascii_doc },

#if 0
	{ "decodetc", symbex_decodetc, METH_VARARGS, symbex_decodetc_doc },
#endif
	{ NULL, NULL, 0, NULL } /* Sentinel */
};


PyMODINIT_FUNC
initsymbex(void) {
	PyObject *m;

	m = Py_InitModule3("symbex", SymbexMethods, module_doc);
	if (m == NULL)
	  return;

	if (s2e_guest == NULL) {
		s2e_guest = new S2EGuest();
		concolic_session = new ConcolicSession(s2e_guest);
		symbolic_utils = new SymbolicUtils(s2e_guest);
	}

	if (SymbexError == NULL) {
		SymbexError = PyErr_NewException((char*)"symbex.SymbexError", NULL, NULL);
		if (SymbexError == NULL)
			return;
	}
	Py_INCREF(SymbexError);
	PyModule_AddObject(m, "SymbexError", SymbexError);
}
