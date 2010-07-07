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
 * This file contains declarations of helper routines that are used to
 * establish communication between UI and Core components of the emulator.
 * This is a temporary file where we will collect functional dependencies
 * between UI and Core in the process of separating UI and Core in the
 * emulator build.
 */

#ifndef QEMU_ANDROID_UI_CORE_PROTOCOL_H
#define QEMU_ANDROID_UI_CORE_PROTOCOL_H

/* Gets LCD density property from the core properties. */
int android_core_get_hw_lcd_density(void);

#endif  // QEMU_ANDROID_UI_CORE_PROTOCOL_H
