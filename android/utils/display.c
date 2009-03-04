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

#include "android/utils/display.h"
#include "android/utils/debug.h"

#define  D(...)   VERBOSE_PRINT(init,__VA_ARGS__)

/** HOST RESOLUTION SETTINGS
 **
 ** return the main monitor's DPI resolution according to the host device
 ** beware: this is not always reliable or even obtainable.
 **
 ** returns 0 on success, or -1 in case of error (e.g. the system returns funky values)
 **/

/** NOTE: the following code assumes that we exclusively use X11 on Linux, and Quartz on OS X
 **/

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL_syswm.h>

int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    HDC  displayDC = CreateDC( "DISPLAY", NULL, NULL, NULL );
    int  xdpi, ydpi;

    if (displayDC == NULL) {
        D( "%s: could not get display DC\n", __FUNCTION__ );
        return -1;
    }
    xdpi = GetDeviceCaps( displayDC, LOGPIXELSX );
    ydpi = GetDeviceCaps( displayDC, LOGPIXELSY );

    /* sanity checks */
    if (xdpi < 20 || xdpi > 400 || ydpi < 20 || ydpi > 400) {
        D( "%s: bad resolution: xpi=%d ydpi=%d", __FUNCTION__,
                xdpi, ydpi );
        return -1;
    }

    *px_dpi = xdpi;
    *py_dpi = ydpi;
    return 0;
}

int
get_nearest_monitor_rect( int  *x, int  *y, int  *width, int  *height )
{
    SDL_SysWMinfo  info;
    HMONITOR       monitor;
    MONITORINFO    monitorInfo;

    SDL_VERSION(&info.version);

    if ( !SDL_GetWMInfo(&info) ) {
        D( "%s: SDL_GetWMInfo() failed: %s", __FUNCTION__, SDL_GetError());
        return -1;
    }

    monitor = MonitorFromWindow( info.window, MONITOR_DEFAULTTONEAREST );
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo( monitor, &monitorInfo );

    *x      = monitorInfo.rcMonitor.left;
    *y      = monitorInfo.rcMonitor.top;
    *width  = monitorInfo.rcMonitor.right - *x;
    *height = monitorInfo.rcMonitor.bottom - *y;

    D("%s: found (x,y,w,h)=(%d,%d,%d,%d)", __FUNCTION__,
      *x, *y, *width, *height);

    return 0;
}


#elif defined __APPLE__

/* the real implementation is in display-quartz.m, but
 * unfortunately, the Android build system doesn't know
 * how to build Objective-C sources, so provide stubs
 * here instead.
 *
 * CONFIG_NO_COCOA is set by Makefile.android
 */

#ifdef CONFIG_NO_COCOA
int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    return -1;
}

int
get_nearest_monitor_rect( int  *x, int  *y, int  *width, int  *height )
{
    return -1;
}
#endif /* CONFIG_NO_COCOA */

#else  /* Linux and others */
#include <SDL.h>
#include <SDL_syswm.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#define  MM_PER_INCH   25.4

#define  DYNLINK_FUNCTIONS  \
    DYNLINK_FUNC(int,XDefaultScreen,(Display*)) \
    DYNLINK_FUNC(int,XDisplayWidth,(Display*,int)) \
    DYNLINK_FUNC(int,XDisplayWidthMM,(Display*,int)) \
    DYNLINK_FUNC(int,XDisplayHeight,(Display*,int)) \
    DYNLINK_FUNC(int,XDisplayHeightMM,(Display*,int)) \

#define  DYNLINK_FUNCTIONS_INIT \
    x11_dynlink_init

#include "dynlink.h"

static int    x11_lib_inited;
static void*  x11_lib;

int
x11_lib_init( void )
{
    if (!x11_lib_inited) {
        x11_lib_inited = 1;

        x11_lib = dlopen( "libX11.so", RTLD_NOW );

        if (x11_lib == NULL) {
            x11_lib = dlopen( "libX11.so.6", RTLD_NOW );
        }
        if (x11_lib == NULL) {
            D("%s: Could not find libX11.so on this machine",
              __FUNCTION__);
            return -1;
        }

        if (x11_dynlink_init(x11_lib) < 0) {
            D("%s: didn't find necessary symbols in libX11.so",
              __FUNCTION__);
            dlclose(x11_lib);
            x11_lib = NULL;
        }
    }
    return x11_lib ? 0 : -1;
}


int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    SDL_SysWMinfo info;
    Display*      display;
    int           screen;
    int           width, width_mm, height, height_mm, xdpi, ydpi;

    SDL_VERSION(&info.version);

    if ( !SDL_GetWMInfo(&info) ) {
        D( "%s: SDL_GetWMInfo() failed: %s", __FUNCTION__, SDL_GetError());
        return -1;
    }

    if (x11_lib_init() < 0)
        return -1;

    display = info.info.x11.display;
    screen  = FF(XDefaultScreen)(display);

    width     = FF(XDisplayWidth)(display, screen);
    width_mm  = FF(XDisplayWidthMM)(display, screen);
    height    = FF(XDisplayHeight)(display, screen);
    height_mm = FF(XDisplayHeightMM)(display, screen);

    if (width_mm <= 0 || height_mm <= 0) {
        D( "%s: bad screen dimensions: width_mm = %d, height_mm = %d",
                __FUNCTION__, width_mm, height_mm);
        return -1;
    }

    D( "%s: found screen width=%d height=%d width_mm=%d height_mm=%d",
            __FUNCTION__, width, height, width_mm, height_mm );

    xdpi = width  * MM_PER_INCH / width_mm;
    ydpi = height * MM_PER_INCH / height_mm;

    if (xdpi < 20 || xdpi > 400 || ydpi < 20 || ydpi > 400) {
        D( "%s: bad resolution: xpi=%d ydpi=%d", __FUNCTION__,
                xdpi, ydpi );
        return -1;
    }

    *px_dpi = xdpi;
    *py_dpi = ydpi;

    return 0;
}

int
get_nearest_monitor_rect( int  *x, int *y, int  *width, int  *height )
{
    SDL_SysWMinfo info;
    Display*      display;
    int           screen;

    SDL_VERSION(&info.version);

    if ( !SDL_GetWMInfo(&info) ) {
        D( "%s: SDL_GetWMInfo() failed: %s", __FUNCTION__, SDL_GetError());
        return -1;
    }

    if (x11_lib_init() < 0)
        return -1;

    display = info.info.x11.display;
    screen  = FF(XDefaultScreen)(display);

    *x      = 0;
    *y      = 0;
    *width  = FF(XDisplayWidth)(display, screen);
    *height = FF(XDisplayHeight)(display, screen);

    D("%s: found (x,y,w,h)=(%d,%d,%d,%d)", __FUNCTION__,
      *x, *y, *width, *height);

    return 0;
}

#endif
