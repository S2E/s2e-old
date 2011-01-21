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

#ifndef _ANDROID_UI_CONTROL_UI_H
#define _ANDROID_UI_CONTROL_UI_H

/*
 * Contains UI-side of UI control protocols. For the simplicity of implementation
 * there are two UI control services: "ui-core control" that handle UI controls
 * initiated in the UI, and "core-ui control" that handle UI controls initiated
 * in the core. The reason for hawing two services is that some of the UI
 * controls expect the core to respond with some data. The simplest way to
 * differentiate core commands from core responses to the UI commands, is to have
 * two separate services: one sends commands only, and another sends only
 * responses.
 */

#include "sockets.h"
#include "android/ui-ctl-common.h"

/* Establishes connection with UI control services in the core.
 * Param:
 *  console_socket Core's console socket.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_create(SockAddress* console_socket);

/*
 * UI->Core API
 */

/* Sends AUI_UICTL_SET_COARSE_ORIENTATION message to the core.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_set_coarse_orientation(AndroidCoarseOrientation orient);

/* Sends AUI_UICTL_TOGGLE_NETWORK message to the core.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_toggle_network();

/* Sends AUI_UICTL_TRACE_CONTROL message to the core.
 * Param:
 *  start - Starts (> 0), or stops (== 0) tracing.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_trace_control(int start);

/* Sends AUI_UICTL_CHK_NETWORK_DISABLED message to the core.
 * Return:
 *  0 if network is enabled, 1 if it is disabled, or < 0 on failure.
 */
int clientuictl_check_network_disabled();

/* Sends AUI_UICTL_GET_NETSPEED message to the core.
 * Param:
 *  index - Index of an entry in the NetworkSpeed array.
 *  netspeed - Upon success contains allocated and initialized NetworkSpeed
 *  instance for the given index. Note that strings addressed by "name" and
 *  "display" fileds in the returned NetworkSpeed instance are containd inside
 *  the buffer allocated for the returned NetworkSpeed instance. Caller of this
 *  routine must eventually free the buffer returned in this parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_get_netspeed(int index, NetworkSpeed** netspeed);

/* Sends AUI_UICTL_GET_NETDELAY message to the core.
 * Param:
 *  index - Index of an entry in the NetworkLatency array.
 *  netdelay - Upon success contains allocated and initialized NetworkLatency
 *  instance for the given index. Note that strings addressed by "name" and
 *  "display" fileds in the returned NetworkLatency instance are containd inside
 *  the buffer allocated for the returned NetworkLatency instance. Caller of this
 *  routine must eventually free the buffer returned in this parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_get_netdelay(int index, NetworkLatency** netdelay);

/* Sends AUI_UICTL_GET_QEMU_PATH message to the core.
 * Param:
 *  type, filename - Query parameters
 *  netdelay - Upon success contains allocated and initialized NetworkLatency
 *  instance for the given index. Note that strings addressed by "name" and
 *  "display" fileds in the returned NetworkLatency instance are containd inside
 *  the buffer allocated for the returned NetworkLatency instance. Caller of this
 *  routine must eventually free the buffer returned in this parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
int clientuictl_get_qemu_path(int type, const char* filename, char** path);

#endif /* _ANDROID_UI_CONTROL_UI_H */

