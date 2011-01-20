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
 * Contains recepient of user events sent from the UI.
 */

#ifndef _ANDROID_USER_EVENTS_CORE_H
#define _ANDROID_USER_EVENTS_CORE_H

/* Descriptor for a core user events instance */
typedef struct CoreUserEvents CoreUserEvents;

/*
 * Creates and initializes core user events instance.
 * Param:
 *  fd - Socket descriptor for the service.
 */
extern CoreUserEvents* coreue_create(int fd);

/*
 * Destroys core user events service.
 * Param:
 *  ue - User event service descriptor to destroy.
 */
extern void coreue_destroy(CoreUserEvents* ue);

#endif /* _ANDROID_USER_EVENTS_CORE_H */
