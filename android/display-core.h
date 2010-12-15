/* Copyright (C) 2010 The Android Open Source Project
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
 * Contains extension to android display (see android/display.h|c) that is used
 * by the core to communicate display changes to the attached UI
 */

#ifndef _ANDROID_DISPLAY_CORE_H
#define _ANDROID_DISPLAY_CORE_H

#include "android/display.h"
#include "framebuffer.h"

/* Descriptor for a core display instance */
typedef struct CoreDisplay CoreDisplay;

/*
 * Initializes one and only one instance of a core display.
 * Param:
 *  ds - Display state to use for the core display.
 */
extern void core_display_init(DisplayState* ds);

#endif /* _ANDROID_DISPLAY_CORE_H */
