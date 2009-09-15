/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
/*
 * Virtual hardware for bridging the FUSE kernel module
 * in the emulated OS and outside file system
 */
#include "qemu_file.h"
#include "goldfish_trace.h"

//#define DEBUG   1

extern void cpu_loop_exit(void);

extern int tracing;

/* for execve */
static char path[CLIENT_PAGE_SIZE];
static char arg[CLIENT_PAGE_SIZE];
static unsigned long vstart;    // VM start
static unsigned long vend;      // VM end
static unsigned long eoff;      // offset in EXE file
static unsigned cmdlen;         // cmdline length
static unsigned pid;            // PID (really thread id)
static unsigned tgid;           // thread group id (really process id)
static unsigned long dsaddr;    // dynamic symbol address
static unsigned long unmap_start; // start address to unmap

/* for context switch */
//static unsigned long cs_pid;    // context switch PID

/* I/O write */
static void trace_dev_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    trace_dev_state *s = (trace_dev_state *)opaque;

    switch (offset >> 2) {
    case TRACE_DEV_REG_SWITCH:  // context switch, switch to pid
        trace_switch(value);
#ifdef DEBUG
        printf("QEMU.trace: kernel, context switch %u\n", value);
#endif
        break;
    case TRACE_DEV_REG_TGID:    // save the tgid for the following fork/clone
        tgid = value;
#ifdef DEBUG
        printf("QEMU.trace: kernel, tgid %u\n", value);
#endif
        break;
    case TRACE_DEV_REG_FORK:    // fork, fork new pid
        trace_fork(tgid, value);
#ifdef DEBUG
        printf("QEMU.trace: kernel, fork %u\n", value);
#endif
        break;
    case TRACE_DEV_REG_CLONE:    // fork, clone new pid (i.e. thread)
        trace_clone(tgid, value);
#ifdef DEBUG
        printf("QEMU.trace: kernel, clone %u\n", value);
#endif
        break;
    case TRACE_DEV_REG_EXECVE_VMSTART:  // execve, vstart
        vstart = value;
        break;
    case TRACE_DEV_REG_EXECVE_VMEND:    // execve, vend
        vend = value;
        break;
    case TRACE_DEV_REG_EXECVE_OFFSET:   // execve, offset in EXE
        eoff = value;
        break;
    case TRACE_DEV_REG_EXECVE_EXEPATH:  // init exec, path of EXE
        vstrcpy(value, path, CLIENT_PAGE_SIZE);
        trace_init_exec(vstart, vend, eoff, path);
#ifdef DEBUG
        printf("QEMU.trace: kernel, init exec [%lx,%lx]@%lx [%s]\n", vstart, vend, eoff, path);
#endif
        path[0] = 0;
        break;
    case TRACE_DEV_REG_CMDLINE_LEN:     // execve, process cmdline length
        cmdlen = value;
        break;
    case TRACE_DEV_REG_CMDLINE:         // execve, process cmdline
        cpu_memory_rw_debug(cpu_single_env, value, arg, cmdlen, 0);
        trace_execve(arg, cmdlen);
#ifdef DEBUG
        {
            int i;
            for (i = 0; i < cmdlen; i ++)
                if (i != cmdlen - 1 && arg[i] == 0)
                    arg[i] = ' ';
            printf("QEMU.trace: kernel, execve %s[%d]\n", arg, cmdlen);
        }
#endif
        arg[0] = 0;
        break;
    case TRACE_DEV_REG_EXIT:            // exit, exit current process with exit code
        trace_exit(value);
#ifdef DEBUG
        printf("QEMU.trace: kernel, exit %x\n", value);
#endif
        break;
    case TRACE_DEV_REG_NAME:            // record thread name
        vstrcpy(value, path, CLIENT_PAGE_SIZE);

        // Remove the trailing newline if it exists
        int len = strlen(path);
        if (path[len - 1] == '\n') {
            path[len - 1] = 0;
        }
        trace_name(path);
#ifdef DEBUG
        printf("QEMU.trace: kernel, name %s\n", path);
#endif
        break;
    case TRACE_DEV_REG_MMAP_EXEPATH:    // mmap, path of EXE, the others are same as execve
        vstrcpy(value, path, CLIENT_PAGE_SIZE);
        trace_mmap(vstart, vend, eoff, path);
#ifdef DEBUG
        printf("QEMU.trace: kernel, mmap [%lx,%lx]@%lx [%s]\n", vstart, vend, eoff, path);
#endif
        path[0] = 0;
        break;
    case TRACE_DEV_REG_INIT_PID:        // init, name the pid that starts before device registered
        pid = value;
        break;
    case TRACE_DEV_REG_INIT_NAME:       // init, the comm of the init pid
        vstrcpy(value, path, CLIENT_PAGE_SIZE);
        trace_init_name(tgid, pid, path);
#ifdef DEBUG
        printf("QEMU.trace: kernel, init name %u [%s]\n", pid, path);
#endif
        path[0] = 0;
        break;

    case TRACE_DEV_REG_DYN_SYM_ADDR:    // dynamic symbol address
        dsaddr = value;
        break;
    case TRACE_DEV_REG_DYN_SYM:         // add dynamic symbol
        vstrcpy(value, arg, CLIENT_PAGE_SIZE);
        trace_dynamic_symbol_add(dsaddr, arg);
#ifdef DEBUG
        printf("QEMU.trace: dynamic symbol %lx:%s\n", dsaddr, arg);
#endif
        arg[0] = 0;
        break;
    case TRACE_DEV_REG_REMOVE_ADDR:         // remove dynamic symbol addr
        trace_dynamic_symbol_remove(value);
#ifdef DEBUG
        printf("QEMU.trace: dynamic symbol remove %lx\n", dsaddr);
#endif
        arg[0] = 0;
        break;

    case TRACE_DEV_REG_PRINT_STR:       // print string
        vstrcpy(value, arg, CLIENT_PAGE_SIZE);
        printf("%s", arg);
        arg[0] = 0;
        break;
    case TRACE_DEV_REG_PRINT_NUM_DEC:   // print number in decimal
        printf("%d", value);
        break;
    case TRACE_DEV_REG_PRINT_NUM_HEX:   // print number in hexical
        printf("%x", value);
        break;

    case TRACE_DEV_REG_STOP_EMU:        // stop the VM execution
        // To ensure that the number of instructions executed in this
        // block is correct, we pretend that there was an exception.
        trace_exception(0);

        cpu_single_env->exception_index = EXCP_HLT;
        cpu_single_env->halted = 1;
        qemu_system_shutdown_request();
        cpu_loop_exit();
        break;

    case TRACE_DEV_REG_ENABLE:          // tracing enable: 0 = stop, 1 = start
        if (value == 1)
            start_tracing();
        else if (value == 0) {
            stop_tracing();

            // To ensure that the number of instructions executed in this
            // block is correct, we pretend that there was an exception.
            trace_exception(0);
        }
        break;

    case TRACE_DEV_REG_UNMAP_START:
        unmap_start = value;
        break;
    case TRACE_DEV_REG_UNMAP_END:
        trace_munmap(unmap_start, value);
        break;

    case TRACE_DEV_REG_METHOD_ENTRY:
    case TRACE_DEV_REG_METHOD_EXIT:
    case TRACE_DEV_REG_METHOD_EXCEPTION:
    case TRACE_DEV_REG_NATIVE_ENTRY:
    case TRACE_DEV_REG_NATIVE_EXIT:
    case TRACE_DEV_REG_NATIVE_EXCEPTION:
        if (tracing) {
            int call_type = (offset - 4096) >> 2;
            trace_interpreted_method(value, call_type);
        }
        break;

    default:
        if (offset < 4096) {
            cpu_abort(cpu_single_env, "trace_dev_write: Bad offset %x\n", offset);
        }
        break;
    }
}

/* I/O read */
static uint32_t trace_dev_read(void *opaque, target_phys_addr_t offset)
{
    trace_dev_state *s = (trace_dev_state *)opaque;

    switch (offset >> 2) {
    case TRACE_DEV_REG_ENABLE:          // tracing enable
        return tracing;
    default:
        if (offset < 4096) {
            cpu_abort(cpu_single_env, "trace_dev_read: Bad offset %x\n", offset);
        }
        return 0;
    }
    return 0;
}

static CPUReadMemoryFunc *trace_dev_readfn[] = {
   trace_dev_read,
   trace_dev_read,
   trace_dev_read
};

static CPUWriteMemoryFunc *trace_dev_writefn[] = {
   trace_dev_write,
   trace_dev_write,
   trace_dev_write
};

/* initialize the trace device */
void trace_dev_init()
{
    int iomemtype;
    trace_dev_state *s;

    s = (trace_dev_state *)qemu_mallocz(sizeof(trace_dev_state));
    s->dev.name = "qemu_trace";
    s->dev.id = -1;
    s->dev.base = 0;       // will be allocated dynamically
    s->dev.size = 0x2000;
    s->dev.irq = 0;
    s->dev.irq_count = 0;

    goldfish_device_add(&s->dev, trace_dev_readfn, trace_dev_writefn, s);

    path[0] = arg[0] = '\0';
}
