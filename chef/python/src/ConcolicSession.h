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

#ifndef CONCOLICSESSION_H_
#define CONCOLICSESSION_H_

#include <Python.h>

#include <stdint.h>

namespace chef {

class S2EGuest;

class ConcolicSession {
public:
	ConcolicSession(S2EGuest *s2e_guest)
		: s2e_guest_(s2e_guest),
		  max_symbolic_size_(64) {

	}

	virtual ~ConcolicSession() {}

#if 0
	PyObject *RunConcolic(PyObject *callable, bool stop_on_error,
			uint32_t max_time, bool use_random_select, bool debug);
#endif

	int StartConcolicSession(bool stop_on_error, uint32_t max_time,
			bool use_random_select);
	int EndConcolicSession(bool is_error_path);

	PyObject *MakeConcolicSequence(PyObject *target, const char *name,
			int max_size, int min_size);

	PyObject *MakeConcolicInt(PyObject *target, const char *name,
			long max_value, long min_value);

#if 0
	PyObject *DecodeTestCases(const char *test_cases, int length);
#endif

private:
	S2EGuest *s2e_guest_;
	int max_symbolic_size_;

	void MakeConcolicBuffer(void *buf, int size, const char *base_name,
			const char *name, const char type);

	int CheckObjectSize(Py_ssize_t size, int max_size, int min_size);
	void ConstrainObjectSize(Py_ssize_t size, int max_size, int min_size);

	PyObject *MakeConcolicString(PyObject *target, const char *name,
			int max_size, int min_size);
	PyObject *MakeConcolicUnicode(PyObject *target, const char *name,
			int max_size, int min_size);
	PyObject *MakeConcolicList(PyObject *target, const char *name,
			int max_size, int min_size);
	PyObject *MakeConcolicDict(PyObject *target, const char *name);
	PyObject *MakeConcolicTuple(PyObject *target, const char *name);
};

} /* namespace symbex */
#endif /* CONCOLICSESSION_H_ */
