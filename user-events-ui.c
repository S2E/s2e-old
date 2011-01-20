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
#include "android/utils/debug.h"
#include "android/user-events-common.h"
#include "console.h"
#include <stdio.h>

#include "android/looper.h"
#include "android/async-utils.h"
#include "android/core-connection.h"

/* Descriptor for the user events client. */
typedef struct ClientUserEvents {
    /* Core connection instance for the user events client. */
    CoreConnection* core_connection;

    /* Socket for the client. */
    int             sock;

    /* Socket wrapper for sync I/O. */
    SyncSocket*     sync_socket;
} ClientUserEvents;

/* One and only one user events client instance. */
static ClientUserEvents _client_ue = { 0 };

int
clientue_create(SockAddress* console_socket)
{
    char* connect_message = NULL;
    char switch_cmd[256];

    // Connect to the framebuffer service.
    _client_ue.core_connection = core_connection_create(console_socket);
    if (_client_ue.core_connection == NULL) {
        derror("User events client is unable to connect to the console: %s\n",
               errno_str);
        return -1;
    }
    if (core_connection_open(_client_ue.core_connection)) {
        core_connection_free(_client_ue.core_connection);
        _client_ue.core_connection = NULL;
        derror("User events client is unable to open the console: %s\n",
               errno_str);
        return -1;
    }
    snprintf(switch_cmd, sizeof(switch_cmd), "user events");
    if (core_connection_switch_stream(_client_ue.core_connection, switch_cmd,
                                      &connect_message)) {
        derror("Unable to connect to the user events service: %s\n",
               connect_message ? connect_message : "");
        if (connect_message != NULL) {
            free(connect_message);
        }
        core_connection_close(_client_ue.core_connection);
        core_connection_free(_client_ue.core_connection);
        _client_ue.core_connection = NULL;
        return -1;
    }

    // Now that we're connected lets initialize the descriptor.
    _client_ue.sock = core_connection_get_socket(_client_ue.core_connection);
    _client_ue.sync_socket = syncsocket_init(_client_ue.sock);
    if (connect_message != NULL) {
        free(connect_message);
    }

    fprintf(stdout, "User events client is now attached to the core %s\n",
            sock_address_to_string(console_socket));

    return 0;
}

/* Sends an event to the core.
 * Parameters:
 *  ue - User events client instance.
 *  event - Event type. Must be one of the AUSER_EVENT_XXX.
 *  event_param - Event parameters.
 *  size - Byte size of the event parameters buffer.
 * Return:
 *  0 on success, or -1 on failure.
 */
static int
clientue_send(ClientUserEvents* ue,
              uint8_t event,
              const void* event_param,
              size_t size)
{
    int res;
    UserEventHeader header;

    header.event_type = event;
    syncsocket_start_write(ue->sync_socket);
    // Send event type first (event header)
    res = syncsocket_write(ue->sync_socket, &header, sizeof(header), 500);
    if (res < 0) {
        return -1;
    }
    // Send event param next.
    res = syncsocket_write(ue->sync_socket, event_param, size, 500);
    if (res < 0) {
        return -1;
    }
    syncsocket_stop_write(ue->sync_socket);
    return 0;
}

void
user_event_keycodes(int *kcodes, int count)
{
    int nn;
    for (nn = 0; nn < count; nn++)
        user_event_keycode(kcodes[nn]);
}

void
user_event_keycode(int  kcode)
{
    UserEventKeycode    message;
    message.keycode = kcode;
    clientue_send(&_client_ue, AUSER_EVENT_KEYCODE, &message, sizeof(message));
}

void
user_event_key(unsigned code, unsigned down)
{
    if(code == 0) {
        return;
    }
    if (VERBOSE_CHECK(keys))
        printf(">> KEY [0x%03x,%s]\n", (code & 0x1ff), down ? "down" : " up " );

    user_event_keycode((code & 0x1ff) | (down ? 0x200 : 0));
}


void
user_event_mouse(int dx, int dy, int dz, unsigned buttons_state)
{
    UserEventMouse    message;
    message.dx = dx;
    message.dy = dy;
    message.dz = dz;
    message.buttons_state = buttons_state;
    clientue_send(&_client_ue, AUSER_EVENT_MOUSE, &message, sizeof(message));
}

void  user_event_register_generic(void* opaque, QEMUPutGenericEvent *callback)
{
}

void
user_event_generic(int type, int code, int value)
{
    UserEventGeneric    message;
    message.type = type;
    message.code = code;
    message.value = value;
    clientue_send(&_client_ue, AUSER_EVENT_GENERIC, &message, sizeof(message));
}
