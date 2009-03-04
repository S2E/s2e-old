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

#ifndef _ANDROID_UTILS_DISPLAY_H
#define _ANDROID_UTILS_DISPLAY_H

/** HOST RESOLUTION SETTINGS
 **
 ** return the main monitor's DPI resolution according to the host device
 ** beware: this is not always reliable or even obtainable.
 **
 ** returns 0 on success, or -1 in case of error (e.g. the system returns funky values)
 **/
extern  int    get_monitor_resolution( int  *px_dpi, int  *py_dpi );

/** return the size in pixels of the nearest monitor for the current window.
 ** this is used to implement full-screen presentation mode.
 **/

extern  int    get_nearest_monitor_rect( int  *x, int *y, int *width, int *height );

#endif /* _ANDROID_UTILS_DISPLAY_H */
