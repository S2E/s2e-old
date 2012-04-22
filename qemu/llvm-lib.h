/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010-2012, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov (vova.kuznetsov@epfl.ch)
 *    Vitaly Chipounov (vitaly.chipounov@epfl.ch)
 *
 *
 * All contributors listed in S2E-AUTHORS.
 *
 */

/**
 * This file contains various helper functions to be used in LLVM bitcode
 * files for emulation helpers.
 */
#ifndef LLVMLIB_H

#define LLVMLIB_H

void klee_make_symbolic(void *addr, unsigned nbytes, const char *name);
uint8_t klee_int8(const char *name);
uint16_t klee_int16(const char *name);
uint32_t klee_int32(const char *name);
void uint32_to_string(uint32_t n, char *str);
void trace_port(char *buf, const char *prefix, uint32_t port, uint32_t pc);

uint8_t klee_int8(const char *name) {
    uint8_t ret;
    klee_make_symbolic(&ret, sizeof(ret), name);
    return ret;
}

uint16_t klee_int16(const char *name) {
    uint16_t ret;
    klee_make_symbolic(&ret, sizeof(ret), name);
    return ret;
}

uint32_t klee_int32(const char *name) {
    uint32_t ret;
    klee_make_symbolic(&ret, sizeof(ret), name);
    return ret;
}

//Helpers to avoid relying on sprintf that does not work properly
static char hextable[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
'c', 'd', 'e', 'f'};
void uint32_to_string(uint32_t n, char *str)
{
  str[0] = hextable[(n >> 28)];
  str[1] = hextable[((n >> 24) & 0xF)];
  str[2] = hextable[((n >> 20) & 0xF)];
  str[3] = hextable[((n >> 16) & 0xF)];
  str[4] = hextable[((n >> 12) & 0xF)];
  str[5] = hextable[((n >> 8) & 0xF)];
  str[6] = hextable[((n >> 4) & 0xF)];
  str[7] = hextable[((n >> 0) & 0xF)];
}

void trace_port(char *buf, const char *prefix, uint32_t port, uint32_t pc)
{
    while(*prefix) {
        *buf = *prefix;
        ++buf; ++prefix;
    }

    uint32_to_string(port, buf);
    buf+=8;
    *buf = '_';
    buf++;
    uint32_to_string(pc, buf);
    buf+=8;
    *buf = 0;
}


#endif
