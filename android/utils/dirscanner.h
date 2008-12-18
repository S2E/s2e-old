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
#ifndef _ANDROID_UTILS_DIR_H
#define _ANDROID_UTILS_DIR_H

/* simple utility to parse directories for files            */
/* needed because Unix and Windows don't use the same stuff */

typedef struct DirScanner  DirScanner;

DirScanner*    dirScanner_new ( const char*  rootPath );
void           dirScanner_free( DirScanner*  s );
const char*    dirScanner_next( DirScanner*  s );
const char*    dirScanner_nextFull( DirScanner*  s );

#endif /* _ANDROID_UTILS_DIR_H */
