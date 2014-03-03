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

#ifndef S2EGUEST_H_
#define S2EGUEST_H_

#include <stdint.h>

namespace chef {

class S2EGuest {
public:
	S2EGuest() {}
	virtual ~S2EGuest() {}

	virtual int version();

	virtual void Assume(int expression);
	virtual void Concretize(void *buf, int size);
	virtual void GetExample(void *buf, int size);
	virtual int InvokePlugin(const char *pluginName, void *data,
			uint32_t dataSize);
	virtual void KillState(int status, const char *message);
	virtual void MakeConcolic(void *buf, int size, const char *name);
	virtual void MakeSymbolic(void *buf, int size, const char *name);
};

}

#endif /* S2EGUEST_H_ */
