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
#ifndef _SKIN_WINDOW_H
#define _SKIN_WINDOW_H

#include "skin_file.h"
#include "skin_trackball.h"
#include "android_utils.h"

#include "vl.h"
#include <SDL.h>

typedef struct SkinWindow  SkinWindow;

extern SkinWindow*      skin_window_create( SkinLayout*  layout,
                                            int          x,
                                            int          y,
                                            double       scale,
                                            int          no_display );

extern int              skin_window_reset ( SkinWindow*  window, SkinLayout*  layout );
extern void             skin_window_free  ( SkinWindow*  window );
extern void             skin_window_redraw( SkinWindow*  window, SkinRect*  rect );
extern void             skin_window_process_event( SkinWindow*  window, SDL_Event*  ev );

extern void             skin_window_set_onion( SkinWindow*   window,
                                               SkinImage*    onion,
                                               SkinRotation  rotation,
                                               int           alpha );

extern void             skin_window_set_scale( SkinWindow*  window,
                                               double       scale );

extern void             skin_window_set_title( SkinWindow*  window,
                                               const char*  title );

extern void             skin_window_set_trackball( SkinWindow*  window, SkinTrackBall*  ball );
extern void             skin_window_toggle_trackball( SkinWindow*  window );
extern void             skin_window_toggle_fullscreen( SkinWindow*  window );

typedef struct {
    int           width;
    int           height;
    SkinRotation  rotation;
    void*         data;
} ADisplayInfo;

extern void             skin_window_get_display( SkinWindow*  window, ADisplayInfo  *info );
extern void             skin_window_update_display( SkinWindow*  window, int  x, int  y, int  w, int  h );

#endif /* _SKIN_WINDOW_H */
