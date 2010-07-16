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
 * between UI and Core components of the emulator. This is a temporary file
 * where we will collect functional dependencies between UI and Core in the
 * process of separating UI and Core in the emulator build. Ideally at the
 * end this will be replaced with a message protocol over sockets, or other
 * means of interprocess communication.
 */

#include "android/android.h"
#include "android/globals.h"
#include "android/hw-control.h"
#include "android/ui-core-protocol.h"

int
android_core_get_hw_lcd_density(void)
{
    return android_hw->hw_lcd_density;
}

void
android_core_set_brightness_change_callback(AndroidHwLightBrightnessCallback callback,
                                            void* opaque)
{
    AndroidHwControlFuncs  funcs;

    funcs.light_brightness = callback;
    android_hw_control_init( opaque, &funcs );
}

int
android_core_get_base_port(void)
{
    return android_base_port;
}
