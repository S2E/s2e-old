/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
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
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

/*
 * Essential structures for Android operating system
 */

#ifndef _ANDROID_STRUCTS_H_
#define _ANDROID_STRUCTS_H_
#include <s2e/Plugins/Linux/LinuxStructures.h>

using namespace s2e::linuxos;
namespace s2e {
namespace android {

/*
 * Linear allocation state.
 */
typedef struct LinearAllocHdr {
    int     curOffset;          /* offset where next data goes */
    char*   mapAddr;            /* start of mmap()ed region */
    int     mapLength;          /* length of region */
    int     firstOffset;        /* for chasing through */
    short*  writeRefCount;      /* for ENFORCE_READ_ONLY */
} LinearAllocHdr;

struct DalvikVM {
	linux_task process;
	TaskSet threads;

    /*
     * Bootstrap class loader linear allocator.
     */
	LinearAllocHdr* pBootLoaderAlloc;
};

struct Thread {
/* thread ID, only useful under Linux */
uint32_t       systemTid;
/* start (high addr) of interp stack (subtract size to get malloc addr) */
uint32_t*         interpStackStart;

/* current limit of stack; flexes for StackOverflowError */
const uint32_t*   interpStackEnd;

/* FP of bottom-most (currently executing) stack frame on interp stack */
void*       curFrame;

};



} //namespace android
} //namespace s2e
#endif //_ANDROID_STRUCTS_H_
