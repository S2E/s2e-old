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
#if !defined(CONFIG_STANDALONE_UI)
#include "telephony/modem_driver.h"
#include "trace.h"
#include "audio/audio.h"
/* Implemented in vl-android.c */
extern char* qemu_find_file(int type, const char* filename);
#endif  // CONFIG_STANDALONE_UI

extern void qemu_system_shutdown_request(void);

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
#if !defined(CONFIG_STANDALONE_UI)
    android_hw_control_init( opaque, &funcs );
#endif  // CONFIG_STANDALONE_UI
}

int
android_core_get_base_port(void)
{
#if !defined(CONFIG_STANDALONE_UI)
    return android_base_port;
#else
    return 5554;
#endif  // CONFIG_STANDALONE_UI
}

void
android_core_sensors_set_coarse_orientation( AndroidCoarseOrientation  orient )
{
#if !defined(CONFIG_STANDALONE_UI)
    android_sensors_set_coarse_orientation(orient);
#endif  // CONFIG_STANDALONE_UI
}

void
android_core_set_network_enabled(int enabled)
{
    /* Temporary implementation for the monolitic (core + ui) builds. */
#if !defined(CONFIG_STANDALONE_UI)
    if (android_modem) {
        amodem_set_data_registration(
                android_modem,
        qemu_net_disable ? A_REGISTRATION_UNREGISTERED
            : A_REGISTRATION_HOME);
    }
#endif  // CONFIG_STANDALONE_UI
}

void
android_core_toggle_network(void)
{
    /* Temporary implementation for the monolitic (core + ui) builds. */
#if !defined(CONFIG_STANDALONE_UI)
    qemu_net_disable = !qemu_net_disable;
    android_core_set_network_enabled(!qemu_net_disable);
#endif  // CONFIG_STANDALONE_UI
}

int
android_core_is_network_disabled(void)
{
    /* Temporary implementation for the monolitic (core + ui) builds. */
#if !defined(CONFIG_STANDALONE_UI)
    return qemu_net_disable;
#else
    return 0;
#endif  // CONFIG_STANDALONE_UI
}

void android_core_tracing_start(void)
{
#if !defined(CONFIG_STANDALONE_UI)
    start_tracing();
#endif  // CONFIG_STANDALONE_UI
}

void android_core_tracing_stop(void)
{
#if !defined(CONFIG_STANDALONE_UI)
    stop_tracing();
#endif  // CONFIG_STANDALONE_UI
}

int
android_core_get_android_netspeed(int index, NetworkSpeed* netspeed) {
    /* This is a temporary code used to support current behavior of the
     *monolitic (core + ui in one executable) emulator executed with
     * -help-netspeed option. In the future, when ui and core get separated,
     * behavior of help may change, and this code should be reviewed. */
#if !defined(CONFIG_STANDALONE_UI)
    if (index >= android_netspeeds_count ||
        android_netspeeds[index].name == NULL) {
        return -1;
    }
    *netspeed = android_netspeeds[index];
    return 0;
#else
    return -1;
#endif  // !CONFIG_STANDALONE_UI
}

int
android_core_get_android_netdelay(int index, NetworkLatency* delay) {
    /* This is a temporary code used to support current behavior of the
     * monolitic (core + ui in one executable) emulator executed with
     * -help-netdelays option. In the future, when ui and core get separated,
     * behavior of help may change, and this code should be reviewed. */
#if !defined(CONFIG_STANDALONE_UI)
    if (index >= android_netdelays_count ||
        android_netdelays[index].name == NULL) {
        return -1;
    }
    *delay = android_netdelays[index];
    return 0;
#else
    return -1;
#endif  // !CONFIG_STANDALONE_UI
}

int
android_core_audio_get_backend_name(int is_input, int index,
                                    char* name, size_t name_buf_size,
                                    char* descr, size_t descr_buf_size) {
    /* This is a temporary code used to support current behavior of the
     * monolitic (core + ui in one executable) emulator executed with
     * -help-audio-in, and -help-audio-in options. In the future, when ui and
     * core get separated, behavior of help may change, and this code should
     * be reviewed. */
#if !defined(CONFIG_STANDALONE_UI)
    const char* descr_ptr = NULL;
    const char* name_ptr = audio_get_backend_name(is_input, index, &descr_ptr);
    if (name_ptr == NULL) {
        return -1;
    }
    if (name != NULL && name_buf_size) {
        strncpy(name, name_ptr, name_buf_size);
        name[name_buf_size - 1] = '\0';
    }
    if (descr != NULL && descr_buf_size && descr_ptr != NULL) {
        strncpy(descr, descr_ptr, descr_buf_size);
        descr[descr_buf_size - 1] = '\0';
    }
    return 0;
#else
    return -1;
#endif  // !CONFIG_STANDALONE_UI
}

void
android_core_system_shutdown_request(void)
{
    qemu_system_shutdown_request();
}

int
android_core_qemu_find_file(int type, const char *filename,
                            char* path, size_t path_buf_size)
{
    /* Temporary implementation for the monolitic (core + ui) builds. */
#if !defined(CONFIG_STANDALONE_UI)
    char* filepath = qemu_find_file(type, filename);
    if (filepath == NULL) {
        return -1;
    }
    strncpy(path, filepath, path_buf_size);
    filepath[path_buf_size - 1] = '\0';
    qemu_free(filepath);
    return 0;
#else
    return -1;
#endif  // !CONFIG_STANDALONE_UI
}
