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
 *    Andreas Kirchner <akalypse@gmail.com>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

// custom S2E-instructions for native ARM programs

#include <assert.h>

#define S2E_INSTRUCTION_COMPLEX(val1, val2)             \
        ".WORD 0xFF" #val1 #val2 "00\n\t"

#define S2E_INSTRUCTION_SIMPLE(val)                     \
    S2E_INSTRUCTION_COMPLEX(val, 00)


/** Get S2E version or 0 when running without S2E. */
static inline int s2e_version()
{
	int version = 0;
    __asm__ volatile(
    	S2E_INSTRUCTION_SIMPLE(00)   /* S2E opcode to store version in r0 */
    	".ALIGN\n"
    	"mov %[v], r0\n\t"
        : [v] "+r" (version) /* output */
        : /* no input */
        : "r0" /* clobbing */
    );
    return version;
}

/** Enable symbolic execution. */
static inline void s2e_enable_symbolic(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(01)
        ".ALIGN\n"
    );
}

/** Disable symbolic execution. */
static inline void s2e_disable_symbolic(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(02)
        ".ALIGN\n"
    );
}

/** Enable forking on symbolic conditions. */
static inline void s2e_enable_forking(void)
{
    __asm__ __volatile__(
		S2E_INSTRUCTION_SIMPLE(09)
		".ALIGN\n"
    );
}

/** Disable forking on symbolic conditions. */
static inline void s2e_disable_forking(void)
{
    __asm__ __volatile__(
		S2E_INSTRUCTION_SIMPLE(0A)
		".ALIGN\n"
    );
}

/** Print message to the S2E log. */
static inline void s2e_message(const char* message)
{
	__s2e_touch_string(message);
    __asm__ __volatile__(
    	S2E_INSTRUCTION_SIMPLE(10)
        : /*no output */
    	: [msg] "r" (message)
    	: "r0"
    );
}

/** Print warning to the S2E log and S2E stdout. */
static inline void s2e_warning(const char* message)
{
	__s2e_touch_string(message);
    __asm__ __volatile__(
        	"MOV r0, %[msg]\n\t"
        	".WORD 0xFF100100\n\t"
        	".ALIGN\n"
            : /*no output */
        	: [msg] "r" (message)
        	: "r0"
    );
}

/** Print symbolic expression to the S2E log. */
static inline void s2e_print_expression(const char* name, int expression)
{
	__s2e_touch_string(name);
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

/** Get the current execution path/state id. */
static inline unsigned s2e_get_path_id(void)
{
    unsigned id;
    __asm__ __volatile__(
    		S2E_INSTRUCTION_SIMPLE(05)
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
	__s2e_touch_string(name);
	__s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
    	"stmfd sp!,{r0, r1, r2}\n\t"
    	"MOV r0, %[buffer]\n\t"
    	"MOV r1, %[symsize]\n\t"
    	"MOV r2, %[symname]\n\t"
    	S2E_INSTRUCTION_SIMPLE(03)
    	".ALIGN\n\t"
    	"ldmfd sp!,{r0, r1, r2}\n\t"
        : /* no output operands */
        : [buffer] "r" (buf), [symsize] "r" (size), [symname] "r" (name)
        : "memory", "r0", "r1", "r2"
    );
}

static inline void s2e_make_concolic(void *buf, int size, const char *name)
{
	__s2e_touch_string(name);
	__s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
    	"stmfd sp!,{r0, r1, r2}\n\t"
    	"MOV r0, %[buffer]\n\t"
    	"MOV r1, %[symsize]\n\t"
    	"MOV r2, %[symname]\n\t"
    	S2E_INSTRUCTION_SIMPLE(11)
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
	__s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
        "stmfd sp!,{r0, r1}\n\t"
    	"MOV r0, %[buffer]\n\t"
    	"MOV r1, %[symsize]\n\t"
    	S2E_INSTRUCTION_SIMPLE(20)
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
	__s2e_touch_buffer(buf, size);
    __asm__ __volatile__(
            "stmfd sp!,{r0, r1}\n\t"
        	"MOV r0, %[buffer]\n\t"
        	"MOV r1, %[symsize]\n\t"
    		S2E_INSTRUCTION_SIMPLE(21)
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
    		S2E_INSTRUCTION_SIMPLE(21)
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
            "stmfd sp!,{}\n\t"
    		S2E_INSTRUCTION_SIMPLE(52)
            ".ALIGN\n\t"
    		"MOV %[robits], r0\n\t"
            "ldmfd sp!,{}\n\t"
        : [robits] "=r" (bits)
        : /* no input */
        : "r0"
    );
    return bits;
}


/** Terminate current state. */
static inline void s2e_kill_state(int status, const char* message)
{
	__s2e_touch_string(message);
    __asm__ __volatile__(
        "stmfd sp!,{r0,r1}\n\t"
    	"MOV r0, %[statcode]\n\t"
    	"MOV r1, %[killmsg]\n\t"
    	S2E_INSTRUCTION_SIMPLE(06)
        ".ALIGN\n\t"
        "ldmfd sp!,{r0,r1}\n\t"
        : /* no output operand*/
	: [statcode] "r" (status), [killmsg] "r" (message)
	: "r0", "r1"
    );
}

/** Declare a merge point: S2E will try to merge
 *  all states when they reach this point.
 *
 * NOTE: This requires merge searcher to be enabled. */
static inline void s2e_merge_point()
{
    __asm__ __volatile__(
    	S2E_INSTRUCTION_SIMPLE(70)
        ".ALIGN\n\t"
    );
}

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures */
static inline void s2e_rawmon_loadmodule(const char *name, unsigned loadbase, unsigned size)
{
	__s2e_touch_string(name);
    __asm__ __volatile__(
            "stmfd sp!,{r0,r1,r2}\n\t"
    		"MOV r0, %[rawname]\n\t"
    		"MOV r1, %[rawbase]\n\t"
    		"MOV r2, %[modsize]\n\t"
    		S2E_INSTRUCTION_SIMPLE(AA)
            ".ALIGN\n\t"
            "ldmfd sp!,{r0,r1,r2}\n\t"
        : /*no output operand */
	: [rawname] "r" (name), [rawbase] "r" (loadbase), [modsize] "r" (size)
	: "r0", "r1", "r2"
    );
}

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures. */
static inline void s2e_rawmon_loadmodule2(const char *name,
                                          uint64_t nativebase,
                                          uint64_t loadbase,
                                          uint64_t entrypoint,
                                          uint64_t size,
                                          unsigned kernelMode)
{
    s2e_opcode_module_config_t cfg;
    cfg.name = (uintptr_t) name;
    cfg.nativeBase = nativebase;
    cfg.loadBase = loadbase;
    cfg.entryPoint = entrypoint;
    cfg.size = size;
    cfg.kernelMode = kernelMode;

    __s2e_touch_string(name);
    __asm__ __volatile__(
		"stmfd sp!,{r2}\n\t"
		"MOV r2, %[cfg]\n\t"
		S2E_INSTRUCTION_COMPLEX(AA, 02)
		 ".ALIGN\n\t"
		 "ldmfd sp!,{r2}\n\t"
        : /* no output operand*/
        : [cfg] "r" (&cfg)
      	: "r2"
    );

}

/** Returns true if ptr points to symbolic memory */
static inline int s2e_is_symbolic(void *ptr, size_t size)
{
    int result;
    __s2e_touch_buffer(ptr, 1);
    __asm__ __volatile__(
		"stmfd sp!,{r0,r2}\n\t"
		"MOV r0, %[size]\n\t"
		"MOV r2, %[ptr]\n\t"
		S2E_INSTRUCTION_SIMPLE(04)
		".ALIGN\n\t"
		"MOV %[result], r0\n\t"
		"ldmfd sp!,{r0,r2}\n\t"
        : [result] "=r" (result)
        : [size] "r" (size), [ptr] "r" (ptr)
      	: "r0", "r2"
    );
    return result;
}

/** Open file from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_open(const char *fname)
{
    int fd;
    __s2e_touch_string(fname);
    __asm__ __volatile__(
		"stmfd sp!,{r0,r1,r2}\n\t"
		"MOV r0, #-1\n\t"
		"MOV r1, %[fname]\n\t"
		"MOV r2, #0\n\t"
		S2E_INSTRUCTION_SIMPLE(EE)
		"MOV %[fd], r0\n\t"
		".ALIGN\n\t"
		"ldmfd sp!,{r0,r1,r2}\n\t"
        : [fd] "=r" (fd)
        : [fname] "r" (fname)
      	: "r0", "r1", "r2"
    );
    return fd;
}

/** Close file from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_close(int fd)
{
    int res;
    __asm__ __volatile__(
		"stmfd sp!,{r1}\n\t"
		"MOV r0, #-1\n\t"
		"MOV r1, %[fd]\n\t"
    	S2E_INSTRUCTION_COMPLEX(EE, 01)
		".ALIGN\n\t"
		"MOV %[res], r0\n\t"
		"ldmfd sp!,{r1}\n\t"
        : [res] "=r" (res)
        : [fd] "r" (fd)
      	: "r0", "r1"
    );
    return res;
}

/** Read file content from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_read(int fd, char *buf, int count)
{
    int res;
    __s2e_touch_buffer(buf, count);
    __asm__ __volatile__(
            "stmfd sp!,{r1,r2,r3}\n\t"
    		"MOV r0, #-1\n\t"
    		"MOV r1, %[fd]\n\t"
    		"MOV r2, %[buf]\n\t"
    		"MOV r3, %[count]\n\t"
    		S2E_INSTRUCTION_COMPLEX(EE, 02)
    		"MOV %[res], r0\n\t"
            ".ALIGN\n\t"
            "ldmfd sp!,{r1,r2,r3}\n\t"
        : [res] "=r" (res)
        : [fd] "r" (fd), [buf] "r" (buf), [count] "r" (count)
      	: "r0", "r1", "r2", "r3"
    );
    return res;
}

/** CodeSelector plugin */
/** Enable forking in the current process (entire address space or user mode only). */
static inline void s2e_codeselector_enable_address_space(unsigned user_mode_only)
{
    __asm__ __volatile__(
   		"stmfd sp!,{r2}\n\t"
   		"MOV r2, %[user_mode_only]\n\t"
    	S2E_INSTRUCTION_SIMPLE(AE)
		".ALIGN\n\t"
		"ldmfd sp!,{r2}\n\t"
        : /* no output operand*/
        : [user_mode_only] "r" (user_mode_only)
      	: "r2"
    );
}

/** Disable forking in the specified process (represented by its page directory).
    If pagedir is 0, disable forking in the current process. */
static inline void s2e_codeselector_disable_address_space(uint64_t pagedir)
{
    __asm__ __volatile__(
		"stmfd sp!,{r2}\n\t"
		"MOV r2, %[pagedir]\n\t"
		S2E_INSTRUCTION_COMPLEX(AE, 01)
		".ALIGN\n\t"
		"ldmfd sp!,{r2}\n\t"
		: /* no output operand*/
		: [pagedir]"r" (pagedir)
	    : "r2"
    );
}

static inline void s2e_codeselector_select_module(const char *moduleId)
{
    __s2e_touch_string(moduleId);
    __asm__ __volatile__(
		"stmfd sp!,{r2}\n\t"
		"MOV r2, %[moduleId]\n\t"
    	S2E_INSTRUCTION_COMPLEX(AE, 02)
		".ALIGN\n\t"
		"ldmfd sp!,{r2}\n\t"
        : /* no output operand*/
    	: [moduleId] "r" (moduleId)
      	: "r2"
    );
}

/** Programmatically add a new configuration entry to the ModuleExecutionDetector plugin. */
static inline void s2e_moduleexec_add_module(const char *moduleId, const char *moduleName, int kernelMode)
{
    __s2e_touch_string(moduleId);
    __s2e_touch_string(moduleName);
    __asm__ __volatile__(
		"stmfd sp!,{r0,r2,r4}\n\t"
		"MOV r0, %[moduleName]\n\t"
		"MOV r2, %[moduleId]\n\t"
		"MOV r4, %[kernelMode]\n\t"
		S2E_INSTRUCTION_SIMPLE(AF)
		 ".ALIGN\n\t"
		 "ldmfd sp!,{r0,r2,r4}\n\t"
		: /* no output operand*/
        : [moduleId] "r" (moduleId), [moduleName] "r" (moduleName), [kernelMode] "r" (kernelMode)
      	: "r0", "r2", "r4"
    );
}

