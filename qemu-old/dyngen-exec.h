/*
 *  dyngen defines for micro operation code
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The file was modified for S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#if !defined(__DYNGEN_EXEC_H__)
#define __DYNGEN_EXEC_H__

/* prevent Solaris from trying to typedef FILE in gcc's
   include/floatingpoint.h which will conflict with the
   definition down below */
#ifdef __sun__
#define _FILEDEFED
#endif

/* NOTE: standard headers should be used with special care at this
   point because host CPU registers are used as global variables. Some
   host headers do not allow that. */
#include <stddef.h>
#include <stdint.h>

#ifdef __OpenBSD__
#include <sys/types.h>
#endif

/* XXX: This may be wrong for 64-bit ILP32 hosts.  */
typedef void * host_reg_t;

#ifdef CONFIG_BSD
typedef struct __sFILE FILE;
#else
typedef struct FILE FILE;
#endif
extern int fprintf(FILE *, const char *, ...);
extern int fputs(const char *, FILE *);
extern int printf(const char *, ...);


#if defined(__i386__)
#define AREG0 "ebp"
#define AREG1 "ebx"
#define AREG2 "esi"
#elif defined(__x86_64__)
#define AREG0 "r14"
#define AREG1 "r15"
#define AREG2 "r12"
#elif defined(_ARCH_PPC)
#define AREG0 "r27"
#define AREG1 "r24"
#define AREG2 "r25"
#elif defined(__arm__)
#define AREG0 "r7"
#define AREG1 "r4"
#define AREG2 "r5"
#elif defined(__hppa__)
#define AREG0 "r17"
#define AREG1 "r14"
#define AREG2 "r15"
#elif defined(__mips__)
#define AREG0 "s0"
#define AREG1 "s1"
#define AREG2 "fp"
#elif defined(__sparc__)
#ifdef CONFIG_SOLARIS
#define AREG0 "g2"
#define AREG1 "g3"
#define AREG2 "g4"
#else
#ifdef __sparc_v9__
#define AREG0 "g5"
#define AREG1 "g6"
#define AREG2 "g7"
#else
#define AREG0 "g6"
#define AREG1 "g1"
#define AREG2 "g2"
#endif
#endif
#elif defined(__s390__)
#define AREG0 "r10"
#define AREG1 "r7"
#define AREG2 "r8"
#elif defined(__alpha__)
/* Note $15 is the frame pointer, so anything in op-i386.c that would
   require a frame pointer, like alloca, would probably loose.  */
#define AREG0 "$15"
#define AREG1 "$9"
#define AREG2 "$10"
#elif defined(__mc68000)
#define AREG0 "%a5"
#define AREG1 "%a4"
#define AREG2 "%d7"
#elif defined(__ia64__)
#define AREG0 "r7"
#define AREG1 "r4"
#define AREG2 "r5"
#else
#error unsupported CPU
#endif

#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#define stringify(s)	tostring(s)
#define tostring(s)	#s

/* The return address may point to the start of the next instruction.
   Subtracting one gets us the call instruction itself.  */
#if defined(__s390__) && !defined(__s390x__)
# define _GETPC() ((void*)(((uintptr_t)__builtin_return_address(0) & 0x7fffffffUL) - 1))
#elif defined(__arm__)
/* Thumb return addresses have the low bit set, so we need to subtract two.
   This is still safe in ARM mode because instructions are 4 bytes.  */
# define _GETPC() ((void *)((uintptr_t)__builtin_return_address(0) - 2))
#else
# define _GETPC() ((void *)((uintptr_t)__builtin_return_address(0) - 1))
#endif

#if defined(CONFIG_S2E)

//extern void* g_s2e_exec_ret_addr;
/*
#ifdef S2E_LLVM_LIB
#define GETPC() (g_s2e_exec_ret_addr)
#else
#define GETPC() (g_s2e_exec_ret_addr ? g_s2e_exec_ret_addr : _GETPC())
#endif
 */
#define GETPC() (NULL)

#elif defined(CONFIG_LLVM)

#if defined(__linux__) || defined(__MINGW32__)
extern uint64_t tcg_llvm_helper_ret_addr asm("tcg_llvm_runtime"); // XXX
#else
extern uint64_t tcg_llvm_helper_ret_addr asm("_tcg_llvm_runtime"); // XXX
#endif
#define GETPC() (execute_llvm ? (void*) tcg_llvm_helper_ret_addr : _GETPC())

#else

#define GETPC() _GETPC()

#endif

#endif /* !defined(__DYNGEN_EXEC_H__) */
