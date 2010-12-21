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
 * Contains extension to android display (see android/display.h|c) that is used
 * by the core to communicate display changes to the attached UI
 */

#ifndef _ANDROID_DISPLAY_CORE_H
#define _ANDROID_DISPLAY_CORE_H

#include "framebuffer.h"
#include "android/display.h"
#include "android/framebuffer-core.h"

/* Descriptor for a core display instance */
typedef struct CoreDisplay CoreDisplay;

/*
 * Initializes one and only one instance of a core display.
 * Param:
 *  ds - Display state to use for the core display.
 */
extern void coredisplay_init(DisplayState* ds);

/*
 * Attaches framebuffer service to the core display.
 * Param:
 *  core_fb - Framebuffer service descriptor to attach.
 * Return:
 *  0 on success, or -1 on failure.
 */
extern int coredisplay_attach_fb_service(CoreFramebuffer* core_fb);

/*
 * Detaches framebuffer service previously attached to the core display.
 * Return:
 *  Framebuffer service descriptor attached to the core display, or NULL if
 *  the core display didn't have framebuffer service attached to it.
 */
extern CoreFramebuffer* coredisplay_detach_fb_service(void);

/*
 * Get framebuffer descriptor for core display.
 * Return:
 *  Framebuffer descriptor for core display.
 */
extern QFrameBuffer* coredisplay_get_framebuffer(void);

#endif /* _ANDROID_DISPLAY_CORE_H */
