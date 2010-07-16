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
 * establish communication between Core and UI components of the emulator.
 * This is a temporary file where we will collect functional dependencies
 * between Core and UI in the process of separating UI and Core in the
 * emulator build.
 */

#ifndef QEMU_ANDROID_CORE_UI_PROTOCOL_H
#define QEMU_ANDROID_CORE_UI_PROTOCOL_H

/* Changes the scale of the emulator window at runtime. */
void android_ui_set_window_scale(double scale, int is_dpi);

#endif  // QEMU_ANDROID_CORE_UI_PROTOCOL_H
