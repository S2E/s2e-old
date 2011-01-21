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

#ifndef _ANDROID_UI_CONTROL_COMMON_H
#define _ANDROID_UI_CONTROL_COMMON_H

#include "android/hw-sensors.h"

/*
 * UI control requests sent by the core to the UI.
 */

/* Sets window scale. */
#define ACORE_UICTL_SET_WINDOWS_SCALE       1

/*
 * UI control requests sent by the UI to the core.
 */

/* Sets coarse orientation. */
#define AUI_UICTL_SET_COARSE_ORIENTATION    2

/* Toggles the network (no parameters). */
#define AUI_UICTL_TOGGLE_NETWORK            3

/* Starts / stops the tracing. */
#define AUI_UICTL_TRACE_CONTROL             4

/* Checks if network is disabled (no params) */
#define AUI_UICTL_CHK_NETWORK_DISABLED      5

/* Gets net speed */
#define AUI_UICTL_GET_NETSPEED              6

/* Gets net delays */
#define AUI_UICTL_GET_NETDELAY              7

/* Gets path to a QEMU file on local host. */
#define AUI_UICTL_GET_QEMU_PATH             8

/* UI control message header. */
typedef struct UICtlHeader {
    /* Message type. */
    uint8_t     msg_type;

    /* Size of the message data following this header. */
    uint32_t    msg_data_size;
} UICtlHeader;

/* UI control response header. */
typedef struct UICtlRespHeader {
    /* Result of the request handling. */
    int result;

    /* Size of the response data following this header. */
    uint32_t    resp_data_size;
} UICtlRespHeader;

/* Formats ACORE_UICTL_SET_WINDOWS_SCALE UI control request.
 */
typedef struct UICtlSetWindowsScale {
    double  scale;
    int     is_dpi;
} UICtlSetWindowsScale;

/* Formats AUI_UICTL_SET_COARSE_ORIENTATION UI control request.
 */
typedef struct UICtlSetCoarseOrientation {
    AndroidCoarseOrientation    orient;
} UICtlSetCoarseOrientation;

/* Formats AUI_UICTL_TRACE_CONTROL UI control request.
 */
typedef struct UICtlTraceControl {
    int start;
} UICtlTraceControl;

/* Formats AUI_UICTL_GET_NETSPEED UI control request.
 */
typedef struct UICtlGetNetSpeed {
    int index;
} UICtlGetNetSpeed;

/* Formats AUI_UICTL_GET_NETSPEED UI control request response.
 */
typedef struct UICtlGetNetSpeedResp {
    /* Size of the entire response structure including name and display strings. */
    int     upload;
    int     download;
    /* display field of NetworkSpeed structure is immediately following
     * this field. */
    char    name[0];
} UICtlGetNetSpeedResp;

/* Formats AUI_UICTL_GET_NETDELAY UI control request.
 */
typedef struct UICtlGetNetDelay {
    int index;
} UICtlGetNetDelay;

/* Formats AUI_UICTL_GET_NETDELAY UI control request response.
 */
typedef struct UICtlGetNetDelayResp {
    /* Size of the entire response structure including name and display strings. */
    int     min_ms;
    int     max_ms;
    /* display field of NetworkLatency structure is immediately following
     * this field. */
    char    name[0];
} UICtlGetNetDelayResp;

/* Formats AUI_UICTL_GET_QEMU_PATH UI control request.
 */
typedef struct UICtlGetQemuPath {
    int     type;
    char    filename[0];
} UICtlGetQemuPath;

/* Formats AUI_UICTL_GET_QEMU_PATH UI control request response.
 */
typedef struct UICtlGetQemuPathResp {
    /* Size of the entire response structure. */
    char    path[0];
} UICtlGetQemuPathResp;

#if 0
android_core_get_android_netspeed(int index, NetworkSpeed* netspeed) {
android_core_get_android_netdelay(int index, NetworkLatency* delay) {
int
android_core_qemu_find_file(int type, const char *filename,
                            char* path, size_t path_buf_size)
#endif

#endif /* _ANDROID_UI_CONTROL_COMMON_H */

