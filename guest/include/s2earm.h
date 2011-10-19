/******************************************************************************
 * s2earm.h
 * Header for invoking custom S2E-instructions in native ARM programs
 *  Created on: 20.07.2011
 *      Author: Andreas Kirchner
 *
 ******************************************************************************
 *
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
 */

/** Get S2E version or 0 when running without S2E. */
static inline int s2e_version()
{
	int version;
    __asm__ volatile(
        ".WORD 0xff000000\n\t"   /* S2E opcode to store version in r0 */
	".ALIGN\n"
	"mov %[v], r0\n\t"
        : [v] "+r" (version) /* output */
        : /* no input */
        : "r0" /* clobbing */
    );
    return version;
}

/** Print message to the S2E log. */
static inline void s2e_message(const char* message)
{
    __asm__ __volatile__(
	".WORD 0xFF100000\n\t"
        : /*no output */
	: [msg] "r" (message)
	: "r0" /* we want the compiler to use r0 */
    );
}

/** Print warning to the S2E log and S2E stdout. */
static inline void s2e_warning(const char* message)
{
    __asm__ __volatile__(
		".WORD 0xFF100100\n\t"
		".ALIGN\n"
            : /*no output */
		: [msg] "r" (message)
		: "r0" /* we want the compiler to use r0 */
    );
}

/** Print symbolic expression to the S2E log. */
static inline void s2e_print_expression(const char* name, int expression)
{
    __asm__ __volatile__(
        "stmfd sp!,{r0, r1}\n\t"
	"mov r0, %[expr]\n\t"
	"mov r1, %[n]\n\t"
        ".WORD 0xFF070100\n\t"
        ".ALIGN\n\t"
        "ldmfd sp!,{r0, r1}\n\t"
        : /*no output */
	: [expr] "r" (expression), [n] "r" (name)
	: "r0", "r1"
    );
}

/** Enable forking on symbolic conditions. */
static inline void s2e_enable_forking(void)
{
    __asm__ __volatile__(
      ".WORD 0xFF090000\n\t"
      ".ALIGN\n"
    );
}

/** Disable forking on symbolic conditions. */
static inline void s2e_disable_forking(void)
{
    __asm__ __volatile__(
		".WORD 0xFF0A0000\n\t"
	    ".ALIGN\n"
    );
}

/** Get the current execution path/state id. */
static inline unsigned s2e_get_path_id(void)
{
    unsigned id;
    __asm__ __volatile__(
		".WORD 0xFF050000\n\t"
	    ".ALIGN\n"
		"MOV %[result], r0\n\t"
        : [result] "=r" (id) /* write content to C variable id */
        : /* no input operand */
    );
    return id;
}

/** Fill buffer with unconstrained symbolic values. */
static inline void s2e_make_symbolic(void* buf, int size, const char* name)
{
    __asm__ __volatile__(
	"stmfd sp!,{r0, r1, r2}\n\t"
	"MOV r0, %[buffer]\n\t"
	"MOV r1, %[symsize]\n\t"
	"MOV r2, %[symname]\n\t"
	".WORD 0xFF030000\n\t"
	".ALIGN\n\t"
	"ldmfd sp!,{r0, r1, r2}\n\t"
        : /* no output operands */
        : [buffer] "r" (buf), [symsize] "r" (size), [symname] "r" (name)
        : "memory", "r0", "r1", "r2"
    );
}

/** Concretize the expression. */
static inline void s2e_concretize(void* buf, int size)
{
    __asm__ __volatile__(
        "stmfd sp!,{r0, r1}\n\t"
	"MOV r0, %[buffer]\n\t"
	"MOV r1, %[symsize]\n\t"
        ".WORD 0xFF200000\n\t"
        ".ALIGN\n\t"
        "ldmfd sp!,{r0, r1}\n\t"
        : /* no output operands */
	: [buffer] "r" (buf), [symsize] "r" (size)
	: "memory", "r0", "r1"
    );
}

/** Get example value for expression (without adding state constraints). */
static inline void s2e_get_example(void* buf, int size)
{
    __asm__ __volatile__(
            "stmfd sp!,{r0, r1}\n\t"
		"MOV r0, %[buffer]\n\t"
		"MOV r1, %[symsize]\n\t"
            ".WORD 0xFF210000\n\t"
            ".ALIGN\n\t"
            "ldmfd sp!,{r0, r1}\n\t"
            : /* no output operands */
		: [buffer] "r" (buf), [symsize] "r" (size)
		: "memory", "r0", "r1"
    );
}

/** Get example value for expression (without adding state constraints). */
/** Convenience function to be used in printfs */
static inline unsigned s2e_get_example_uint(unsigned val)
{
    unsigned buf = val;
    __asm__ __volatile__(
            "stmfd sp!,{r1}\n\t"
		"MOV r0, %[buffer]\n\t"
		"MOV r1, %[symsize]\n\t"
            ".WORD 0xFF210000\n\t"
            ".ALIGN\n\t"
            "ldmfd sp!,{r1}\n\t"
            : /* no output operands */
		: [buffer] "r" (&buf), [symsize] "r" (sizeof(buf))
		: "memory", "r0", "r1"
    );
    return buf;
}

/** Get the current S2E_RAM_OBJECT_BITS configuration macro */
static inline int s2e_get_ram_object_bits()
{
    int bits;
    __asm__ __volatile__(
            "stmfd sp!,{r0}\n\t"
            ".WORD 0xFF520000\n\t"
            ".ALIGN\n\t"
		"MOV %[robits], r0\n\t"
            "ldmfd sp!,{r0}\n\t"
        : [robits] "=r" (bits)
        : /* no input */
        : "r0"
    );
    return bits;
}


/** Terminate current state. */
static inline void s2e_kill_state(int status, const char* message)
{
    __asm__ __volatile__(
        "stmfd sp!,{r0,r1}\n\t"
	"MOV r0, %[statcode]\n\t"
	"MOV r1, %[killmsg]\n\t"
        ".WORD 0xFF060000\n\t"
        ".ALIGN\n\t"
        "ldmfd sp!,{r0,r1}\n\t"
        : /* no output operand*/
	: [statcode] "r" (status), [killmsg] "r" (message)
	: "r0", "r1"
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


/** Declare a merge point: S2E will try to merge
 *  all states when they reach this point.
 *
 * NOTE: This requires merge searcher to be enabled. */
static inline void s2e_merge_point()
{
    __asm__ __volatile__(
        ".WORD 0xFF700000\n\t"
        ".ALIGN\n\t"
    );
}

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures */
static inline void s2e_rawmon_loadmodule(const char *name, unsigned loadbase, unsigned size)
{
    __asm__ __volatile__(
            "stmfd sp!,{r0,r1,r2}\n\t"
		"MOV r0, %[rawname]\n\t"
		"MOV r1, %[rawbase]\n\t"
		"MOV r2, %[modsize]\n\t"
            ".WORD 0xFFAA0000\n\t"
            ".ALIGN\n\t"
            "ldmfd sp!,{r0,r1,r2}\n\t"
        : /*no output operand */
	: [rawname] "r" (name), [rawbase] "r" (loadbase), [modsize] "r" (size)
	: "r0", "r1", "r2"
    );
}
