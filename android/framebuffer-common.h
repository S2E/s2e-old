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
 * Contains framebuffer declarations that are shared by the core and the UI.
 */

#ifndef _ANDROID_FRAMEBUFFER_COMMON_H
#define _ANDROID_FRAMEBUFFER_COMMON_H

#include "sysemu.h"

/* Header of framebuffer update message sent from the core to the UI. */
typedef struct FBUpdateMessage {
    /* x, y, w, and h identify the rectangle that is being updated. */
    uint16_t    x;
    uint16_t    y;
    uint16_t    w;
    uint16_t    h;

    /* Contains updating rectangle copied over from the framebuffer's pixels. */
    uint8_t rect[0];
} FBUpdateMessage;

#endif /* _ANDROID_FRAMEBUFFER_UI_H */
