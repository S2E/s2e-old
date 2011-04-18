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
#ifndef _TRACE_DEV_H_
#define _TRACE_DEV_H_

#include "goldfish_device.h"

#define CLIENT_PAGE_SIZE        4096

/* trace device registers */
#define TRACE_DEV_REG_SWITCH            0
#define TRACE_DEV_REG_FORK              1
#define TRACE_DEV_REG_EXECVE_PID        2
#define TRACE_DEV_REG_EXECVE_VMSTART    3
#define TRACE_DEV_REG_EXECVE_VMEND      4
#define TRACE_DEV_REG_EXECVE_OFFSET     5
#define TRACE_DEV_REG_EXECVE_EXEPATH    6
#define TRACE_DEV_REG_EXIT              7
#define TRACE_DEV_REG_CMDLINE           8
#define TRACE_DEV_REG_CMDLINE_LEN       9
#define TRACE_DEV_REG_MMAP_EXEPATH      10
#define TRACE_DEV_REG_INIT_PID          11
#define TRACE_DEV_REG_INIT_NAME         12
#define TRACE_DEV_REG_CLONE             13
#define TRACE_DEV_REG_UNMAP_START       14
#define TRACE_DEV_REG_UNMAP_END         15
#define TRACE_DEV_REG_NAME              16
#define TRACE_DEV_REG_TGID              17
#define TRACE_DEV_REG_DYN_SYM           50
#define TRACE_DEV_REG_DYN_SYM_ADDR      51
#define TRACE_DEV_REG_REMOVE_ADDR       52
#define TRACE_DEV_REG_PRINT_STR         60
#define TRACE_DEV_REG_PRINT_NUM_DEC     61
#define TRACE_DEV_REG_PRINT_NUM_HEX     62
#define TRACE_DEV_REG_STOP_EMU          90
#define TRACE_DEV_REG_ENABLE            100
#define TRACE_DEV_REG_METHOD_ENTRY      1024
#define TRACE_DEV_REG_METHOD_EXIT       1025
#define TRACE_DEV_REG_METHOD_EXCEPTION  1026
#define TRACE_DEV_REG_NATIVE_ENTRY      1028
#define TRACE_DEV_REG_NATIVE_EXIT       1029
#define TRACE_DEV_REG_NATIVE_EXCEPTION  1030

/* the virtual trace device state */
typedef struct {
    struct goldfish_device dev;
} trace_dev_state;

/*
 * interfaces for copy from virtual space
 * from target-arm/op_helper.c
 */
extern void vstrcpy(target_ulong ptr, char *buf, int max);

/*
 * interfaces to trace module to signal kernel events
 */
extern void trace_switch(int pid);
extern void trace_fork(int tgid, int pid);
extern void trace_clone(int tgid, int pid);
extern void trace_execve(const char *arg, int len);
extern void trace_exit(int exitcode);
extern void trace_mmap(unsigned long vstart, unsigned long vend,
                       unsigned long offset, const char *path);
extern void trace_munmap(unsigned long vstart, unsigned long vend);
extern void trace_dynamic_symbol_add(unsigned long vaddr, const char *name);
extern void trace_dynamic_symbol_remove(unsigned long vaddr);
extern void trace_init_name(int tgid, int pid, const char *name);
extern void trace_init_exec(unsigned long start, unsigned long end,
                            unsigned long offset, const char *exe);
extern void start_tracing(void);
extern void stop_tracing(void);
extern void trace_exception(uint32 target_pc);

#endif
