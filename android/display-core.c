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

#include "android/utils/system.h"
#include "android/display-core.h"

/* Core display descriptor. */
struct CoreDisplay {
    /* Display state for this core display. */
    DisplayState*       ds;

    /* Framebuffer for this core display. */
    QFrameBuffer*       fb;

    /* Framebuffer service associated with this core display. */
    CoreFramebuffer*    core_fb;
};

/* One and only one core display instance. */
CoreDisplay core_display;

/*
 * Framebuffer calls this routine when it detects changes. This routine will
 * initiate a "push" of the framebuffer changes to the UI.
 * See QFrameBufferUpdateFunc in framebuffer.h for more info on this callback.
 */
static void
coredisplay_fb_update(void* opaque, int x, int y, int w, int h)
{
    CoreDisplay* cd = (CoreDisplay*)opaque;
    if (cd->core_fb) {
        corefb_update(cd->core_fb, cd->ds, cd->fb, x, y, w, h);
    }
}

/*
 * Framebuffer callback. See QFrameBufferRotateFunc in framebuffer.h for more
 * info on this callback.
 */
static void
coredisplay_fb_rotate(void* opaque, int rotation)
{
}

/*
 * Framebuffer callback. See QFrameBufferPollFunc in framebuffer.h for more
 * info on this callback.
 */
static void
coredisplay_fb_poll(void* opaque)
{
    // This will eventually call core_display_fb_update.
    qframebuffer_check_updates();
}

/*
 * Framebuffer callback. See QFrameBufferDoneFunc in framebuffer.h for more
 * info on this callback.
 */
static void
coredisplay_fb_done(void* opaque)
{
}

void
coredisplay_init(DisplayState* ds)
{
    core_display.ds = ds;
    /* Create and initialize framebuffer instance that will be used for core
     * display.
     */
    ANEW0(core_display.fb);
    core_display.core_fb = NULL;
    qframebuffer_init(core_display.fb, ds->surface->width, ds->surface->height,
                      0, QFRAME_BUFFER_RGB565 );
    qframebuffer_fifo_add(core_display.fb);
    /* Register core display as the client for the framebuffer, so we can start
     * receiving framebuffer notifications. Note that until UI connects to the
     * core all framebuffer callbacks are essentially no-ops.
     */
    qframebuffer_add_client(core_display.fb, &core_display,
                            coredisplay_fb_update, coredisplay_fb_rotate,
                            coredisplay_fb_poll, coredisplay_fb_done);
    android_display_init(ds, core_display.fb);
}

int
coredisplay_attach_fb_service(CoreFramebuffer* core_fb)
{
    if (core_display.core_fb == NULL) {
        core_display.core_fb = core_fb;
        return 0;
    } else {
        return -1;
    }
}

CoreFramebuffer*
coredisplay_detach_fb_service(void)
{
    CoreFramebuffer* ret = core_display.core_fb;
    core_display.core_fb = NULL;
    return ret;
}

QFrameBuffer*
coredisplay_get_framebuffer(void)
{
    return core_display.fb;
}
