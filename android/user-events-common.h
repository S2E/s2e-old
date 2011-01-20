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

#ifndef _ANDROID_USER_EVENTS_COMMON_H
#define _ANDROID_USER_EVENTS_COMMON_H

#include "globals.h"

/* Mouse event. */
#define AUSER_EVENT_MOUSE     0
/* Keycode event. */
#define AUSER_EVENT_KEYCODE   1
/* Generic event. */
#define AUSER_EVENT_GENERIC   2

/* Header for user event message sent from UI to the core. */
typedef struct UserEventHeader {
    /* Event type. See AUSER_EVENT_XXX for possible values. */
    uint8_t event_type;
} UserEventHeader;

/* Formats mouse event message (AUSER_EVENT_MOUSE) sent from
 * UI to the core.
 */
typedef struct UserEventMouse {
    int         dx;
    int         dy;
    int         dz;
    unsigned    buttons_state;
} UserEventMouse;

/* Formats keycode event message (AUSER_EVENT_KEYCODE) sent from
 * UI to the core.
 */
typedef struct UserEventKeycode {
    int         keycode;
} UserEventKeycode;

/* Formats generic event message (AUSER_EVENT_GENERIC) sent from
 * UI to the core.
 */
typedef struct UserEventGeneric {
    int         type;
    int         code;
    int         value;
} UserEventGeneric;

#endif /* _ANDROID_USER_EVENTS_COMMON_H */
