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

#include "user-events.h"
#include "android/globals.h"
#include "android/android.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/user-events-common.h"
#include "android/user-events-core.h"
#include "android/sync-utils.h"

/* States of the core user events service.
 */

/* Event header is expected in the pipe. */
#define UE_STATE_EVENT_HEADER  0
/* Event parameters are expected in the pipe. */
#define UE_STATE_EVENT_PARAM   1

/* Core user events service descriptor. */
struct CoreUserEvents {
    /* Reader to receive user events. */
    AsyncReader     user_events_reader;

    /* I/O associated with this descriptor. */
    LoopIo          io;

    /* Looper used to communicate user events. */
    Looper*         looper;

    /* Socket for this service. */
    int             sock;

    /* State of the service (see UE_STATE_XXX for possible values). */
    int             state;

    /* Current event header. */
    UserEventHeader event_header;

    /* Current event parameters. */
    union {
        UserEventGeneric    generic_event;
        UserEventMouse      mouse_event;
        UserEventKeycode    keycode_event;
    };
};


/*
 * Asynchronous I/O callback launched when reading user events from the socket.
 * Param:
 *  opaque - CoreUserEvents instance.
 */
static void
coreue_io_func(void* opaque, int fd, unsigned events)
{
    CoreUserEvents* ue = opaque;
    // Read whatever is expected from the socket.
    const AsyncStatus status = asyncReader_read(&ue->user_events_reader, &ue->io);

    switch (status) {
        case ASYNC_COMPLETE:
            switch (ue->state) {
                case UE_STATE_EVENT_HEADER:
                    // We just read event header. Now we expect event parameters.
                    ue->state = UE_STATE_EVENT_PARAM;
                    // Setup the reader depending on the event type.
                    switch (ue->event_header.event_type) {
                        case AUSER_EVENT_MOUSE:
                            asyncReader_init(&ue->user_events_reader,
                                             &ue->mouse_event,
                                             sizeof(ue->mouse_event),
                                             &ue->io);
                            break;
                        case AUSER_EVENT_KEYCODE:
                            asyncReader_init(&ue->user_events_reader,
                                             &ue->keycode_event,
                                             sizeof(ue->keycode_event),
                                             &ue->io);
                            break;
                        case AUSER_EVENT_GENERIC:
                            asyncReader_init(&ue->user_events_reader,
                                             &ue->generic_event,
                                             sizeof(ue->generic_event),
                                             &ue->io);
                            break;
                        default:
                            derror("Unexpected event type %d\n",
                                   ue->event_header.event_type);
                            break;
                    }
                    break;

                case UE_STATE_EVENT_PARAM:
                    // We just read event parameters. Lets fire the event.
                    switch (ue->event_header.event_type) {
                        case AUSER_EVENT_MOUSE:
                            user_event_mouse(ue->mouse_event.dx,
                                             ue->mouse_event.dy,
                                             ue->mouse_event.dz,
                                             ue->mouse_event.buttons_state);
                            break;
                        case AUSER_EVENT_KEYCODE:
                            user_event_keycode(ue->keycode_event.keycode);
                            break;
                        case AUSER_EVENT_GENERIC:
                            user_event_generic(ue->generic_event.type,
                                               ue->generic_event.code,
                                               ue->generic_event.value);
                            break;
                        default:
                            derror("Unexpected event type %d\n",
                                   ue->event_header.event_type);
                            break;
                    }
                    // Now we expect event header.
                    ue->event_header.event_type = -1;
                    ue->state = UE_STATE_EVENT_HEADER;
                    asyncReader_init(&ue->user_events_reader, &ue->event_header,
                                     sizeof(ue->event_header), &ue->io);
                    break;
            }
            break;
        case ASYNC_ERROR:
            loopIo_dontWantRead(&ue->io);
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

CoreUserEvents*
coreue_create(int fd)
{
    CoreUserEvents* ue;
    ANEW0(ue);
    ue->sock = fd;
    ue->state = UE_STATE_EVENT_HEADER;
    ue->looper = looper_newCore();
    loopIo_init(&ue->io, ue->looper, ue->sock, coreue_io_func, ue);
    asyncReader_init(&ue->user_events_reader, &ue->event_header,
                     sizeof(ue->event_header), &ue->io);
    return ue;
}

void
coreue_destroy(CoreUserEvents* ue)
{
    if (ue != NULL) {
        if (ue->looper != NULL) {
            // Stop all I/O that may still be going on.
            loopIo_done(&ue->io);
            looper_free(ue->looper);
            ue->looper = NULL;
        }
        free(ue);
    }
}
