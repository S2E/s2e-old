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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_CONFIG_H
#define S2E_CONFIG_H

/** How many S2E instances we want to handle.
    Plugins can use this constant to allocate blocks of shared memory whose size
    depends on the maximum number of processes (e.g., bitmaps) */
#define S2E_MAX_PROCESSES 48

/** Enables S2E TLB to speed-up concrete memory accesses */
#define S2E_ENABLE_S2E_TLB

/** This defines the size of each MemoryObject that represents physical RAM.
    Larger values save some memory, smaller (exponentially) decrease solving
    time for constraints with symbolic addresses */

#ifdef S2E_ENABLE_S2E_TLB
#define S2E_RAM_OBJECT_BITS 7
#else
/* Do not touch this */
#define S2E_RAM_OBJECT_BITS TARGET_PAGE_BITS
#endif

#define S2E_RAM_OBJECT_SIZE (1 << S2E_RAM_OBJECT_BITS)
#define S2E_RAM_OBJECT_MASK (~(S2E_RAM_OBJECT_SIZE - 1))

#define S2E_MEMCACHE_SUPERPAGE_BITS 20

/** Enables simple memory debugging support */
//#define S2E_DEBUG_MEMORY
//#define S2E_DEBUG_TLBCACHE

#define S2E_USE_FAST_SIGNALS

#endif // S2E_CONFIG_H
