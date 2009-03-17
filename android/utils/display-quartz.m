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

/* this is the Quartz-specific implementation of
 * <android/utils/display.h>
 */

#include "android/utils/display.h"
#include "android/utils/debug.h"

#define  D(...)   VERBOSE_PRINT(init,__VA_ARGS__)

#include <stdio.h>
#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <SDL_syswm.h>

int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    fprintf(stderr, "emulator: FIXME: implement get_monitor_resolution on OS X\n" );
    return -1;
}

int
get_nearest_monitor_rect( int  *x, int  *y, int  *width, int  *height )
{
    SDL_SysWMinfo  info;
    NSWindow*      window;

    SDL_VERSION(&info.version);
    if ( SDL_GetWMInfo(&info) < 0 ) {
        D( "%s: SDL_GetWMInfo() failed: %s", __FUNCTION__, SDL_GetError());
        return -1;
    }
    window = info.nsWindowPtr;
    if (window == NULL) {
        D( "%s: SDL_GetWMInfo() returned NULL NSWindow ptr",
           __FUNCTION__ );
        return -1;
    }
    else
    {
        NSRect        frame   = [ window frame ];
        int           fx1     = frame.origin.x;
        int           fy1     = frame.origin.y;
        int           fx2     = frame.size.width + fx1;
        int           fy2     = frame.size.height + fy1; 
        NSArray*      screens = [ NSScreen screens ];
        unsigned int  count   = [ screens count ];
        int           bestScreen = -1;
        int           bestArea = 0;

        unsigned int  n;
        printf( "window frame (%d,%d) (%d,%d)\n", fx1, fy1, fx2, fy2 );

        /* we need to compute which screen has the most window pixels */
        for (n = 0; n < count; n++) {
            NSScreen*  screen = [ screens objectAtIndex: n ];
            NSRect     vis    = [ screen visibleFrame ];
            int        vx1    = vis.origin.x;
            int        vy1    = vis.origin.y;
            int        vx2    = vis.size.width + vx1;
            int        vy2    = vis.size.height + vy1;
            int        cx1, cx2, cy1, cy2, cArea;

            //printf( "screen %d/%d  frame (%d,%d) (%d,%d)\n", n+1, count,
            //        vx1, vy1, vx2, vy2 );

            if (fx1 >= vx2 || vx1 >= fx2 || fy1 >= vy2 || vy1 >= fy2)
                continue;

            cx1 = (fx1 < vx1) ? vx1 : fx1;
            cx2 = (fx2 > vx2) ? vx2 : fx2;
            cy1 = (fy1 < vy1) ? vy1 : fy1;
            cy2 = (fy2 > vy2) ? vy2 : fy2;

            if (cx1 >= cx2 || cy1 >= cy2)
                continue;

            cArea = (cx2-cx1)*(cy2-cy1);

            if (bestScreen < 0 || cArea > bestArea) {
                bestScreen = n;
                bestArea   = cArea;
            }
        }
        if (bestScreen < 0)
            bestScreen = 0;

        {
            NSScreen*  screen = [ screens objectAtIndex: bestScreen ];
            NSRect     vis    = [ screen visibleFrame ];

            *x      = vis.origin.x;
            *y      = vis.origin.y;
            *width  = vis.size.width;
            *height = vis.size.height; 
        }
        return 0;
    }
};
