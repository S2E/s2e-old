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
#ifndef _android_charmap_h
#define _android_charmap_h

#include "android/keycode.h"

/* this defines a structure used to describe an Android keyboard charmap */
typedef struct AKeyEntry {
    unsigned short  code;
    unsigned short  base;
    unsigned short  caps;
    unsigned short  fn;
    unsigned short  caps_fn;
    unsigned short  number;
} AKeyEntry;

typedef struct {
    const AKeyEntry*  entries;
    int               num_entries;
    char              name[ 32 ];
} AKeyCharmap;

extern const int           android_charmap_count;
extern const AKeyCharmap*  android_charmaps[];

#endif /* _android_charmap_h */
