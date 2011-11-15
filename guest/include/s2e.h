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

/** Forces the read of every byte of the specified string.
  * This makes sure the memory pages occupied by the string are paged in
  * before passing them to S2E, which can't page in memory by itself. */
static inline void __s2e_touch_string(volatile const char *string)
{
    while(*string) {
        ++string;
    }
}

static inline void __s2e_touch_buffer(volatile void *buffer, unsigned size)
{
    unsigned i;
    for (i=0; i<size; ++i) {
        volatile const char *b = (volatile const char *)buffer;
        *b;
    }
}

/** Get S2E version or 0 when running without S2E. */
static inline int s2e_version()
{
    int version;
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (version)  : "a" (0)
    );
    return version;
}

/** Print message to the S2E log. */
static inline void s2e_message(const char* message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x10, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" (message)
    );
}

/** Print warning to the S2E log and S2E stdout. */
static inline void s2e_warning(const char* message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x10, 0x01, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" (message)
    );
}

/** Print symbolic expression to the S2E log. */
static inline void s2e_print_expression(const char* name, int expression)
{
    __s2e_touch_string(name);
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x07, 0x01, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : : "a" (expression), "c" (name)
    );
}

/** Enable forking on symbolic conditions. */
static inline void s2e_enable_forking(void)
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x09, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Disable forking on symbolic conditions. */
static inline void s2e_disable_forking(void)
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x0a, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Get the current execution path/state id. */
static inline unsigned s2e_get_path_id(void)
{
    unsigned id;
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x05, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (id)
    );
    return id;
}

/** Fill buffer with unconstrained symbolic values. */
static inline void s2e_make_symbolic(void* buf, int size, const char* name)
{
    __s2e_touch_string(name);
    __s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x03, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (buf), "d" (size), "c" (name) : "memory"
    );
}

/** Returns true if ptr points to a symbolic value */
static inline int s2e_is_symbolic(void* ptr)
{
    int result;
    __s2e_touch_buffer(ptr, 1);
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x04, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (result) : "a" (0), "c" (ptr)
    );
    return result;
}

/** Concretize the expression. */
static inline void s2e_concretize(void* buf, int size)
{
    __s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x20, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (buf), "d" (size) : "memory"
    );
}

/** Get example value for expression (without adding state constraints). */
static inline void s2e_get_example(void* buf, int size)
{
    __s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x21, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (buf), "d" (size) : "memory"
    );
}

/** Get example value for expression (without adding state constraints). */
/** Convenience function to be used in printfs */
static inline unsigned s2e_get_example_uint(unsigned val)
{
    unsigned buf = val;
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x21, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (&buf), "d" (sizeof(buf)) : "memory"
    );
    return buf;
}

/** Terminate current state. */
static inline void s2e_kill_state(int status, const char* message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x06, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (status), "d" (message)
    );
}

/** Disable timer interrupt in the guest. */
static inline void s2e_disable_timer_interrupt()
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x50, 0x01, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Enable timer interrupt in the guest. */
static inline void s2e_enable_timer_interrupt()
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x50, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Disable all APIC interrupts in the guest. */
static inline void s2e_disable_all_apic_interrupts()
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x51, 0x01, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Enable all APIC interrupts in the guest. */
static inline void s2e_enable_all_apic_interrupts()
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x51, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Get the current S2E_RAM_OBJECT_BITS configuration macro */
static inline int s2e_get_ram_object_bits()
{
    int bits;
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x52, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (bits)  : "a" (0)
    );
    return bits;
}

/** Declare a merge point: S2E will try to merge
 *  all states when they reach this point.
 *
 * NOTE: This requires merge searcher to be enabled. */
static inline void s2e_merge_point()
{
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0x70, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
    );
}

/** Open file from the guest.
 *
 * NOTE: This require HostFiles plugin. */
static inline int s2e_open(const char* fname)
{
    int fd;
    __s2e_touch_string(fname);
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0xEE, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (fd) : "a"(-1), "b" (fname), "c" (0)
    );
    return fd;
}

/** Close file from the guest.
 *
 * NOTE: This require HostFiles plugin. */
static inline int s2e_close(int fd)
{
    int res;
    __asm__ __volatile__(
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0xEE, 0x01, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        : "=a" (res) : "a" (-1), "b" (fd)
    );
    return res;
}

/** Read file content from the guest.
 *
 * NOTE: This require HostFiles plugin. */
static inline int s2e_read(int fd, char* buf, int count)
{
    int res;
    __s2e_touch_buffer(buf, count);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%esi, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0xEE, 0x02, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : "=a" (res) : "a" (-1), "S" (fd), "c" (buf), "d" (count)
    );
    return res;
}

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures */
static inline void s2e_rawmon_loadmodule(const char *name, unsigned loadbase, unsigned size)
{
    __s2e_touch_string(name);
    __asm__ __volatile__(
        "pushl %%ebx\n"
        "movl %%edx, %%ebx\n"
        ".byte 0x0f, 0x3f\n"
        ".byte 0x00, 0xAA, 0x00, 0x00\n"
        ".byte 0x00, 0x00, 0x00, 0x00\n"
        "popl %%ebx\n"
        : : "a" (name), "d" (loadbase), "c" (size)
    );
}

/* Kills the current state if b is zero */
static inline void _s2e_assert(int b, const char *expression )
{
   if (!b) {
      s2e_kill_state(0, expression);
   }
}

#define s2e_assert(expression) _s2e_assert(expression, "Assertion failed: "  #expression)
