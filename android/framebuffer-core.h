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
 * Contains core-side framebuffer service that sends framebuffer updates
 * to the UI connected to the core.
 */

#ifndef _ANDROID_FRAMEBUFFER_CORE_H
#define _ANDROID_FRAMEBUFFER_CORE_H

/* Descriptor for a framebuffer core service instance */
typedef struct CoreFramebuffer CoreFramebuffer;

/*
 * Creates framebuffer service.
 * Param:
 *  sock - Socket descriptor for the service
 *  protocol - Defines protocol to use when sending FB updates to the UI. The
 *      supported values ar:
 *      -raw Transfers the updating rectangle buffer over the socket.
 *      -shared Used a shared memory to transfer the updating rectangle buffer.
 *  fb - Framebuffer descriptor for this service.
 * Return:
 *  Framebuffer service descriptor.
 */
CoreFramebuffer* corefb_create(int sock, const char* protocol, struct QFrameBuffer* fb);

/*
 * Destroys framebuffer service created with corefb_create.
 * Param:
 *  core_fb - Framebuffer service descriptor created with corefb_create
 */
void corefb_destroy(CoreFramebuffer* core_fb);

/*
 * Notifies framebuffer client about changes in framebuffer.
 * Param:
 *  core_fb - Framebuffer service descriptor created with corefb_create
 *  ds - Display state for the framebuffer.
 *  fb Framebuffer containing pixels.
 *  x, y, w, and h identify the rectangle that has benn changed.
 */
void corefb_update(CoreFramebuffer* core_fb, struct DisplayState* ds,
                   struct QFrameBuffer* fb, int x, int y, int w, int h);

/*
 * Gets number of bits used to encode a single pixel.
 * Param:
 *  core_fb - Framebuffer service descriptor created with corefb_create
 * Return:
 *  Number of bits used to encode a single pixel.
 */
int corefb_get_bits_per_pixel(CoreFramebuffer* core_fb);

#endif /* _ANDROID_FRAMEBUFFER_CORE_H */
