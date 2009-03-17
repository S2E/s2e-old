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
#ifndef DCACHE_H
#define DCACHE_H

#include <inttypes.h>

// Define constants for the replacement policies
#define kPolicyRoundRobin 1
#define kPolicyRandom 2

extern int dcache_size;
extern int dcache_ways;
extern int dcache_line_size;
extern int dcache_replace_policy;
extern int dcache_load_miss_penalty;
extern int dcache_store_miss_penalty;

extern void dcache_init(int size, int ways, int line_size, int replace_policy,
                        int load_miss_penalty, int store_miss_penalty);

#endif /* DCACHE_H */
