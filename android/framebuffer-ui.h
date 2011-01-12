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
 * Contains UI-side framebuffer client that receives framebuffer updates
 * from the core.
 */

#ifndef _ANDROID_FRAMEBUFFER_UI_H
#define _ANDROID_FRAMEBUFFER_UI_H

#include "console.h"
#include "framebuffer.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/core-connection.h"

/* Descriptor for the framebuffer client. */
typedef struct ClientFramebuffer ClientFramebuffer;

/*
 * Creates framebuffer client, and connects it with the core.
 * Param:
 *  console_socket Address of the core's console socket.
 *  protocol Protocol to use for the updates:
 *      -raw Stream pixels over socket
 *      -shared Use shared memory for pixels.
 * fb - Framebuffer associated with this FB client.
 * Return:
 *  Descriptor for the framebuffer client on success, or NULL on failure.
 */
ClientFramebuffer* clientfb_create(SockAddress* console_socket,
                                   const char* protocol,
                                   QFrameBuffer* fb);

/*
 * Disconnects and destroys framebuffer client.
 * Param:
 *  client_fb Framebuffer client descriptor created with clientfb_create.
 */
void clientfb_destroy(ClientFramebuffer* client_fb);

#endif /* _ANDROID_FRAMEBUFFER_UI_H */
