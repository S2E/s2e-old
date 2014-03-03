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

#include "ConcolicSession.h"

#include "S2EGuest.h"

#include <Python.h>

#include <cassert>
#include <string>


#define S2E_CONCOLIC_PLUGIN        "ConcolicSession"
#define MIN_BUFFER_SIZE            4096

namespace {

using namespace chef;


typedef enum {
	CONCOLIC_RET_OK,
	CONCOLIC_RET_TOO_SMALL,
	CONCOLIC_RET_ERROR
} ConcolicStatus;


typedef enum {
	START_CONCOLIC_SESSION,
	END_CONCOLIC_SESSION,
	GET_CONCOLIC_ALTERNATES
} ConcolicCommand;


typedef struct {
	ConcolicCommand command;
	uint32_t max_time;
	uint8_t is_error_path;
	uint32_t result_ptr;
	uint32_t result_size;
} __attribute__((packed)) ConcolicMessage;


int Communicate(S2EGuest *s2e_guest, const ConcolicMessage &message) {

	return s2e_guest->InvokePlugin(S2E_CONCOLIC_PLUGIN,
			(void*)&message, sizeof(message));
}


void DecodeArrayName(const std::string &name, std::string &assgn_key,
		std::string &assgn_value, char &assgn_type) {

	size_t dot_pos = name.rfind('.');
	if (dot_pos == std::string::npos) {
		assgn_key = name;
	} else {
		assgn_key = name.substr(0, dot_pos);
		assgn_value = name.substr(dot_pos + 1);

		if (assgn_value.at(1) == '#') {
			assgn_type = assgn_value.at(0);
			assert(assgn_value.size() > 2 && "Invalid value encoding");
			assgn_value = assgn_value.substr(2);
		} else {
			assgn_type = 'b';
		}
	}
}


PyObject *ConvertBufferValue(const std::string &value, const char &assgn_type) {
	switch (assgn_type) {
	case 'i': // Integer
		assert(value.size() == sizeof(int32_t) && "Invalid content size");
		return PyInt_FromLong(*((int32_t*)value.data()));
	case 'l': // Py_ssize_t
		assert(value.size() == sizeof(Py_ssize_t) && "Invalid content size");
		return PyInt_FromSsize_t(*((Py_ssize_t*)value.data()));
	case 's': // Regular string
		return PyString_FromStringAndSize(value.data(), value.size());
	case 'u': // Unicode string
		return PyUnicode_FromUnicode((Py_UNICODE *)value.data(),
				value.size() / sizeof(Py_UNICODE));
	case 'b': // Byte array
		return PyByteArray_FromStringAndSize(value.data(), value.size());
	default:
		assert(0 && "Invalid assignment type");
		return NULL;
	}
}


int DecodeAssignment(PyObject *assgndict, const std::string &name,
		const std::string &value_buff) {
	std::string assgn_key;
	std::string assgn_value;
	char assgn_type = 'b';

	DecodeArrayName(name, assgn_key, assgn_value, assgn_type);

	PyObject *assgn_key_obj = NULL, *assgn_value_obj = NULL;
	PyObject *value = NULL, *valuedict = NULL;
	int has_key;
	int result = -1;

	assgn_key_obj = PyString_FromString(assgn_key.c_str());
	assgn_value_obj = PyString_FromString(assgn_value.c_str());

	if (assgn_key_obj == NULL || assgn_value_obj == NULL) {
		goto done;
	}

	value = ConvertBufferValue(value_buff, assgn_type);
	if (value == NULL) {
		goto done;
	}

	has_key = PyDict_Contains(assgndict, assgn_key_obj);
	if (has_key < 0) {
		goto done;
	}

	if (!has_key) {
		valuedict = PyDict_New();
		if (PyDict_SetItem(assgndict, assgn_key_obj, valuedict) < 0) {
			goto done;
		}
	} else {
		valuedict = PyDict_GetItem(assgndict, assgn_key_obj);
		assert(valuedict != NULL);
		Py_INCREF(valuedict);
	}

	if (PyDict_SetItem(valuedict, assgn_value_obj, value) < 0) {
		goto done;
	}

	result = 0;

done:
	Py_XDECREF(valuedict);
	Py_XDECREF(assgn_key_obj);
	Py_XDECREF(assgn_value_obj);
	Py_XDECREF(value);

	return result;
}

PyObject *DecodeTimingInfo(const chef::TimingInfo &timing_info) {
	PyObject *result = NULL;

	PyObject *tstamp_obj = NULL;
	PyObject *sdelta_obj = NULL, *tdelta_obj = NULL, *udelta_obj = NULL;

	tstamp_obj = PyLong_FromUnsignedLongLong(
			(uint64_t)timing_info.time_stamp());

	if (timing_info.has_solver_delta()) {
		sdelta_obj = PyLong_FromUnsignedLongLong(
				(uint64_t)timing_info.solver_delta());
	} else {
		sdelta_obj = Py_None;
		Py_INCREF(sdelta_obj);
	}

	if (timing_info.has_total_delta()) {
		tdelta_obj = PyLong_FromUnsignedLongLong(
				(uint64_t)timing_info.total_delta());
	} else {
		tdelta_obj = Py_None;
		Py_INCREF(tdelta_obj);
	}

	if (timing_info.has_useful_delta()) {
		udelta_obj = PyLong_FromUnsignedLongLong(
				(uint64_t)timing_info.useful_delta());
	} else {
		udelta_obj = Py_None;
		Py_INCREF(udelta_obj);
	}

	if (tstamp_obj == NULL || sdelta_obj == NULL || tdelta_obj == NULL
			|| udelta_obj == NULL) {
		goto done;
	}

	result = PyTuple_Pack(4, tstamp_obj, sdelta_obj, tdelta_obj, udelta_obj);

done:
	Py_XDECREF(tstamp_obj);
	Py_XDECREF(sdelta_obj);
	Py_XDECREF(tdelta_obj);
	Py_XDECREF(udelta_obj);
	return result;
}

}


namespace chef {


PyObject *ConcolicSession::MakeConcolicInt(PyObject *target, const char *name,
		long max_value, long min_value) {
	assert(PyInt_Check(target));

	if (!s2e_guest_->version()) {
		PyErr_SetString(PyExc_RuntimeError, "Not in symbolic mode");
		return NULL;
	}

	PyIntObject *int_target = (PyIntObject*)target;
	long value = int_target->ob_ival;

	if (max_value >= min_value && (value < min_value || value > max_value)) {
		PyErr_SetString(PyExc_ValueError, "Incompatible value constraints");
		return NULL;
	}

	MakeConcolicBuffer(&value, sizeof(value), name, "value", 'i');
	if (max_value >= min_value) {
		s2e_guest_->Assume(value >= min_value);
		s2e_guest_->Assume(value <= max_value);
	}

	return PyInt_FromLong(value);
}


PyObject *ConcolicSession::MakeConcolicSequence(PyObject *target, const char *name,
		int max_size, int min_size) {
	if (!s2e_guest_->version()) {
		PyErr_SetString(PyExc_RuntimeError, "Not in symbolic mode");
		return NULL;
	}

	if (min_size < 0) {
		PyErr_SetString(PyExc_ValueError, "Minimum size cannot be negative");
		return NULL;
	}

	if (target == Py_None) {
		PyErr_SetString(PyExc_ValueError, "Cannot make symbolic None");
		return NULL;
	} else if (PyString_Check(target)) {
		return MakeConcolicString(target, name, max_size, min_size);
	} else if (PyUnicode_Check(target)) {
		return MakeConcolicUnicode(target, name, max_size, min_size);
	} else if (PyList_Check(target)) {
		return MakeConcolicList(target, name, max_size, min_size);
	} else if (PyDict_Check(target)) {
		return MakeConcolicDict(target, name);
	} else if (PyTuple_Check(target)) {
		return MakeConcolicTuple(target, name);
	} else {
		PyErr_SetString(PyExc_TypeError, "Unsupported type");
		return NULL;
	}
}


/*
 * Mark the buffer `buf' of size `size' as concolic (i.e., the current value
 * of the buffer is preserved).  The name of the symbolic data is obtained
 * by concatenating the `base_name' with `name', separated by a '.' character.
 *
 * The `name' string has the format: <T>#<name>, where T is a format character
 * identifying the Python type used to reconstruct the value when returned
 * by the symbolic execution engine.  Currently supported values: i (int),
 * s (Regular string), u (Unicode string), b (bytearray), l (Python size).
 * 'b' is the default format if the `name' string doesn't obey the format.
 */
void ConcolicSession::MakeConcolicBuffer(void *buf, int size,
		const char *base_name, const char *name, const char type) {
	static char obj_name[256];
	snprintf(obj_name, 256, "%s.%c#%s", base_name, type, name);

	s2e_guest_->MakeConcolic(buf, size, obj_name);
}

int ConcolicSession::CheckObjectSize(Py_ssize_t size, int max_size,
		int min_size) {
	assert(min_size >= 0);

	if (max_size < 0) {
		return 0; // Fixed-size objects are OK
	} else if (max_size == 0) {
		return (size >= min_size) ? 0 : (-1);
	} else {
		return (size >= min_size && size <= max_size) ? 0 : (-1);
	}
}

void ConcolicSession::ConstrainObjectSize(Py_ssize_t size, int max_size,
		int min_size) {
	assert(min_size >= 0);

	if (max_size > 0) {
		s2e_guest_->Assume(size <= max_size);
	}
	s2e_guest_->Assume(size >= min_size);
}


PyObject *ConcolicSession::MakeConcolicString(PyObject *target,
		const char *name, int max_size, int min_size) {
	assert(PyString_Check(target));

	PyStringObject *str_target = (PyStringObject*)target;

	if (CheckObjectSize(str_target->ob_size, max_size, min_size) < 0) {
		PyErr_SetString(PyExc_ValueError, "Incompatible size constraints");
		return NULL;
	}

	char *str_data = (char *)PyMem_Malloc(str_target->ob_size);
	if (!str_data) {
		return PyErr_NoMemory();
	}
	memcpy(str_data, str_target->ob_sval, str_target->ob_size);
	MakeConcolicBuffer(str_data, str_target->ob_size, name, "value", 's');

	PyObject *result = PyString_FromStringAndSize(str_data, str_target->ob_size);
	if (result == NULL) {
		PyMem_Free(str_data);
		return NULL;
	}

	if (max_size >= 0) {
		PyStringObject *str_result = (PyStringObject*)result;
		MakeConcolicBuffer(&str_result->ob_size, sizeof(str_result->ob_size),
				name, "size", 'l');
		ConstrainObjectSize(str_result->ob_size, max_size, min_size);
	}

	PyMem_Free(str_data);
	return result;
}


PyObject *ConcolicSession::MakeConcolicUnicode(PyObject *target,
		const char *name, int max_size, int min_size) {
	assert(PyUnicode_Check(target));

	PyUnicodeObject *uni_target = (PyUnicodeObject*)target;

	if (CheckObjectSize(uni_target->length, max_size, min_size) < 0) {
		PyErr_SetString(PyExc_ValueError, "Incompatible size constraints");
		return NULL;
	}

	Py_ssize_t buf_size = uni_target->length * sizeof(Py_UNICODE);

	Py_UNICODE *uni_data = (Py_UNICODE *)PyMem_Malloc(buf_size);
	if (!uni_data) {
		return PyErr_NoMemory();
	}
	memcpy(uni_data, uni_target->str, buf_size);
	MakeConcolicBuffer(uni_data, buf_size, name, "value", 'u');

	PyObject *result = PyUnicode_FromUnicode(uni_data, uni_target->length);
	if (result == NULL) {
		PyMem_Free(uni_data);
		return NULL;
	}

	if (max_size >= 0) {
		PyUnicodeObject *uni_result = (PyUnicodeObject*)result;
		MakeConcolicBuffer(&uni_result->length, sizeof(uni_result->length),
				name, "size", 'l');
		ConstrainObjectSize(uni_result->length, max_size, min_size);
	}

	PyMem_Free(uni_data);
	return result;
}


PyObject *ConcolicSession::MakeConcolicList(PyObject *target,
		const char *name, int max_size, int min_size) {
	assert(PyList_Check(target));

	PyListObject *list_target = (PyListObject*)target;
	if (CheckObjectSize(list_target->ob_size, max_size, min_size) < 0) {
		PyErr_SetString(PyExc_ValueError, "Incompatible size constraints");
		return NULL;
	}

	if (max_size >= 0) {
		MakeConcolicBuffer(&list_target->ob_size, sizeof(list_target->ob_size),
				name, "size", 'l');
		ConstrainObjectSize(list_target->ob_size, max_size, min_size);
	}

	Py_INCREF(target);
	return target;
}


PyObject *ConcolicSession::MakeConcolicDict(PyObject *target,
		const char *name) {
	assert(PyDict_Check(target));

	PyDictObject *dict_target = (PyDictObject*)target;
	MakeConcolicBuffer(&dict_target->ma_used, sizeof(dict_target->ma_used),
			name, "size", 'l');
	s2e_guest_->Assume(dict_target->ma_used >= 0);
	s2e_guest_->Assume(dict_target->ma_used < max_symbolic_size_);

	Py_INCREF(target);
	return target;
}


PyObject *ConcolicSession::MakeConcolicTuple(PyObject *target,
		const char *name) {
	assert(PyTuple_Check(target));

	PyTupleObject *tup_target = (PyTupleObject*)target;
	MakeConcolicBuffer(&tup_target->ob_size, sizeof(tup_target->ob_size),
			name, "size", 'l');
	s2e_guest_->Assume(tup_target->ob_size >= 0);
	s2e_guest_->Assume(tup_target->ob_size < max_symbolic_size_);

	Py_INCREF(target);
	return target;
}

int ConcolicSession::StartConcolicSession(bool stop_on_error,
		uint32_t max_time, bool use_random_select) {
	ConcolicMessage message;
	message.command = ::START_CONCOLIC_SESSION;
	message.max_time = max_time;

	return Communicate(s2e_guest_, message);
}

int ConcolicSession::EndConcolicSession(bool is_error_path) {
	ConcolicMessage message;
	message.command = ::END_CONCOLIC_SESSION;
	message.is_error_path = is_error_path;

	return Communicate(s2e_guest_, message);
}

} /* namespace symbex */
