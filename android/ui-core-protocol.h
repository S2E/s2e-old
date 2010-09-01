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

#include "android/hw-sensors.h"

/* Gets LCD density property from the core properties. */
int android_core_get_hw_lcd_density(void);

/* This is temporary redeclaration for AndroidHwLightBrightnessFunc declared
 * in android/hw-control.h We redeclare it here in order to keep type
 * consistency between android_core_set_brightness_change_callback and
 * light_brightness field of AndroidHwControlFuncs structure.
 */
typedef void  (*AndroidHwLightBrightnessCallback)( void*       opaque,
                                               const char* light,
                                               int         brightness );

/* Registers a UI callback to be called when brightness is changed by the core. */
void android_core_set_brightness_change_callback(AndroidHwLightBrightnessCallback callback,
                                                 void* opaque);

/* Returns base port assigned for the emulated system. */
int android_core_get_base_port(void);

/* change the coarse orientation value */
void  android_core_sensors_set_coarse_orientation( AndroidCoarseOrientation  orient );

/* Sets the network state */
void android_core_set_network_enabled(int enabled);

/* Toggles the network state */
void android_core_toggle_network(void);

/* Gets the network state */
int android_core_is_network_disabled(void);

/* Start/stop tracing in the guest system */
void android_core_tracing_start(void);
void android_core_tracing_stop(void);

/* Gets an entry in android_netspeeds array defined in net-android.c
 * Parameters:
 *  index - Index of the entry to get from the array.
 *  netspeed - Upon successful return contains copy of the requested entry.
 * Return:
 *  0 on success, or -1 if requested entry index is too large.
 */
int android_core_get_android_netspeed(int index, NetworkSpeed* netspeed);

/* Gets an entry in android_netdelays array defined in net-android.c
 * Parameters:
 *  index - Index of the entry to get from the array.
 *  netspeed - Upon successful return contains copy of the requested entry.
 * Return:
 *  0 on success, or -1 if requested entry index is too large.
 */
int android_core_get_android_netdelay(int index, NetworkLatency* delay);

/* Get name of a given audio backend.
 * Parameters
 *  is_input - If 1, routine should lookup for input audio backend, if zero,
 *      routine should lookup for output audio backend.
 *  index - Index of the registered audio backend to lookup.
 *  name - Upon successful return contains backend name.
 *  name_buf_size - name buffer size (in characters).
 *  descr - Upon successful return contains backend description.
 *  descr_buf_size - descre buffer size (in characters).
 * Return:
 *  0 on success, or -1 if requested backend has not been found.
 */
int android_core_audio_get_backend_name(int is_input, int index,
                                        char* name, size_t name_buf_size,
                                        char* descr, size_t descr_buf_size);

/* Notifies the core about system shutdown requested by the UI. */
void android_core_system_shutdown_request(void);

/* Builds a path to a file of the given type in the emulator's data directory.
 * Param:
 *  type - Type of the file to find. Only QEMU_FILE_TYPE_BIOS, and
 *      QEMU_FILE_TYPE_KEYMAP are allowed for this value.
 *  filename - Name of the file to build path for.
 *  path - Upon success contains path to the requested file inside the
 *      emulator's data directory.
 *  path_buf_size Character size of the buffer addressed by the path parameter.
 * Return:
 *  0 on success, or -1 on an error.
 */
int
android_core_qemu_find_file(int type, const char *filename,
                            char* path, size_t path_buf_size);

#endif  // QEMU_ANDROID_UI_CORE_PROTOCOL_H
