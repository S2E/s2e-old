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

#include "android/framebuffer-common.h"
#include "android/framebuffer-ui.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"

#define  PANIC(...) do { fprintf(stderr, __VA_ARGS__);  \
                         exit(1);                       \
                    } while (0)

/*
 * Enumerates states for the client framebuffer update reader.
 */
typedef enum ClientFBState {
    /* The reader is waiting on update header. */
    WAIT_HEADER,

    /* The reader is waiting on pixels. */
    WAIT_PIXELS,
} ClientFBState;

/*
 * Descriptor for the framebuffer client.
 */
struct ClientFramebuffer {
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
    ClientFBState   fb_state;

    /* Socket descriptor for the framebuffer client. */
    int             sock;

    /* Number of bits used to encode single pixel. */
    int             bits_per_pixel;
};

/* The only instance of framebuffer client. */
static ClientFramebuffer _client_fb;

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
update_rect(QFrameBuffer* fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
            uint8_t bits_per_pixel, uint8_t* pixels)
{
    if (fb != NULL) {
        uint16_t n;
        const uint8_t* src = pixels;
        const uint16_t src_line_size = w * ((bits_per_pixel + 7) / 8);
        uint8_t* dst  = (uint8_t*)fb->pixels + y * fb->pitch + x * fb->bytes_per_pixel;
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
 *  opaque - ClientFramebuffer instance.
 */
static void
_clientfb_read_cb(void* opaque)
{
    ClientFramebuffer* fb_client = opaque;
    int  ret;

    // Read updates while they are immediately available.
    for (;;) {
        // Read next chunk of data.
        ret = read(fb_client->sock, fb_client->reader_buffer + fb_client->reader_offset,
                   fb_client->reader_bytes - fb_client->reader_offset);
        if (ret == 0) {
            /* disconnection ! */
            clientfb_destroy(fb_client);
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
        if (fb_client->fb_state == WAIT_HEADER) {
            // Update header has been read. Prepare for the pixels.
            fb_client->fb_state = WAIT_PIXELS;
            fb_client->reader_offset = 0;
            fb_client->reader_bytes = fb_client->update_header.w *
                                      fb_client->update_header.h *
                                      (fb_client->bits_per_pixel / 8);
            fb_client->reader_buffer = malloc(fb_client->reader_bytes);
            if (fb_client->reader_buffer == NULL) {
                PANIC("Unable to allocate memory for framebuffer update\n");
            }
        } else {
            // Pixels have been read. Prepare for the header.
             uint8_t* pixels = fb_client->reader_buffer;

            fb_client->fb_state = WAIT_HEADER;
            fb_client->reader_offset = 0;
            fb_client->reader_bytes = sizeof(FBUpdateMessage);
            fb_client->reader_buffer = (uint8_t*)&fb_client->update_header;

            // Perform the update. Note that pixels buffer must be freed there.
            update_rect(fb_client->fb, fb_client->update_header.x,
                        fb_client->update_header.y, fb_client->update_header.w,
                        fb_client->update_header.h, fb_client->bits_per_pixel,
                        pixels);
        }
    }
}

ClientFramebuffer*
clientfb_create(SockAddress* console_socket,
                const char* protocol,
                QFrameBuffer* fb)
{
    char* connect_message = NULL;
    char switch_cmd[256];

    // Connect to the framebuffer service.
    _client_fb.core_connection = core_connection_create(console_socket);
    if (_client_fb.core_connection == NULL) {
        derror("Framebuffer client is unable to connect to the console: %s\n",
               errno_str);
        return NULL;
    }
    if (core_connection_open(_client_fb.core_connection)) {
        core_connection_free(_client_fb.core_connection);
        _client_fb.core_connection = NULL;
        derror("Framebuffer client is unable to open the console: %s\n",
               errno_str);
        return NULL;
    }
    snprintf(switch_cmd, sizeof(switch_cmd), "framebuffer %s", protocol);
    if (core_connection_switch_stream(_client_fb.core_connection, switch_cmd,
                                      &connect_message)) {
        derror("Unable to attach to the framebuffer %s: %s\n",
               switch_cmd, connect_message ? connect_message : "");
        if (connect_message != NULL) {
            free(connect_message);
        }
        core_connection_close(_client_fb.core_connection);
        core_connection_free(_client_fb.core_connection);
        _client_fb.core_connection = NULL;
        return NULL;
    }

    // We expect core framebuffer to return us bits per pixel property in
    // the handshake message.
    _client_fb.bits_per_pixel = 0;
    if (connect_message != NULL) {
        char* bpp = strstr(connect_message, "bitsperpixel=");
        if (bpp != NULL) {
            char* end;
            bpp += strlen("bitsperpixel=");
            end = strchr(bpp, ' ');
            if (end == NULL) {
                end = bpp + strlen(bpp);
            }
            _client_fb.bits_per_pixel = strtol(bpp, &end, 0);
        }
    }

    if (!_client_fb.bits_per_pixel) {
        derror("Unexpected core framebuffer reply: %s\n"
               "Bits per pixel property is not there, or is invalid\n", connect_message);
        core_connection_close(_client_fb.core_connection);
        core_connection_free(_client_fb.core_connection);
        _client_fb.core_connection = NULL;
        return NULL;
    }

    // Now that we're connected lets initialize the descriptor.
    _client_fb.fb = fb;
    _client_fb.sock = core_connection_get_socket(_client_fb.core_connection);
    _client_fb.fb_state = WAIT_HEADER;
    _client_fb.reader_buffer = (uint8_t*)&_client_fb.update_header;
    _client_fb.reader_offset = 0;
    _client_fb.reader_bytes = sizeof(FBUpdateMessage);

    if (connect_message != NULL) {
        free(connect_message);
    }

    // At last setup read callback, and start receiving the updates.
    if (qemu_set_fd_handler(_client_fb.sock, _clientfb_read_cb, NULL, &_client_fb)) {
        derror("Unable to set up framebuffer read callback\n");
        core_connection_close(_client_fb.core_connection);
        core_connection_free(_client_fb.core_connection);
        _client_fb.core_connection = NULL;
        return NULL;
    }

    fprintf(stdout, "Framebuffer %s is now attached to the core %s\n",
            protocol, sock_address_to_string(console_socket));

    return &_client_fb;
}

void
clientfb_destroy(ClientFramebuffer* client_fb)
{
    if (client_fb != NULL && client_fb->core_connection != NULL) {
        // Disable the reader callback.
        qemu_set_fd_handler(client_fb->sock, NULL, NULL, NULL);

        // Close framebuffer connection.
        core_connection_close(client_fb->core_connection);
        core_connection_free(client_fb->core_connection);
        client_fb->core_connection = NULL;
    }
}
