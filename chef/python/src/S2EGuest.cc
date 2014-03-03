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

#include "S2EGuest.h"
#include "Python.h"
#include "s2e.h"

namespace chef {

int S2EGuest::version() {
	return s2e_version();
}

void S2EGuest::Assume(int expression) {
	s2e_assume(expression);
}

void S2EGuest::Concretize(void *buf, int size) {
	s2e_concretize(buf, size);
}

void S2EGuest::GetExample(void *buf, int size) {
	s2e_get_example(buf, size);
}

int S2EGuest::InvokePlugin(const char *pluginName, void *data,
		uint32_t dataSize) {
	return s2e_invoke_plugin(pluginName, data, dataSize);
}

void S2EGuest::KillState(int status, const char *message) {
	s2e_kill_state(status, message);
}

void S2EGuest::MakeConcolic(void *buf, int size, const char *name) {
	s2e_make_concolic(buf, size, name);
}

void S2EGuest::MakeSymbolic(void *buf, int size, const char *name) {
	s2e_make_symbolic(buf, size, name);
}

}
