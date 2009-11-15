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

/* Defines size of name buffer in AKeyCharmap entry. */
#define AKEYCHARMAP_NAME_SIZE   32

typedef struct AKeyCharmap {
    const AKeyEntry*  entries;
    int               num_entries;
    char              name[ AKEYCHARMAP_NAME_SIZE ];
} AKeyCharmap;

/* Array of charmaps available in the current emulator session. */
extern const AKeyCharmap**  android_charmaps;

/* Number of entries in android_charmaps array. */
extern int                  android_charmap_count;

/* Extracts charmap name from .kcm file name.
 * Charmap name, extracted by this routine is a name of the kcm file, trimmed
 * of file name extension, and shrinked (if necessary) to fit into the name
 * buffer. Here are examples on how this routine extracts charmap name:
 * /a/path/to/kcmfile.kcm       -> kcmfile
 * /a/path/to/kcmfile.ext.kcm   -> kcmfile.ext
 * /a/path/to/kcmfile           -> kcmfile
 * /a/path/to/.kcmfile          -> kcmfile
 * /a/path/to/.kcmfile.kcm      -> .kcmfile
 * kcm_file_path - Path to key charmap file to extract charmap name from.
 * charmap_name - Buffer, where to save extracted charname.
 * max_len - charmap_name buffer size.
*/
void kcm_extract_charmap_name(const char* kcm_file_path,
                              char* charmap_name,
                              int max_len);

/* Initialzes key charmap array.
 * Key charmap array always contains two maps: one for qwerty, and
 * another for qwerty2 keyboard layout. However, a custom layout can
 * be requested with -charmap option. In tha case kcm_file_path
 * parameter contains path to a .kcm file that defines that custom
 * layout, and as the result, key charmap array will contain another
 * entry built from that file. If -charmap option was not specified,
 * kcm_file_path is NULL and final key charmap array will contain only
 * two default entries.
 * Returns a zero value on success, or -1 on failure.
*/
int android_charmap_setup(const char* kcm_file_path);

/* Cleanups initialization performed in android_charmap_setup routine. */
void android_charmap_done(void);

#endif /* _android_charmap_h */
