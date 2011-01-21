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

#ifndef _ANDROID_UI_CONTROL_CORE_H
#define _ANDROID_UI_CONTROL_CORE_H

/*
 * Contains core-side of UI control protocols. For the simplicity of the
 * implementation there are two UI control services: "ui-core control" that
 * handle UI controls initiated in the UI, and "core-ui control" that handle UI
 * controls initiated in the core. The reason for hawing two services is that
 * some of the UI controls expect the core to respond with some data. The
 * simplest way to differentiate core commands from core responses to the UI
 * commands, is to have two separate services: one sends commands only, and
 * another sends only responses.
 */

/*
 * Creates and initializes Core->UI UI control service.
 * Param:
 *  fd - Socket descriptor for the service.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int coreuictl_create(int fd);

/*
 * Destroys Core->UI UI control service.
 */
extern void coreuictl_destroy();

/* Changes the scale of the emulator window at runtime.
 * Param:
 *  scale, is_dpi - New window scale parameters
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int coreuictl_set_window_scale(double scale, int is_dpi);

/*
 * Creates and initializes UI->Core UI control instance.
 * Param:
 *  fd - Socket descriptor for the service.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int uicorectl_create(int fd);

/*
 * Destroys UI->Core UI control service.
 */
extern void uicorectl_destroy();

#endif /* _ANDROID_UI_CONTROL_CORE_H */
