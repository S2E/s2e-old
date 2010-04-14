/* Copyright (C) 2007-2010 The Android Open Source Project
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
 * Includes common headers for the ELFF library.
 */

#ifndef ELFF_ELFF_COMMON_H_
#define ELFF_ELFF_COMMON_H_

#include "stddef.h"
#include "sys/types.h"
#include "assert.h"
#include "memory.h"
#include "errno.h"
#ifdef  WIN32
#include "windows.h"
#else   // WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif  // WIN32

static inline void _set_errno(uint32_t err) {
    errno = err;
}

/* Main operator new. We overwrite it to redirect memory
 * allocations to qemu_malloc, instead of malloc. */
inline void* operator new(size_t size) {
    return qemu_malloc(size);
}

/* Main operator delete. We overwrite it to redirect memory
 * deallocation to qemu_free, instead of free. */
inline void operator delete(void* p) {
    if (p != NULL) {
        qemu_free(p);
    }
}

/* Main operator delete for arrays. We overwrite it to redirect
 * memory deallocation to qemu_free, instead of free. */
inline void operator delete[](void* p) {
    if (p != NULL) {
        qemu_free(p);
    }
}

#endif  // ELFF_ELFF_COMMON_H_
