/* Copyright (C) 2006-2007 The Android Open Source Project
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

#ifndef TRACE_H
#define TRACE_H

#include <inttypes.h>
#include "trace_common.h"

extern uint64_t start_time, end_time;
extern uint64_t elapsed_usecs;
extern uint64 Now();

struct TranslationBlock;

// For tracing dynamic execution of basic blocks
typedef struct TraceBB {
    char	*filename;
    FILE	*fstream;
    BBRec	buffer[kMaxNumBasicBlocks];
    BBRec	*next;		// points to next record in buffer
    uint64_t	flush_time;	// time of last buffer flush
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    char	*high_water_ptr;
    int64_t	prev_bb_num;
    uint64_t	prev_bb_time;
    uint64_t	current_bb_num;
    uint64_t	current_bb_start_time;
    uint64_t	recnum;		// counts number of trace records
    uint32_t	current_bb_addr;
    int		num_insns;
} TraceBB;

// For tracing simuation start times of instructions
typedef struct TraceInsn {
    char	*filename;
    FILE	*fstream;
    InsnRec	dummy;		// this is here so we can use buffer[-1]
    InsnRec	buffer[kInsnBufferSize];
    InsnRec	*current;
    uint64_t	prev_time;	// time of last instruction start
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    char	*high_water_ptr;
} TraceInsn;

// For tracing the static information about a basic block
typedef struct TraceStatic {
    char	*filename;
    FILE	*fstream;
    uint32_t	insns[kMaxInsnPerBB];
    int		next_insn;
    uint64_t	bb_num;
    uint32_t	bb_addr;
    int		is_thumb;
} TraceStatic;

// For tracing load and store addresses
typedef struct TraceAddr {
    char	*filename;
    FILE	*fstream;
    AddrRec	buffer[kMaxNumAddrs];
    AddrRec	*next;
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    char	*high_water_ptr;
    uint32_t	prev_addr;
    uint64_t	prev_time;
} TraceAddr;

// For tracing exceptions
typedef struct TraceExc {
    char	*filename;
    FILE	*fstream;
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    char	*high_water_ptr;
    uint64_t	prev_time;
    uint64_t	prev_bb_recnum;
} TraceExc;

// For tracing process id changes
typedef struct TracePid {
    char	*filename;
    FILE	*fstream;
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    uint64_t	prev_time;
} TracePid;

// For tracing Dalvik VM method enter and exit
typedef struct TraceMethod {
    char	*filename;
    FILE	*fstream;
    char	compressed[kCompressedSize];
    char	*compressed_ptr;
    uint64_t	prev_time;
    uint32_t	prev_addr;
    int32_t	prev_pid;
} TraceMethod;

extern TraceBB trace_bb;
extern TraceInsn trace_insn;
extern TraceStatic trace_static;
extern TraceAddr trace_load;
extern TraceAddr trace_store;
extern TraceExc trace_exc;
extern TracePid trace_pid;
extern TraceMethod trace_method;

// The simulated time, in clock ticks, starting with one.
extern uint64_t sim_time;

// This variable == 1 if we are currently tracing, otherwise == 0.
extern int tracing;
extern int trace_all_addr;
extern int trace_cache_miss;

extern void start_tracing();
extern void stop_tracing();
extern void trace_init(const char *filename);
extern void trace_bb_start(uint32_t bb_addr);
extern void trace_add_insn(uint32_t insn, int is_thumb);
extern void trace_bb_end();

extern int get_insn_ticks_arm(uint32_t insn);
extern int get_insn_ticks_thumb(uint32_t  insn);

extern void trace_exception(uint32 pc);
extern void trace_bb_helper(uint64_t bb_num, TranslationBlock *tb);
extern void trace_insn_helper();
extern void sim_dcache_load(uint32_t addr);
extern void sim_dcache_store(uint32_t addr, uint32_t val);
extern void sim_dcache_swp(uint32_t addr);
extern void trace_interpreted_method(uint32_t addr, int call_type);

extern const char *trace_filename;
extern int tracing;
extern int trace_cache_miss;
extern int trace_all_addr;

#endif /* TRACE_H */
