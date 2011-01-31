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

#include "console.h"
#include "framebuffer.h"
#include "android/looper.h"
#include "android/display-core.h"
#include "android/async-utils.h"
#include "android/protocol/fb-updates.h"
#include "android/protocol/fb-updates-proxy.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"

/* Descriptor for the Core-side implementation of the "framebufer" service.
 */
struct ProxyFramebuffer {
    /* Writer used to send FB update notification messages. */
    AsyncWriter             fb_update_writer;

    /* Reader used to read FB requests from the client. */
    AsyncReader             fb_req_reader;

    /* I/O associated with this descriptor. */
    LoopIo                  io;

    /* Framebuffer used for this service. */
    QFrameBuffer*           fb;

    /* Looper used to communicate framebuffer updates. */
    Looper* looper;

    /* Head of the list of pending FB update notifications. */
    struct FBUpdateNotify*  fb_update_head;

    /* Tail of the list of pending FB update notifications. */
    struct FBUpdateNotify*  fb_update_tail;

    /* Socket used to communicate framebuffer updates. */
    int     sock;

    /* Framebuffer request header. */
    FBRequestHeader         fb_req_header;
};

/* Framebuffer update notification descriptor. */
typedef struct FBUpdateNotify {
    /* Links all pending FB update notifications. */
    struct FBUpdateNotify*  next_fb_update;

    /* Core framebuffer instance that owns the message. */
    ProxyFramebuffer*       proxy_fb;

    /* Size of the message to transfer. */
    size_t                  message_size;

    /* Update message. */
    FBUpdateMessage         message;
} FBUpdateNotify;

/*
 * Gets pointer in framebuffer's pixels for the given pixel.
 * Param:
 *  fb Framebuffer containing pixels.
 *  x, and y identify the pixel to get pointer for.
 * Return:
 *  Pointer in framebuffer's pixels for the given pixel.
 */
static const uint8_t*
_pixel_offset(const QFrameBuffer* fb, int x, int y)
{
    return (const uint8_t*)fb->pixels + y * fb->pitch + x * fb->bytes_per_pixel;
}

/*
 * Copies pixels from a framebuffer rectangle.
 * Param:
 *  rect - Buffer where to copy pixel.
 *  fb - Framebuffer containing the rectangle to copy.
 *  x, y, w, and h - dimensions of the rectangle to copy.
 */
static void
_copy_fb_rect(uint8_t* rect, const QFrameBuffer* fb, int x, int y, int w, int h)
{
    const uint8_t* start = _pixel_offset(fb, x, y);
    for (; h > 0; h--) {
        memcpy(rect, start, w * fb->bytes_per_pixel);
        start += fb->pitch;
        rect += w * fb->bytes_per_pixel;
    }
}

/*
 * Allocates and initializes framebuffer update notification descriptor.
 * Param:
 *  ds - Display state for the framebuffer.
 *  fb Framebuffer containing pixels.
 *  x, y, w, and h identify the rectangle that is being updated.
 * Return:
 *  Initialized framebuffer update notification descriptor.
 */
static FBUpdateNotify*
fbupdatenotify_create(ProxyFramebuffer* proxy_fb, const QFrameBuffer* fb,
                      int x, int y, int w, int h)
{
    const size_t rect_size = w * h * fb->bytes_per_pixel;
    FBUpdateNotify* ret = malloc(sizeof(FBUpdateNotify) + rect_size);

    ret->next_fb_update = NULL;
    ret->proxy_fb = proxy_fb;
    ret->message_size = sizeof(FBUpdateMessage) + rect_size;
    ret->message.x = x;
    ret->message.y = y;
    ret->message.w = w;
    ret->message.h = h;
    _copy_fb_rect(ret->message.rect, fb, x, y, w, h);
    return ret;
}

/*
 * Deletes FBUpdateNotify descriptor, created with fbupdatenotify_create.
 * Param:
 *  desc - Descreptor to delete.
 */
static void
fbupdatenotify_delete(FBUpdateNotify* desc)
{
    if (desc != NULL) {
        free(desc);
    }
}

/* Implemented in android/console.c */
extern void destroy_control_fb_client(void);

/*
 * Asynchronous write I/O callback launched when writing framebuffer
 * notifications to the socket.
 * Param:
 *  proxy_fb - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_write(ProxyFramebuffer* proxy_fb)
{
    while (proxy_fb->fb_update_head != NULL) {
        FBUpdateNotify* current_update = proxy_fb->fb_update_head;
        // Lets continue writing of the current notification.
        const AsyncStatus status =
            asyncWriter_write(&proxy_fb->fb_update_writer, &proxy_fb->io);
        switch (status) {
            case ASYNC_COMPLETE:
                // Done with the current update. Move on to the next one.
                break;
            case ASYNC_ERROR:
                // Done with the current update. Move on to the next one.
                loopIo_dontWantWrite(&proxy_fb->io);
                break;

            case ASYNC_NEED_MORE:
                // Transfer will eventually come back into this routine.
                return;
        }

        // Advance the list of updates
        proxy_fb->fb_update_head = current_update->next_fb_update;
        if (proxy_fb->fb_update_head == NULL) {
            proxy_fb->fb_update_tail = NULL;
        }
        fbupdatenotify_delete(current_update);

        if (proxy_fb->fb_update_head != NULL) {
            // Schedule the next one.
            asyncWriter_init(&proxy_fb->fb_update_writer,
                             &proxy_fb->fb_update_head->message,
                             proxy_fb->fb_update_head->message_size,
                             &proxy_fb->io);
        }
    }
}

/*
 * Asynchronous read I/O callback launched when reading framebuffer requests
 * from the socket.
 * Param:
 *  proxy_fb - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_read(ProxyFramebuffer* proxy_fb)
{
    // Read the request header.
    const AsyncStatus status =
        asyncReader_read(&proxy_fb->fb_req_reader, &proxy_fb->io);
    switch (status) {
        case ASYNC_COMPLETE:
            // Request header is received
            switch (proxy_fb->fb_req_header.request_type) {
                case AFB_REQUEST_REFRESH:
                    // Force full screen update to be sent
                    proxyFb_update(proxy_fb, proxy_fb->fb,
                                  0, 0, proxy_fb->fb->width,
                                  proxy_fb->fb->height);
                    break;
                default:
                    derror("Unknown framebuffer request %d\n",
                           proxy_fb->fb_req_header.request_type);
                    break;
            }
            proxy_fb->fb_req_header.request_type = -1;
            asyncReader_init(&proxy_fb->fb_req_reader, &proxy_fb->fb_req_header,
                             sizeof(proxy_fb->fb_req_header), &proxy_fb->io);
            break;
        case ASYNC_ERROR:
            loopIo_dontWantRead(&proxy_fb->io);
            if (errno == ECONNRESET) {
                // UI has exited. We need to destroy framebuffer service.
                destroy_control_fb_client();
            }
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

/*
 * Asynchronous I/O callback launched when writing framebuffer notifications
 * to the socket.
 * Param:
 *  opaque - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_fun(void* opaque, int fd, unsigned events)
{
    if (events & LOOP_IO_READ) {
        _proxyFb_io_read((ProxyFramebuffer*)opaque);
    } else if (events & LOOP_IO_WRITE) {
        _proxyFb_io_write((ProxyFramebuffer*)opaque);
    }
}

ProxyFramebuffer*
proxyFb_create(int sock, const char* protocol, QFrameBuffer* fb)
{
    // At this point we're implementing the -raw protocol only.
    ProxyFramebuffer* ret;
    ANEW0(ret);
    ret->sock = sock;
    ret->looper = looper_newCore();
    ret->fb = fb;
    ret->fb_update_head = NULL;
    ret->fb_update_tail = NULL;
    loopIo_init(&ret->io, ret->looper, sock, _proxyFb_io_fun, ret);
    asyncReader_init(&ret->fb_req_reader, &ret->fb_req_header,
                     sizeof(ret->fb_req_header), &ret->io);
    return ret;
}

void
proxyFb_destroy(ProxyFramebuffer* proxy_fb)
{
    if (proxy_fb != NULL) {
        if (proxy_fb->looper != NULL) {
            // Stop all I/O that may still be going on.
            loopIo_done(&proxy_fb->io);
            // Delete all pending frame updates.
            while (proxy_fb->fb_update_head != NULL) {
                FBUpdateNotify* pending_update = proxy_fb->fb_update_head;
                proxy_fb->fb_update_head = pending_update->next_fb_update;
                fbupdatenotify_delete(pending_update);
            }
            proxy_fb->fb_update_tail = NULL;
            looper_free(proxy_fb->looper);
            proxy_fb->looper = NULL;
        }
    }
}

void
proxyFb_update(ProxyFramebuffer* proxy_fb,
              struct QFrameBuffer* fb, int x, int y, int w, int h)
{
    AsyncStatus status;
    FBUpdateNotify* descr = fbupdatenotify_create(proxy_fb, fb, x, y, w, h);

    // Lets see if we should list it behind other pending updates.
    if (proxy_fb->fb_update_tail != NULL) {
        proxy_fb->fb_update_tail->next_fb_update = descr;
        proxy_fb->fb_update_tail = descr;
        return;
    }

    // We're first in the list. Just send it now.
    proxy_fb->fb_update_head = proxy_fb->fb_update_tail = descr;
    asyncWriter_init(&proxy_fb->fb_update_writer,
                     &proxy_fb->fb_update_head->message,
                     proxy_fb->fb_update_head->message_size, &proxy_fb->io);
    status = asyncWriter_write(&proxy_fb->fb_update_writer, &proxy_fb->io);
    switch (status) {
        case ASYNC_COMPLETE:
            fbupdatenotify_delete(descr);
            proxy_fb->fb_update_head = proxy_fb->fb_update_tail = NULL;
            return;
        case ASYNC_ERROR:
            fbupdatenotify_delete(descr);
            proxy_fb->fb_update_head = proxy_fb->fb_update_tail = NULL;
            return;
        case ASYNC_NEED_MORE:
            // Update transfer will eventually complete in _proxyFb_io_fun
            return;
    }
}

int
proxyFb_get_bits_per_pixel(ProxyFramebuffer* proxy_fb)
{
    return (proxy_fb != NULL && proxy_fb->fb != NULL) ?
                                            proxy_fb->fb->bits_per_pixel : -1;
}
