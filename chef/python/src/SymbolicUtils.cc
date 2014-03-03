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

#include "SymbolicUtils.h"
#include "S2EGuest.h"

#include <Python.h>

namespace chef {

/*
 * Obtain a concrete value for a possibly symbolic pointer value `p'.
 */
void *SymbolicUtils::ConcretizePointer(const void *p) {
  if (s2e_guest_->version()) {
	  s2e_guest_->Concretize((void*)&p, sizeof(p));
	  return (void *)p;
  } else {
	  return (void *)p;
  }
}


/*
 * Obtain a concrete string out of a possibly symbolic string `s'.
 */
const char *SymbolicUtils::ConcretizeString(const char *s) {
  char *sc = (char*)ConcretizePointer(s);
  unsigned i;

  for (i = 0; ; ++i) {
    char c = *sc;
    // String ends at powers of two
    if (!(i&(i-1))) {
      if (!c) {
        *sc++ = 0;
        break;
        // TODO: See if we should make explicit path separators at this point,
        // since they may occur frequently. However, premature path splitting
        // sounds like redundant work, so we may decide to skip...
      } /* else if (c=='/') {
        *sc++ = '/';
      } */
    } else {
      char cc = c;
      if (s2e_guest_->version()) {
    	  s2e_guest_->GetExample(&cc, sizeof(cc));
      }

      // TODO: See if we need to force the concretization in the PC
      // klee_assume(cc == c);
      *sc++ = cc;
      if (!cc) break;
    }
  }

  return s;
}


/*
 * Construct an unconstrained symbolic Python string object of size `size'
 * and symbolic name `name'.
 */
PyObject *SymbolicUtils::MakeSymbolicString(unsigned int size,
		const char *name) {
	char *sym_data = (char *)PyMem_Malloc(size + 1);

	if (!sym_data) {
		return PyErr_NoMemory();
	}

	if (s2e_guest_->version()) {
		s2e_guest_->MakeSymbolic((void*)sym_data, size + 1, name);
	} else {
		memset(sym_data, 'X', size);
	}

	sym_data[size] = 0;

	PyObject *result = PyString_FromString(sym_data);
	PyMem_Free(sym_data);

	return result;
}


/*
 * Terminate the current execution state with status code `status' and
 * message `message'.
 */
void SymbolicUtils::KillState(int status, const char *message) {
	if (s2e_guest_->version()) {
		s2e_guest_->KillState(status, message);
	} else {
		Py_Exit(status);
	}
}

} /* namespace symbex */
