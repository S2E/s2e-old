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

#include "sysemu.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/utils/panic.h"
#include "android/sync-utils.h"
#include "android/protocol/fb-updates.h"
#include "android/protocol/fb-updates-impl.h"

/*Enumerates states for the client framebuffer update reader. */
typedef enum ImplFBState {
    /* The reader is waiting on update header. */
    EXPECTS_HEADER,

    /* The reader is waiting on pixels. */
    EXPECTS_PIXELS,
} ImplFBState;

/* Descriptor for the UI-side implementation of the "framebufer" service.
 */
typedef struct ImplFramebuffer {
    /* Framebuffer for this client. */
    QFrameBuffer*   fb;

    /* Core connection instance for the framebuffer client. */
    CoreConnection* core_connection;

    /* Current update header. */
    FBUpdateMessage update_header;

    /* Reader's buffer. */
    uint8_t*        reader_buffer;

    /* Offset in the reader's buffer where to read next chunk of data. */
    size_t          reader_offset;

    /* Total number of bytes the reader expects to read. */
    size_t          reader_bytes;

    /* Current state of the update reader. */
    ImplFBState     fb_state;

    /* Socket descriptor for the framebuffer client. */
    int             sock;

    /* Number of bits used to encode single pixel. */
    int             bits_per_pixel;
} ImplFramebuffer;

/* One and the only ImplFramebuffer instance. */
static ImplFramebuffer _implFb;

/*
 * Updates a display rectangle.
 * Param
 *  fb - Framebuffer where to update the rectangle.
 *  x, y, w, and h define rectangle to update.
 *  bits_per_pixel define number of bits used to encode a single pixel.
 *  pixels contains pixels for the rectangle. Buffer addressed by this parameter
 *      must be eventually freed with free()
 */
static void
_update_rect(QFrameBuffer* fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
             uint8_t bits_per_pixel, uint8_t* pixels)
{
    if (fb != NULL) {
        uint16_t n;
        const uint8_t* src = pixels;
        const uint16_t src_line_size = w * ((bits_per_pixel + 7) / 8);
        uint8_t* dst  = (uint8_t*)fb->pixels + y * fb->pitch + x *
                        fb->bytes_per_pixel;
        for (n = 0; n < h; n++) {
            memcpy(dst, src, src_line_size);
            src += src_line_size;
            dst += fb->pitch;
        }
        qframebuffer_update(fb, x, y, w, h);
    }
    free(pixels);
}

/*
 * Asynchronous I/O callback launched when framebuffer notifications are ready
 * to be read.
 * Param:
 *  opaque - ImplFramebuffer instance.
 */
static void
_implFb_read_cb(void* opaque)
{
    ImplFramebuffer* fb_client = opaque;
    int  ret;

    // Read updates while they are immediately available.
    for (;;) {
        // Read next chunk of data.
        ret = read(fb_client->sock, fb_client->reader_buffer + fb_client->reader_offset,
                   fb_client->reader_bytes - fb_client->reader_offset);
        if (ret == 0) {
            /* disconnection ! */
            implFb_destroy();
            return;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                /* loop on EINTR */
                continue;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Chunk is not avalable at this point. Come back later.
                return;
            }
        }

        fb_client->reader_offset += ret;
        if (fb_client->reader_offset != fb_client->reader_bytes) {
            // There are still some data left in the pipe.
            continue;
        }

        // All expected data has been read. Time to change the state.
        if (fb_client->fb_state == EXPECTS_HEADER) {
            // Update header has been read. Prepare for the pixels.
            fb_client->fb_state = EXPECTS_PIXELS;
            fb_client->reader_offset = 0;
            fb_client->reader_bytes = fb_client->update_header.w *
                                      fb_client->update_header.h *
                                      (fb_client->bits_per_pixel / 8);
            fb_client->reader_buffer = malloc(fb_client->reader_bytes);
            if (fb_client->reader_buffer == NULL) {
                APANIC("Unable to allocate memory for framebuffer update\n");
            }
        } else {
            // Pixels have been read. Prepare for the header.
             uint8_t* pixels = fb_client->reader_buffer;

            fb_client->fb_state = EXPECTS_HEADER;
            fb_client->reader_offset = 0;
            fb_client->reader_bytes = sizeof(FBUpdateMessage);
            fb_client->reader_buffer = (uint8_t*)&fb_client->update_header;

            // Perform the update. Note that pixels buffer must be freed there.
            _update_rect(fb_client->fb, fb_client->update_header.x,
                        fb_client->update_header.y, fb_client->update_header.w,
                        fb_client->update_header.h, fb_client->bits_per_pixel,
                        pixels);
        }
    }
}

int
implFb_create(SockAddress* console_socket, const char* protocol, QFrameBuffer* fb)
{
    char* handshake = NULL;
    char switch_cmd[256];

    // Initialize descriptor.
    _implFb.fb = fb;
    _implFb.reader_buffer = (uint8_t*)&_implFb.update_header;
    _implFb.reader_offset = 0;
    _implFb.reader_bytes = sizeof(FBUpdateMessage);

    // Connect to the framebuffer service.
    snprintf(switch_cmd, sizeof(switch_cmd), "framebuffer %s", protocol);
    _implFb.core_connection =
        core_connection_create_and_switch(console_socket, switch_cmd, &handshake);
    if (_implFb.core_connection == NULL) {
        derror("Unable to connect to the framebuffer service: %s\n",
               errno_str);
        return -1;
    }

    // We expect core framebuffer to return us bits per pixel property in
    // the handshake message.
    _implFb.bits_per_pixel = 0;
    if (handshake != NULL) {
        char* bpp = strstr(handshake, "bitsperpixel=");
        if (bpp != NULL) {
            char* end;
            bpp += strlen("bitsperpixel=");
            end = strchr(bpp, ' ');
            if (end == NULL) {
                end = bpp + strlen(bpp);
            }
            _implFb.bits_per_pixel = strtol(bpp, &end, 0);
        }
    }
    if (!_implFb.bits_per_pixel) {
        derror("Unexpected core framebuffer reply: %s\n"
               "Bits per pixel property is not there, or is invalid\n",
               handshake);
        implFb_destroy();
        return -1;
    }

    _implFb.sock = core_connection_get_socket(_implFb.core_connection);

    // At last setup read callback, and start receiving the updates.
    if (qemu_set_fd_handler(_implFb.sock, _implFb_read_cb, NULL, &_implFb)) {
        derror("Unable to set up framebuffer read callback.\n");
        implFb_destroy();
        return -1;
    }
    {
        // Force the core to send us entire framebuffer now, when we're prepared
        // to receive it.
        FBRequestHeader hd;
        SyncSocket* sk = syncsocket_init(_implFb.sock);

        hd.request_type = AFB_REQUEST_REFRESH;
        syncsocket_start_write(sk);
        syncsocket_write(sk, &hd, sizeof(hd), 5000);
        syncsocket_stop_write(sk);
        syncsocket_free(sk);
    }

    fprintf(stdout, "framebuffer is now connected to the core at %s.",
            sock_address_to_string(console_socket));
    if (handshake != NULL) {
        if (handshake[0] != '\0') {
            fprintf(stdout, " Handshake: %s", handshake);
        }
        free(handshake);
    }
    fprintf(stdout, "\n");

    return 0;
}

void
implFb_destroy(void)
{
    if (_implFb.core_connection != NULL) {
        // Disable the reader callback.
        qemu_set_fd_handler(_implFb.sock, NULL, NULL, NULL);

        // Close framebuffer connection.
        core_connection_close(_implFb.core_connection);
        core_connection_free(_implFb.core_connection);
        _implFb.core_connection = NULL;
    }

    _implFb.fb = NULL;
    if (_implFb.reader_buffer != NULL &&
        _implFb.reader_buffer != (uint8_t*)&_implFb.update_header) {
        free(_implFb.reader_buffer);
        _implFb.reader_buffer = (uint8_t*)&_implFb.update_header;
    }
}
