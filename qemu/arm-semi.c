/*
 *  Arm "Angel" semihosting syscalls
 *
 *  Copyright (c) 2005, 2007 CodeSourcery.
 *  Written by Paul Brook.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "cpu.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"

#define ARM_ANGEL_HEAP_SIZE (128 * 1024 * 1024)
#else
#include "qemu-common.h"
#include "gdbstub.h"
#include "hw/arm-misc.h"
#endif

#define SYS_OPEN        0x01
#define SYS_CLOSE       0x02
#define SYS_WRITEC      0x03
#define SYS_WRITE0      0x04
#define SYS_WRITE       0x05
#define SYS_READ        0x06
#define SYS_READC       0x07
#define SYS_ISTTY       0x09
#define SYS_SEEK        0x0a
#define SYS_FLEN        0x0c
#define SYS_TMPNAM      0x0d
#define SYS_REMOVE      0x0e
#define SYS_RENAME      0x0f
#define SYS_CLOCK       0x10
#define SYS_TIME        0x11
#define SYS_SYSTEM      0x12
#define SYS_ERRNO       0x13
#define SYS_GET_CMDLINE 0x15
#define SYS_HEAPINFO    0x16
#define SYS_EXIT        0x18

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define GDB_O_RDONLY  0x000
#define GDB_O_WRONLY  0x001
#define GDB_O_RDWR    0x002
#define GDB_O_APPEND  0x008
#define GDB_O_CREAT   0x200
#define GDB_O_TRUNC   0x400
#define GDB_O_BINARY  0

static int gdb_open_modeflags[12] = {
    GDB_O_RDONLY,
    GDB_O_RDONLY | GDB_O_BINARY,
    GDB_O_RDWR,
    GDB_O_RDWR | GDB_O_BINARY,
    GDB_O_WRONLY | GDB_O_CREAT | GDB_O_TRUNC,
    GDB_O_WRONLY | GDB_O_CREAT | GDB_O_TRUNC | GDB_O_BINARY,
    GDB_O_RDWR | GDB_O_CREAT | GDB_O_TRUNC,
    GDB_O_RDWR | GDB_O_CREAT | GDB_O_TRUNC | GDB_O_BINARY,
    GDB_O_WRONLY | GDB_O_CREAT | GDB_O_APPEND,
    GDB_O_WRONLY | GDB_O_CREAT | GDB_O_APPEND | GDB_O_BINARY,
    GDB_O_RDWR | GDB_O_CREAT | GDB_O_APPEND,
    GDB_O_RDWR | GDB_O_CREAT | GDB_O_APPEND | GDB_O_BINARY
};

static int open_modeflags[12] = {
    O_RDONLY,
    O_RDONLY | O_BINARY,
    O_RDWR,
    O_RDWR | O_BINARY,
    O_WRONLY | O_CREAT | O_TRUNC,
    O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
    O_RDWR | O_CREAT | O_TRUNC,
    O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
    O_WRONLY | O_CREAT | O_APPEND,
    O_WRONLY | O_CREAT | O_APPEND | O_BINARY,
    O_RDWR | O_CREAT | O_APPEND,
    O_RDWR | O_CREAT | O_APPEND | O_BINARY
};

#ifdef CONFIG_USER_ONLY
static inline uint32_t set_swi_errno(TaskState *ts, uint32_t code)
{
    if (code == (uint32_t)-1)
        ts->swi_errno = errno;
    return code;
}
#else
static inline uint32_t set_swi_errno(CPUARMState *env, uint32_t code)
{
    return code;
}

#include "softmmu-semi.h"
#endif

static target_ulong arm_semi_syscall_len;

#if !defined(CONFIG_USER_ONLY)
static target_ulong syscall_err;
#endif

static void arm_semi_cb(CPUARMState *env, target_ulong ret, target_ulong err)
{
#ifdef CONFIG_USER_ONLY
    TaskState *ts = env->opaque;
#endif

    if (ret == (target_ulong)-1) {
#ifdef CONFIG_USER_ONLY
        ts->swi_errno = err;
#else
	syscall_err = err;
#endif
        WR_cpu(env,regs[0],ret);
    } else {
        /* Fixup syscalls that use nonstardard return conventions.  */
        switch (RR_cpu(env,regs[0])) {
        case SYS_WRITE:
        case SYS_READ:
            WR_cpu(env,regs[0],arm_semi_syscall_len - ret);
            break;
        case SYS_SEEK:
            WR_cpu(env,regs[0],0);
            break;
        default:
            WR_cpu(env,regs[0],ret);
            break;
        }
    }
}

static void arm_semi_flen_cb(CPUARMState *env, target_ulong ret, target_ulong err)
{
    /* The size is always stored in big-endian order, extract
       the value. We assume the size always fit in 32 bits.  */
    uint32_t size;
    cpu_memory_rw_debug(env, (RR_cpu(env,regs[13])-64+32), (uint8_t *)&size, 4, 0);
    WR_cpu(env,regs[0],be32_to_cpu(size));
#ifdef CONFIG_USER_ONLY
    ((TaskState *)env->opaque)->swi_errno = err;
#else
    syscall_err = err;
#endif
}

#define ARG(n)					\
({						\
    target_ulong __arg;				\
    /* FIXME - handle get_user() failure */	\
    get_user_ual(__arg, args + (n) * 4);	\
    __arg;					\
})
#define SET_ARG(n, val) put_user_ual(val, args + (n) * 4)
uint32_t do_arm_semihosting(CPUARMState *env)
{
    target_ulong args;
    char * s;
    int nr;
    uint32_t ret;
    uint32_t len;
#ifdef CONFIG_USER_ONLY
    TaskState *ts = env->opaque;
#else
    CPUARMState *ts = env;
#endif

    nr = RR_cpu(env,regs[0]);
    args = RR_cpu(env,regs[1]);
    switch (nr) {
    case SYS_OPEN:
        if (!(s = lock_user_string(ARG(0))))
            /* FIXME - should this error code be -TARGET_EFAULT ? */
            return (uint32_t)-1;
        if (ARG(1) >= 12)
            return (uint32_t)-1;
        if (strcmp(s, ":tt") == 0) {
            if (ARG(1) < 4)
                return STDIN_FILENO;
            else
                return STDOUT_FILENO;
        }
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "open,%s,%x,1a4", ARG(0),
			   (int)ARG(2)+1, gdb_open_modeflags[ARG(1)]);
            return RR_cpu(env,regs[0]);
        } else {
            ret = set_swi_errno(ts, open(s, open_modeflags[ARG(1)], 0644));
        }
        unlock_user(s, ARG(0), 0);
        return ret;
    case SYS_CLOSE:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "close,%x", ARG(0));
            return RR_cpu(env,regs[0]);
        } else {
            return set_swi_errno(ts, close(ARG(0)));
        }
    case SYS_WRITEC:
        {
          char c;

          if (get_user_u8(c, args))
              /* FIXME - should this error code be -TARGET_EFAULT ? */
              return (uint32_t)-1;
          /* Write to debug console.  stderr is near enough.  */
          if (use_gdb_syscalls()) {
                gdb_do_syscall(arm_semi_cb, "write,2,%x,1", args);
                return RR_cpu(env,regs[0]);
          } else {
                return write(STDERR_FILENO, &c, 1);
          }
        }
    case SYS_WRITE0:
        if (!(s = lock_user_string(args)))
            /* FIXME - should this error code be -TARGET_EFAULT ? */
            return (uint32_t)-1;
        len = strlen(s);
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "write,2,%x,%x\n", args, len);
            ret = RR_cpu(env,regs[0]);
        } else {
            ret = write(STDERR_FILENO, s, len);
        }
        unlock_user(s, args, 0);
        return ret;
    case SYS_WRITE:
        len = ARG(2);
        if (use_gdb_syscalls()) {
            arm_semi_syscall_len = len;
            gdb_do_syscall(arm_semi_cb, "write,%x,%x,%x", ARG(0), ARG(1), len);
            return RR_cpu(env,regs[0]);
        } else {
            if (!(s = lock_user(VERIFY_READ, ARG(1), len, 1)))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            ret = set_swi_errno(ts, write(ARG(0), s, len));
            unlock_user(s, ARG(1), 0);
            if (ret == (uint32_t)-1)
                return -1;
            return len - ret;
        }
    case SYS_READ:
        len = ARG(2);
        if (use_gdb_syscalls()) {
            arm_semi_syscall_len = len;
            gdb_do_syscall(arm_semi_cb, "read,%x,%x,%x", ARG(0), ARG(1), len);
            return RR_cpu(env,regs[0]);
        } else {
            if (!(s = lock_user(VERIFY_WRITE, ARG(1), len, 0)))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            do
              ret = set_swi_errno(ts, read(ARG(0), s, len));
            while (ret == -1 && errno == EINTR);
            unlock_user(s, ARG(1), len);
            if (ret == (uint32_t)-1)
                return -1;
            return len - ret;
        }
    case SYS_READC:
       /* XXX: Read from debug cosole. Not implemented.  */
        return 0;
    case SYS_ISTTY:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "isatty,%x", ARG(0));
            return RR_cpu(env,regs[0]);
        } else {
            return isatty(ARG(0));
        }
    case SYS_SEEK:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "lseek,%x,%x,0", ARG(0), ARG(1));
            return RR_cpu(env,regs[0]);
        } else {
            ret = set_swi_errno(ts, lseek(ARG(0), ARG(1), SEEK_SET));
            if (ret == (uint32_t)-1)
              return -1;
            return 0;
        }
    case SYS_FLEN:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_flen_cb, "fstat,%x,%x",
			   ARG(0), (RR_cpu(env,regs[1])-64));
            return RR_cpu(env,regs[0]);
        } else {
            struct stat buf;
            ret = set_swi_errno(ts, fstat(ARG(0), &buf));
            if (ret == (uint32_t)-1)
                return -1;
            return buf.st_size;
        }
    case SYS_TMPNAM:
        /* XXX: Not implemented.  */
        return -1;
    case SYS_REMOVE:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "unlink,%s", ARG(0), (int)ARG(1)+1);
            ret = RR_cpu(env,regs[0]);
        } else {
            if (!(s = lock_user_string(ARG(0))))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            ret =  set_swi_errno(ts, remove(s));
            unlock_user(s, ARG(0), 0);
        }
        return ret;
    case SYS_RENAME:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "rename,%s,%s",
                           ARG(0), (int)ARG(1)+1, ARG(2), (int)ARG(3)+1);
            return RR_cpu(env,regs[0]);
        } else {
            char *s2;
            s = lock_user_string(ARG(0));
            s2 = lock_user_string(ARG(2));
            if (!s || !s2)
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                ret = (uint32_t)-1;
            else
                ret = set_swi_errno(ts, rename(s, s2));
            if (s2)
                unlock_user(s2, ARG(2), 0);
            if (s)
                unlock_user(s, ARG(0), 0);
            return ret;
        }
    case SYS_CLOCK:
        return clock() / (CLOCKS_PER_SEC / 100);
    case SYS_TIME:
        return set_swi_errno(ts, time(NULL));
    case SYS_SYSTEM:
        if (use_gdb_syscalls()) {
            gdb_do_syscall(arm_semi_cb, "system,%s", ARG(0), (int)ARG(1)+1);
            return RR_cpu(env,regs[0]);
        } else {
            if (!(s = lock_user_string(ARG(0))))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            ret = set_swi_errno(ts, system(s));
            unlock_user(s, ARG(0), 0);
            return ret;
        }
    case SYS_ERRNO:
#ifdef CONFIG_USER_ONLY
        return ts->swi_errno;
#else
        return syscall_err;
#endif
    case SYS_GET_CMDLINE:
        {
            /* Build a command-line from the original argv.
             *
             * The inputs are:
             *     * ARG(0), pointer to a buffer of at least the size
             *               specified in ARG(1).
             *     * ARG(1), size of the buffer pointed to by ARG(0) in
             *               bytes.
             *
             * The outputs are:
             *     * ARG(0), pointer to null-terminated string of the
             *               command line.
             *     * ARG(1), length of the string pointed to by ARG(0).
             */

            char *output_buffer;
            size_t input_size = ARG(1);
            size_t output_size;
            int status = 0;

            /* Compute the size of the output string.  */
#if !defined(CONFIG_USER_ONLY)
            output_size = strlen(ts->boot_info->kernel_filename)
                        + 1  /* Separating space.  */
                        + strlen(ts->boot_info->kernel_cmdline)
                        + 1; /* Terminating null byte.  */
#else
            unsigned int i;

            output_size = ts->info->arg_end - ts->info->arg_start;
            if (!output_size) {
                /* We special-case the "empty command line" case (argc==0).
                   Just provide the terminating 0. */
                output_size = 1;
            }
#endif

            if (output_size > input_size) {
                 /* Not enough space to store command-line arguments.  */
                return -1;
            }

            /* Adjust the command-line length.  */
            SET_ARG(1, output_size - 1);

            /* Lock the buffer on the ARM side.  */
            output_buffer = lock_user(VERIFY_WRITE, ARG(0), output_size, 0);
            if (!output_buffer) {
                return -1;
            }

            /* Copy the command-line arguments.  */
#if !defined(CONFIG_USER_ONLY)
            pstrcpy(output_buffer, output_size, ts->boot_info->kernel_filename);
            pstrcat(output_buffer, output_size, " ");
            pstrcat(output_buffer, output_size, ts->boot_info->kernel_cmdline);
#else
            if (output_size == 1) {
                /* Empty command-line.  */
                output_buffer[0] = '\0';
                goto out;
            }

            if (copy_from_user(output_buffer, ts->info->arg_start,
                               output_size)) {
                status = -1;
                goto out;
            }

            /* Separate arguments by white spaces.  */
            for (i = 0; i < output_size - 1; i++) {
                if (output_buffer[i] == 0) {
                    output_buffer[i] = ' ';
                }
            }
        out:
#endif
            /* Unlock the buffer on the ARM side.  */
            unlock_user(output_buffer, ARG(0), output_size);

            return status;
        }
    case SYS_HEAPINFO:
        {
            uint32_t *ptr;
            uint32_t limit;

#ifdef CONFIG_USER_ONLY
            /* Some C libraries assume the heap immediately follows .bss, so
               allocate it using sbrk.  */
            if (!ts->heap_limit) {
                abi_ulong ret;

                ts->heap_base = do_brk(0);
                limit = ts->heap_base + ARM_ANGEL_HEAP_SIZE;
                /* Try a big heap, and reduce the size if that fails.  */
                for (;;) {
                    ret = do_brk(limit);
                    if (ret >= limit) {
                        break;
                    }
                    limit = (ts->heap_base >> 1) + (limit >> 1);
                }
                ts->heap_limit = limit;
            }

            if (!(ptr = lock_user(VERIFY_WRITE, ARG(0), 16, 0)))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            ptr[0] = tswap32(ts->heap_base);
            ptr[1] = tswap32(ts->heap_limit);
            ptr[2] = tswap32(ts->stack_base);
            ptr[3] = tswap32(0); /* Stack limit.  */
            unlock_user(ptr, ARG(0), 16);
#else
            limit = ram_size;
            if (!(ptr = lock_user(VERIFY_WRITE, ARG(0), 16, 0)))
                /* FIXME - should this error code be -TARGET_EFAULT ? */
                return (uint32_t)-1;
            /* TODO: Make this use the limit of the loaded application.  */
            ptr[0] = tswap32(limit / 2);
            ptr[1] = tswap32(limit);
            ptr[2] = tswap32(limit); /* Stack base */
            ptr[3] = tswap32(0); /* Stack limit.  */
            unlock_user(ptr, ARG(0), 16);
#endif
            return 0;
        }
    case SYS_EXIT:
        gdb_exit(env, 0);
        exit(0);
    default:
        fprintf(stderr, "qemu: Unsupported SemiHosting SWI 0x%02x\n", nr);
        cpu_dump_state(env, stderr, fprintf, 0);
        abort();
    }
}
