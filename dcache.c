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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dcache.h"
#include "cpu.h"
#include "exec-all.h"
#include "trace.h"
#include "varint.h"

extern FILE *ftrace_debug;

int dcache_size = 16 * 1024;
int dcache_ways = 4;
int dcache_line_size = 32;
int dcache_replace_policy = kPolicyRandom;
int dcache_load_miss_penalty = 30;
int dcache_store_miss_penalty = 5;

typedef struct Dcache {
  int		size;
  int		ways;
  int		line_size;
  int		log_line_size;
  int		rows;
  uint32_t	addr_mask;
  int		replace_policy;
  int		next_way;
  int		extra_increment_counter;
  int		*replace;
  uint32_t	**table;
  int		load_miss_penalty;
  int		store_miss_penalty;
  uint64_t	load_hits;
  uint64_t	load_misses;
  uint64_t	store_hits;
  uint64_t	store_misses;
} Dcache;

Dcache dcache;

void dcache_cleanup();

// Returns the log2 of "num" rounded up to the nearest integer.
int log2_roundup(int num)
{
  int power2;
  int exp;

  for (exp = 0, power2 = 1; power2 < num; power2 <<= 1) {
    exp += 1;
  }
  return exp;
}

void dcache_init(int size, int ways, int line_size, int replace_policy,
                 int load_miss_penalty, int store_miss_penalty)
{
  int ii;

  // Compute the logs of the params, rounded up
  int log_size = log2_roundup(size);
  int log_ways = log2_roundup(ways);
  int log_line_size = log2_roundup(line_size);

  // The number of rows in the table = size / (line_size * ways)
  int log_rows = log_size - log_line_size - log_ways;

  dcache.size = 1 << log_size;
  dcache.ways = 1 << log_ways;
  dcache.line_size = 1 << log_line_size;
  dcache.log_line_size = log_line_size;
  dcache.rows = 1 << log_rows;
  dcache.addr_mask = (1 << log_rows) - 1;

  // Allocate an array of pointers, one for each row
  uint32_t **table = malloc(sizeof(uint32_t *) << log_rows);

  // Allocate the data for the whole cache in one call to malloc()
  int data_size = sizeof(uint32_t) << (log_rows + log_ways);
  uint32_t *data = malloc(data_size);

  // Fill the cache with invalid addresses
  memset(data, ~0, data_size);

  // Assign the pointers into the data array
  int rows = dcache.rows;
  for (ii = 0; ii < rows; ++ii) {
    table[ii] = &data[ii << log_ways];
  }
  dcache.table = table;
  dcache.replace_policy = replace_policy;
  dcache.next_way = 0;
  dcache.extra_increment_counter = 0;

  dcache.replace = NULL;
  if (replace_policy == kPolicyRoundRobin) {
    dcache.replace = malloc(sizeof(int) << log_rows);
    memset(dcache.replace, 0, sizeof(int) << log_rows);
  }
  dcache.load_miss_penalty = load_miss_penalty;
  dcache.store_miss_penalty = store_miss_penalty;
  dcache.load_hits = 0;
  dcache.load_misses = 0;
  dcache.store_hits = 0;
  dcache.store_misses = 0;

  atexit(dcache_cleanup);
}

void dcache_stats()
{
  uint64_t hits = dcache.load_hits + dcache.store_hits;
  uint64_t misses = dcache.load_misses + dcache.store_misses;
  uint64_t total = hits + misses;
  double hit_per = 0;
  double miss_per = 0;
  if (total) {
    hit_per = 100.0 * hits / total;
    miss_per = 100.0 * misses / total;
  }
  printf("\n");
  printf("Dcache hits   %10llu %6.2f%%\n", hits, hit_per);
  printf("Dcache misses %10llu %6.2f%%\n", misses, miss_per);
  printf("Dcache total  %10llu\n", hits + misses);
}

void dcache_free()
{
  free(dcache.table[0]);
  free(dcache.table);
  free(dcache.replace);
  dcache.table = NULL;
}

void dcache_cleanup()
{
  dcache_stats();
  dcache_free();
}

void compress_trace_addresses(TraceAddr *trace_addr)
{
  AddrRec *ptr;
  char *comp_ptr = trace_addr->compressed_ptr;
  uint32_t prev_addr = trace_addr->prev_addr;
  uint64_t prev_time = trace_addr->prev_time;
  AddrRec *last = &trace_addr->buffer[kMaxNumAddrs];
  for (ptr = trace_addr->buffer; ptr != last; ++ptr) {
    if (comp_ptr >= trace_addr->high_water_ptr) {
      uint32_t size = comp_ptr - trace_addr->compressed;
      fwrite(trace_addr->compressed, sizeof(char), size, trace_addr->fstream);
      comp_ptr = trace_addr->compressed;
    }

    int addr_diff = ptr->addr - prev_addr;
    uint64_t time_diff = ptr->time - prev_time;
    prev_addr = ptr->addr;
    prev_time = ptr->time;

    comp_ptr = varint_encode_signed(addr_diff, comp_ptr);
    comp_ptr = varint_encode(time_diff, comp_ptr);
  }
  trace_addr->compressed_ptr = comp_ptr;
  trace_addr->prev_addr = prev_addr;
  trace_addr->prev_time = prev_time;
}

// This function is called by the generated code to simulate
// a dcache load access.
void dcache_load(uint32_t addr)
{
  int ii;
  int ways = dcache.ways;
  uint32_t cache_addr = addr >> dcache.log_line_size;
  int row = cache_addr & dcache.addr_mask;
  //printf("ld %lld 0x%x\n", sim_time, addr);
  for (ii = 0; ii < ways; ++ii) {
    if (cache_addr == dcache.table[row][ii]) {
      dcache.load_hits += 1;
#if 0
      printf("dcache load hit  addr: 0x%x cache_addr: 0x%x row %d way %d\n",
             addr, cache_addr, row, ii);
#endif
      // If we are tracing all addresses, then include this in the trace.
      if (trace_all_addr) {
        AddrRec *next = trace_load.next;
        next->addr = addr;
        next->time = sim_time;
        next += 1;
        if (next == &trace_load.buffer[kMaxNumAddrs]) {
          // Compress the trace
          compress_trace_addresses(&trace_load);
          next = &trace_load.buffer[0];
        }
        trace_load.next = next;
      }
      return;
    }
  }
  // This is a cache miss

#if 0
  if (ftrace_debug)
    fprintf(ftrace_debug, "t%lld %08x\n", sim_time, addr);
#endif
  if (trace_load.fstream) {
    AddrRec *next = trace_load.next;
    next->addr = addr;
    next->time = sim_time;
    next += 1;
    if (next == &trace_load.buffer[kMaxNumAddrs]) {
      // Compress the trace
      compress_trace_addresses(&trace_load);
      next = &trace_load.buffer[0];
    }
    trace_load.next = next;
  }

  dcache.load_misses += 1;
  sim_time += dcache.load_miss_penalty;

  // Pick a way to replace
  int way;
  if (dcache.replace_policy == kPolicyRoundRobin) {
    // Round robin replacement policy
    way = dcache.replace[row];
    int next_way = way + 1;
    if (next_way == dcache.ways)
      next_way = 0;
    dcache.replace[row] = next_way;
  } else {
    // Random replacement policy
    way = dcache.next_way;
    dcache.next_way += 1;
    if (dcache.next_way >= dcache.ways)
      dcache.next_way = 0;

    // Every 13 replacements, add an extra increment to the next way
    dcache.extra_increment_counter += 1;
    if (dcache.extra_increment_counter == 13) {
      dcache.extra_increment_counter = 0;
      dcache.next_way += 1;
      if (dcache.next_way >= dcache.ways)
        dcache.next_way = 0;
    }
  }
#if 0
  printf("dcache load miss addr: 0x%x cache_addr: 0x%x row %d replacing way %d\n",
         addr, cache_addr, row, way);
#endif
  dcache.table[row][way] = cache_addr;
}

// This function is called by the generated code to simulate
// a dcache store access.
void dcache_store(uint32_t addr, uint32_t val)
{
  //printf("st %lld 0x%08x val 0x%x\n", sim_time, addr, val);

  int ii;
  int ways = dcache.ways;
  uint32_t cache_addr = addr >> dcache.log_line_size;
  int row = cache_addr & dcache.addr_mask;
  for (ii = 0; ii < ways; ++ii) {
    if (cache_addr == dcache.table[row][ii]) {
      dcache.store_hits += 1;
#if 0
      printf("dcache store hit  addr: 0x%x cache_addr: 0x%x row %d way %d\n",
             addr, cache_addr, row, ii);
#endif
      // If we are tracing all addresses, then include this in the trace.
      if (trace_all_addr) {
        AddrRec *next = trace_store.next;
        next->addr = addr;
        next->time = sim_time;
        next += 1;
        if (next == &trace_store.buffer[kMaxNumAddrs]) {
          // Compress the trace
          compress_trace_addresses(&trace_store);
          next = &trace_store.buffer[0];
        }
        trace_store.next = next;
      }
      return;
    }
  }
  // This is a cache miss
#if 0
  printf("dcache store miss addr: 0x%x cache_addr: 0x%x row %d\n",
         addr, cache_addr, row);
#endif

#if 0
  if (ftrace_debug)
    fprintf(ftrace_debug, "t%lld %08x\n", sim_time, addr);
#endif

  if (trace_store.fstream) {
    AddrRec *next = trace_store.next;
    next->addr = addr;
    next->time = sim_time;
    next += 1;
    if (next == &trace_store.buffer[kMaxNumAddrs]) {
      // Compress the trace
      compress_trace_addresses(&trace_store);
      next = &trace_store.buffer[0];
    }
    trace_store.next = next;
  }

  dcache.store_misses += 1;
  sim_time += dcache.store_miss_penalty;

  // Assume no write-allocate for now
}

// This function is called by the generated code to simulate
// a dcache load and store (swp) access.
void dcache_swp(uint32_t addr)
{
  dcache_load(addr);
  dcache_store(addr, 0);
}
