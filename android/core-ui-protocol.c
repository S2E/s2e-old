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
 * This file contains helper routines that are used to establish communication
 * between Core and UI components of the emulator. This is a temporary file
 * where we will collect functional dependencies between Core and UI in the
 * process of separating UI and Core in the emulator build.
 */

#include "android/globals.h"
#include "android/core-ui-protocol.h"

extern void  android_emulator_set_window_scale( double, int );

void
android_ui_set_window_scale(double scale, int is_dpi)
{
#if !defined(CONFIG_STANDALONE_CORE)
    android_emulator_set_window_scale(scale, is_dpi);
#endif
}
