/* Copyright (C) 2006-2010 The Android Open Source Project
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

#include "android/utils/debug.h"
#include "android/utils/bufprint.h"
#include "android/globals.h"
#include "android/qemulator.h"
#include "android/ui-core-protocol.h"

#define  D(...)  do {  if (VERBOSE_CHECK(init)) dprint(__VA_ARGS__); } while (0)
static double get_default_scale( AndroidOptions*  opts );

/* QEmulator structure instance. */
static QEmulator   qemulator[1];

static void
qemulator_light_brightness( void* opaque, const char*  light, int  value )
{
    QEmulator*  emulator = opaque;

    VERBOSE_PRINT(hw_control,"%s: light='%s' value=%d window=%p", __FUNCTION__, light, value, emulator->window);
    if ( !strcmp(light, "lcd_backlight") ) {
        emulator->lcd_brightness = value;
        if (emulator->window)
            skin_window_set_lcd_brightness( emulator->window, value );
        return;
    }
}

static void
qemulator_setup( QEmulator*  emulator )
{
    AndroidOptions*  opts = emulator->opts;

    if ( !emulator->window && !opts->no_window ) {
        SkinLayout*  layout = emulator->layout;
        double       scale  = get_default_scale(emulator->opts);

        emulator->window = skin_window_create( layout, emulator->win_x, emulator->win_y, scale, 0);
        if (emulator->window == NULL)
            return;

        {
            SkinTrackBall*           ball;
            SkinTrackBallParameters  params;

            params.diameter   = 30;
            params.ring       = 2;
            params.ball_color = 0xffe0e0e0;
            params.dot_color  = 0xff202020;
            params.ring_color = 0xff000000;

            ball = skin_trackball_create( &params );
            emulator->trackball = ball;
            skin_window_set_trackball( emulator->window, ball );

            emulator->lcd_brightness = 128;  /* 50% */
            skin_window_set_lcd_brightness( emulator->window, emulator->lcd_brightness );
        }

        if ( emulator->onion != NULL )
            skin_window_set_onion( emulator->window,
                                   emulator->onion,
                                   emulator->onion_rotation,
                                   emulator->onion_alpha );

        qemulator_set_title(emulator);

        skin_window_enable_touch ( emulator->window, android_hw->hw_touchScreen != 0 );
        skin_window_enable_dpad  ( emulator->window, android_hw->hw_dPad != 0 );
        skin_window_enable_qwerty( emulator->window, android_hw->hw_keyboard != 0 );
        skin_window_enable_trackball( emulator->window, android_hw->hw_trackBall != 0 );
    }

    /* initialize hardware control support */
    android_core_set_brightness_change_callback(qemulator_light_brightness,
                                                emulator);
}

static void
qemulator_fb_update( void*   _emulator, int  x, int  y, int  w, int  h )
{
    QEmulator*  emulator = _emulator;

    if (emulator->window)
        skin_window_update_display( emulator->window, x, y, w, h );
}

static void
qemulator_fb_rotate( void*  _emulator, int  rotation )
{
    QEmulator*     emulator = _emulator;

    qemulator_setup( emulator );
}

QEmulator*
qemulator_get(void)
{
    return qemulator;
}

int
qemulator_init( QEmulator*       emulator,
                AConfig*         aconfig,
                const char*      basepath,
                int              x,
                int              y,
                AndroidOptions*  opts )
{
    emulator->aconfig     = aconfig;
    emulator->layout_file = skin_file_create_from_aconfig(aconfig, basepath);
    emulator->layout      = emulator->layout_file->layouts;
    // If we have a custom charmap use it to initialize keyboard.
    // Otherwise initialize keyboard from configuration settings.
    // Another way to configure keyboard to use a custom charmap would
    // be saving a custom charmap name into AConfig's keyboard->charmap
    // property, and calling single skin_keyboard_create_from_aconfig
    // routine to initialize keyboard.
    if (NULL != opts->charmap) {
        emulator->keyboard = skin_keyboard_create_from_kcm(opts->charmap, opts->raw_keys);
    } else {
        emulator->keyboard = skin_keyboard_create_from_aconfig(aconfig, opts->raw_keys);
    }
    emulator->window      = NULL;
    emulator->win_x       = x;
    emulator->win_y       = y;
    emulator->opts[0]     = opts[0];

    /* register as a framebuffer clients for all displays defined in the skin file */
    SKIN_FILE_LOOP_PARTS( emulator->layout_file, part )
        SkinDisplay*  disp = part->display;
        if (disp->valid) {
            qframebuffer_add_client( disp->qfbuff,
                                        emulator,
                                        qemulator_fb_update,
                                        qemulator_fb_rotate,
                                        NULL );
        }
    SKIN_FILE_LOOP_END_PARTS
    return 0;
}

void
qemulator_done(QEmulator* emulator)
{
    if (emulator->window) {
        skin_window_free(emulator->window);
        emulator->window = NULL;
    }
    if (emulator->trackball) {
        skin_trackball_destroy(emulator->trackball);
        emulator->trackball = NULL;
    }
    if (emulator->keyboard) {
        skin_keyboard_free(emulator->keyboard);
        emulator->keyboard = NULL;
    }
    emulator->layout = NULL;
    if (emulator->layout_file) {
        skin_file_free(emulator->layout_file);
        emulator->layout_file = NULL;
    }
}

SkinLayout*
qemulator_get_layout(QEmulator* emulator)
{
    return emulator->layout;
}

void
qemulator_set_title(QEmulator* emulator)
{
    char  temp[128], *p=temp, *end=p+sizeof temp;;

    if (emulator->window == NULL)
        return;

    if (emulator->show_trackball) {
        SkinKeyBinding  bindings[ SKIN_KEY_COMMAND_MAX_BINDINGS ];
        int             count;

        count = skin_keyset_get_bindings( android_keyset,
                                          SKIN_KEY_COMMAND_TOGGLE_TRACKBALL,
                                          bindings );

        if (count > 0) {
            int  nn;
            p = bufprint( p, end, "Press " );
            for (nn = 0; nn < count; nn++) {
                if (nn > 0) {
                    if (nn < count-1)
                        p = bufprint(p, end, ", ");
                    else
                        p = bufprint(p, end, " or ");
                }
                p = bufprint(p, end, "%s",
                             skin_key_symmod_to_str( bindings[nn].sym,
                                                     bindings[nn].mod ) );
            }
            p = bufprint(p, end, " to leave trackball mode. ");
        }
    }

    p = bufprint(p, end, "%d:%s",
                 android_core_get_base_port(),
                 avdInfo_getName( android_avdInfo ));

    skin_window_set_title( emulator->window, temp );
}

/*
 * Helper routines
 */

int
get_device_dpi( AndroidOptions*  opts )
{
    int    dpi_device  = android_core_get_hw_lcd_density();

    if (opts->dpi_device != NULL) {
        char*  end;
        dpi_device = strtol( opts->dpi_device, &end, 0 );
        if (end == NULL || *end != 0 || dpi_device <= 0) {
            fprintf(stderr, "argument for -dpi-device must be a positive integer. Aborting\n" );
            exit(1);
        }
    }
    return  dpi_device;
}

static double
get_default_scale( AndroidOptions*  opts )
{
    int     dpi_device  = get_device_dpi( opts );
    int     dpi_monitor = -1;
    double  scale       = 0.0;

    /* possible values for the 'scale' option are
     *   'auto'        : try to determine the scale automatically
     *   '<number>dpi' : indicates the host monitor dpi, compute scale accordingly
     *   '<fraction>'  : use direct scale coefficient
     */

    if (opts->scale) {
        if (!strcmp(opts->scale, "auto"))
        {
            /* we need to get the host dpi resolution ? */
            int   xdpi, ydpi;

            if ( SDL_WM_GetMonitorDPI( &xdpi, &ydpi ) < 0 ) {
                fprintf(stderr, "could not get monitor DPI resolution from system. please use -dpi-monitor to specify one\n" );
                exit(1);
            }
            D( "system reported monitor resolutions: xdpi=%d ydpi=%d\n", xdpi, ydpi);
            dpi_monitor = (xdpi + ydpi+1)/2;
        }
        else
        {
            char*   end;
            scale = strtod( opts->scale, &end );

            if (end && end[0] == 'd' && end[1] == 'p' && end[2] == 'i' && end[3] == 0) {
                if ( scale < 20 || scale > 1000 ) {
                    fprintf(stderr, "emulator: ignoring bad -scale argument '%s': %s\n", opts->scale,
                            "host dpi number must be between 20 and 1000" );
                    exit(1);
                }
                dpi_monitor = scale;
                scale       = 0.0;
            }
            else if (end == NULL || *end != 0) {
                fprintf(stderr, "emulator: ignoring bad -scale argument '%s': %s\n", opts->scale,
                        "not a number or the 'auto' keyword" );
                exit(1);
            }
            else if ( scale < 0.1 || scale > 3. ) {
                fprintf(stderr, "emulator: ignoring bad -window-scale argument '%s': %s\n", opts->scale,
                        "must be between 0.1 and 3.0" );
                exit(1);
            }
        }
    }

    if (scale == 0.0 && dpi_monitor > 0)
        scale = dpi_monitor*1.0/dpi_device;

    if (scale == 0.0)
        scale = 1.0;

    return scale;
}

/*
 * android/console.c helper routines.
 */

SkinKeyboard*
android_emulator_get_keyboard(void)
{
    return qemulator->keyboard;
}

void
android_emulator_set_window_scale( double  scale, int  is_dpi )
{
    QEmulator*  emulator = qemulator;

    if (is_dpi)
        scale /= get_device_dpi( emulator->opts );

    if (emulator->window)
        skin_window_set_scale( emulator->window, scale );
}

