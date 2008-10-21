/* Copyright (C) 2007-2008 The Android Open Source Project
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
#ifndef _QEMU_FRAMEBUFFER_H_
#define _QEMU_FRAMEBUFFER_H_

/* a simple interface to a framebuffer display. this is to be used by the hardware framebuffer
 * driver (e.g. hw/goldfish_fb.c) to send VRAM updates to the emulator.
 *
 * note the 'rotation' field: it can take values 0, 1, 2 or 3 and corresponds to a rotation
 * that must be performed to the pixels stored in the framebuffer *before* displaying them
 * a value of 1 corresponds to a rotation of 90 clockwise-degrees, when the framebuffer is
 * rotated 90 or 270 degrees, its width/height are swapped automatically
 *
 * phys_width_mm and phys_height_mm are physical dimensions expressed in millimeters
 *
 * each QFrameBuffer can have one "client" that reacts to VRAM updates or the framebuffer
 * rotations requested by the system.
 */
typedef struct QFrameBuffer   QFrameBuffer;


typedef enum {
    QFRAME_BUFFER_NONE   = 0,
    QFRAME_BUFFER_RGB565 = 1,
    QFRAME_BUFFER_MAX          /* do not remove */
} QFrameBufferFormat;

struct QFrameBuffer {
    int                 width;        /* width in pixels */
    int                 height;       /* height in pixels */
    int                 pitch;        /* bytes per line */
    int                 rotation;     /* rotation to be applied when displaying */
    QFrameBufferFormat  format;
    void*               pixels;       /* pixel buffer */

    int                 phys_width_mm;
    int                 phys_height_mm;

    /* extra data that is handled by the framebuffer implementation */
    void*               extra;

};

/* the default dpi resolution of a typical framebuffer. this is an average between
 * various prototypes being used during the development of the Android system...
 */
#define  DEFAULT_FRAMEBUFFER_DPI   165


/* initialize a framebuffer object and allocate its pixel buffer */
/* this computes phys_width_mm and phys_height_mm assuming a 165 dpi screen */
/* returns -1 in case of error, 0 otherwise */
extern int
qframebuffer_init( QFrameBuffer*       qfbuff,
                   int                 width,
                   int                 height,
                   int                 rotation,
                   QFrameBufferFormat  format );

/* recompute phys_width_mm and phys_height_mm according to the emulated screen DPI settings */
extern void
qframebuffer_set_dpi( QFrameBuffer*   qfbuff,
                      int             x_dpi,
                      int             y_dpi );

/* alternative to qframebuffer_set_dpi where one can set the physical dimensions directly */
/* in millimeters. for the record 1 inch = 25.4 mm */
extern void
qframebuffer_set_mm( QFrameBuffer*   qfbuff,
                     int             width_mm,
                     int             height_mm );

/* add one client to a given framebuffer */
/* client functions */
typedef void (*QFrameBufferUpdateFunc)( void*  opaque, int  x, int  y, int  w, int  h );
typedef void (*QFrameBufferRotateFunc)( void*  opaque, int  rotation );
typedef void (*QFrameBufferDoneFunc)  ( void*  opaque );

extern void
qframebuffer_add_client( QFrameBuffer*           qfbuff,
                         void*                   fb_opaque,
                         QFrameBufferUpdateFunc  fb_update,
                         QFrameBufferRotateFunc  fb_rotate,
                         QFrameBufferDoneFunc    fb_done );

/* add one producer to a given framebuffer */
/* producer functions */
typedef void (*QFrameBufferCheckUpdateFunc)( void*  opaque );
typedef void (*QFrameBufferInvalidateFunc) ( void*  opaque );
typedef void (*QFrameBufferDetachFunc)     ( void*  opaque );

extern void
qframebuffer_add_producer( QFrameBuffer*                qfbuff,
                           void*                        opaque,
                           QFrameBufferCheckUpdateFunc  fb_check,
                           QFrameBufferInvalidateFunc   fb_invalidate,
                           QFrameBufferDetachFunc       fb_detach );

/* tell a client that a rectangle region has been updated in the framebuffer pixel buffer */
extern void
qframebuffer_update( QFrameBuffer*  qfbuff, int  x, int  y, int  w, int  h );

/* rotate the framebuffer (may swap width/height), and tell a client that we did */
extern void
qframebuffer_rotate( QFrameBuffer*  qfbuff, int  rotation );

/* finalize a framebuffer, release its pixel buffer */
extern void
qframebuffer_done( QFrameBuffer*   qfbuff );


/*
 * QFrameBuffer objects are created by the emulated system, its characteristics typically
 * depend on the current device skin being used.
 *
 * there are also used by emulated framebuffer devices, who don't know much about all this
 *
 * use a simple fifo to bridge these together
 */

/* add a new constructed frame buffer object to our global list */
extern void
qframebuffer_fifo_add( QFrameBuffer*  qfbuff );

extern QFrameBuffer*
qframebuffer_fifo_get( void );

/*
 *  check all registered framebuffers for updates. tgus wukk cakk tge ckuebt's update
 *  functions in the end...
 */

extern void
qframebuffer_check_updates( void );

extern void
qframebuffer_invalidate_all( void );

#endif /* _QEMU_FRAMEBUFFER_H_ */

