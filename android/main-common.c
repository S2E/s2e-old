/* Copyright (C) 2011 The Android Open Source Project
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
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include <process.h>
#endif

#include <SDL.h>
#include <SDL_syswm.h>

#include "console.h"

#include "android/utils/debug.h"
#include "android/utils/path.h"
#include "android/utils/bufprint.h"
#include "android/main-common.h"
#include "android/globals.h"
#include "android/resource.h"
#include "android/user-config.h"
#include "android/qemulator.h"
#include "android/display.h"
#include "android/skin/image.h"
#include "android/skin/trackball.h"
#include "android/skin/keyboard.h"
#include "android/skin/file.h"
#include "android/skin/window.h"



/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            U T I L I T Y   R O U T I N E S                  *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

#define  D(...)  do {  if (VERBOSE_CHECK(init)) dprint(__VA_ARGS__); } while (0)

/***  CONFIGURATION
 ***/

static AUserConfig*  userConfig;

void
emulator_config_init( void )
{
    userConfig = auserConfig_new( android_avdInfo );
}

/* only call this function on normal exits, so that ^C doesn't save the configuration */
void
emulator_config_done( void )
{
    int  win_x, win_y;

    if (!userConfig) {
        D("no user configuration?");
        return;
    }

    SDL_WM_GetPos( &win_x, &win_y );
    auserConfig_setWindowPos(userConfig, win_x, win_y);
    auserConfig_save(userConfig);
}

void
emulator_config_get_window_pos( int *window_x, int *window_y )
{
    *window_x = *window_y = 10;

    if (userConfig)
        auserConfig_getWindowPos(userConfig, window_x, window_y);
}

unsigned convertBytesToMB( uint64_t  size )
{
    if (size == 0)
        return 0;

    size = (size + ONE_MB-1) >> 20;
    if (size > UINT_MAX)
        size = UINT_MAX;

    return (unsigned) size;
}

uint64_t convertMBToBytes( unsigned  megaBytes )
{
    return ((uint64_t)megaBytes << 20);
}


/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            K E Y S E T   R O U T I N E S                    *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

#define  KEYSET_FILE    "default.keyset"

SkinKeyset*  android_keyset = NULL;

static int
load_keyset(const char*  path)
{
    if (path_can_read(path)) {
        AConfig*  root = aconfig_node("","");
        if (!aconfig_load_file(root, path)) {
            android_keyset = skin_keyset_new(root);
            if (android_keyset != NULL) {
                D( "keyset loaded from: %s", path);
                return 0;
            }
        }
    }
    return -1;
}

void
parse_keyset(const char*  keyset, AndroidOptions*  opts)
{
    char   kname[MAX_PATH];
    char   temp[MAX_PATH];
    char*  p;
    char*  end;

    /* append .keyset suffix if needed */
    if (strchr(keyset, '.') == NULL) {
        p   =  kname;
        end = p + sizeof(kname);
        p   = bufprint(p, end, "%s.keyset", keyset);
        if (p >= end) {
            derror( "keyset name too long: '%s'\n", keyset);
            exit(1);
        }
        keyset = kname;
    }

    /* look for a the keyset file */
    p   = temp;
    end = p + sizeof(temp);
    p = bufprint_config_file(p, end, keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    p = temp;
    p = bufprint(p, end, "%s" PATH_SEP "keysets" PATH_SEP "%s", opts->sysdir, keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    p = temp;
    p = bufprint_app_dir(p, end);
    p = bufprint(p, end, PATH_SEP "keysets" PATH_SEP "%s", keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    return;
}

void
write_default_keyset( void )
{
    char   path[MAX_PATH];

    bufprint_config_file( path, path+sizeof(path), KEYSET_FILE );

    /* only write if there is no file here */
    if ( !path_exists(path) ) {
        int          fd = open( path, O_WRONLY | O_CREAT, 0666 );
        int          ret;
        const char*  ks = skin_keyset_get_default();


        D( "writing default keyset file to %s", path );

        if (fd < 0) {
            D( "%s: could not create file: %s", __FUNCTION__, strerror(errno) );
            return;
        }
        CHECKED(ret, write(fd, ks, strlen(ks)));
        close(fd);
    }
}



/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            S D L   S U P P O R T                            *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

void *readpng(const unsigned char*  base, size_t  size, unsigned *_width, unsigned *_height);

#ifdef CONFIG_DARWIN
#  define  ANDROID_ICON_PNG  "android_icon_256.png"
#else
#  define  ANDROID_ICON_PNG  "android_icon_16.png"
#endif

static void
sdl_set_window_icon( void )
{
    static int  window_icon_set;

    if (!window_icon_set)
    {
#ifdef _WIN32
        HANDLE         handle = GetModuleHandle( NULL );
        HICON          icon   = LoadIcon( handle, MAKEINTRESOURCE(1) );
        SDL_SysWMinfo  wminfo;

        SDL_GetWMInfo(&wminfo);

        SetClassLong( wminfo.window, GCL_HICON, (LONG)icon );
#else  /* !_WIN32 */
        unsigned              icon_w, icon_h;
        size_t                icon_bytes;
        const unsigned char*  icon_data;
        void*                 icon_pixels;

        window_icon_set = 1;

        icon_data = android_icon_find( ANDROID_ICON_PNG, &icon_bytes );
        if ( !icon_data )
            return;

        icon_pixels = readpng( icon_data, icon_bytes, &icon_w, &icon_h );
        if ( !icon_pixels )
            return;

       /* the data is loaded into memory as RGBA bytes by libpng. we want to manage
        * the values as 32-bit ARGB pixels, so swap the bytes accordingly depending
        * on our CPU endianess
        */
        {
            unsigned*  d     = icon_pixels;
            unsigned*  d_end = d + icon_w*icon_h;

            for ( ; d < d_end; d++ ) {
                unsigned  pix = d[0];
#if HOST_WORDS_BIGENDIAN
                /* R,G,B,A read as RGBA => ARGB */
                pix = ((pix >> 8) & 0xffffff) | (pix << 24);
#else
                /* R,G,B,A read as ABGR => ARGB */
                pix = (pix & 0xff00ff00) | ((pix >> 16) & 0xff) | ((pix & 0xff) << 16);
#endif
                d[0] = pix;
            }
        }

        SDL_Surface* icon = sdl_surface_from_argb32( icon_pixels, icon_w, icon_h );
        if (icon != NULL) {
            SDL_WM_SetIcon(icon, NULL);
            SDL_FreeSurface(icon);
            free( icon_pixels );
        }
#endif  /* !_WIN32 */
    }
}

/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            S K I N   S U P P O R T                          *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

const char*  skin_network_speed = NULL;
const char*  skin_network_delay = NULL;


static void sdl_at_exit(void)
{
    emulator_config_done();
    qemulator_done(qemulator_get());
    SDL_Quit();
}


void sdl_display_init(DisplayState *ds, int full_screen, int  no_frame)
{
    QEmulator*    emulator = qemulator_get();
    SkinDisplay*  disp     = skin_layout_get_display(emulator->layout);
    int           width, height;
    char          buf[128];

    if (disp->rotation & 1) {
        width  = disp->rect.size.h;
        height = disp->rect.size.w;
    } else {
        width  = disp->rect.size.w;
        height = disp->rect.size.h;
    }

    snprintf(buf, sizeof buf, "width=%d,height=%d", width, height);
    android_display_init(ds, qframebuffer_fifo_get());
}

/* list of skin aliases */
static const struct {
    const char*  name;
    const char*  alias;
} skin_aliases[] = {
    { "QVGA-L", "320x240" },
    { "QVGA-P", "240x320" },
    { "HVGA-L", "480x320" },
    { "HVGA-P", "320x480" },
    { "QVGA", "320x240" },
    { "HVGA", "320x480" },
    { NULL, NULL }
};

/* this is used by hw/events_device.c to send the charmap name to the system */
const char*    android_skin_keycharmap = NULL;

void init_skinned_ui(const char *path, const char *name, AndroidOptions*  opts)
{
    char      tmp[1024];
    AConfig*  root;
    AConfig*  n;
    int       win_x, win_y, flags;

    signal(SIGINT, SIG_DFL);
#ifndef _WIN32
    signal(SIGQUIT, SIG_DFL);
#endif

    /* we're not a game, so allow the screensaver to run */
    putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");

    flags = SDL_INIT_NOPARACHUTE;
    if (!opts->no_window)
        flags |= SDL_INIT_VIDEO;

    if(SDL_Init(flags)){
        fprintf(stderr, "SDL init failure, reason is: %s\n", SDL_GetError() );
        exit(1);
    }

    if (!opts->no_window) {
        SDL_EnableUNICODE(!opts->raw_keys);
        SDL_EnableKeyRepeat(0,0);

        sdl_set_window_icon();
    }
    else
    {
#ifndef _WIN32
       /* prevent SIGTTIN and SIGTTOUT from stopping us. this is necessary to be
        * able to run the emulator in the background (e.g. "emulator &").
        * despite the fact that the emulator should not grab input or try to
        * write to the output in normal cases, we're stopped on some systems
        * (e.g. OS X)
        */
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
#endif
    }
    atexit(sdl_at_exit);

    root = aconfig_node("", "");

    if(name) {
        /* Support skin aliases like QVGA-H QVGA-P, etc...
           But first we check if it's a directory that exist before applying
           the alias */
        int  checkAlias = 1;

        if (path != NULL) {
            bufprint(tmp, tmp+sizeof(tmp), "%s/%s", path, name);
            if (path_exists(tmp)) {
                checkAlias = 0;
            } else {
                D("there is no '%s' skin in '%s'", name, path);
            }
        }

        if (checkAlias) {
            int  nn;

            for (nn = 0; ; nn++ ) {
                const char*  skin_name  = skin_aliases[nn].name;
                const char*  skin_alias = skin_aliases[nn].alias;

                if ( !skin_name )
                    break;

                if ( !strcasecmp( skin_name, name ) ) {
                    D("skin name '%s' aliased to '%s'", name, skin_alias);
                    name = skin_alias;
                    break;
                }
            }
        }

        /* Magically support skins like "320x240" or "320x240x16" */
        if(isdigit(name[0])) {
            char *x = strchr(name, 'x');
            if(x && isdigit(x[1])) {
                int width = atoi(name);
                int height = atoi(x+1);
                int bpp   = 16;
                char* y = strchr(x+1, 'x');
                if (y && isdigit(y[1])) {
                    bpp = atoi(y+1);
                }
                sprintf(tmp,"display {\n  width %d\n  height %d\n bpp %d}\n",
                        width, height,bpp);
                aconfig_load(root, strdup(tmp));
                path = ":";
                goto found_a_skin;
            }
        }

        if (path == NULL) {
            derror("unknown skin name '%s'", name);
            exit(1);
        }

        sprintf(tmp, "%s/%s/layout", path, name);
        D("trying to load skin file '%s'", tmp);

        if(aconfig_load_file(root, tmp) >= 0) {
            sprintf(tmp, "%s/%s/", path, name);
            path = tmp;
            goto found_a_skin;
        } else {
            dwarning("could not load skin file '%s', using built-in one\n",
                     tmp);
        }
    }

    {
        const unsigned char*  layout_base;
        size_t                layout_size;

        name = "<builtin>";

        layout_base = android_resource_find( "layout", &layout_size );
        if (layout_base != NULL) {
            char*  base = malloc( layout_size+1 );
            memcpy( base, layout_base, layout_size );
            base[layout_size] = 0;

            D("parsing built-in skin layout file (size=%d)", (int)layout_size);
            aconfig_load(root, base);
            path = ":";
        } else {
            fprintf(stderr, "Couldn't load builtin skin\n");
            exit(1);
        }
    }

found_a_skin:
    emulator_config_get_window_pos(&win_x, &win_y);

    if ( qemulator_init(qemulator_get(), root, path, win_x, win_y, opts ) < 0 ) {
        fprintf(stderr, "### Error: could not load emulator skin '%s'\n", name);
        exit(1);
    }

    android_skin_keycharmap = skin_keyboard_charmap_name(qemulator_get()->keyboard);

    /* the default network speed and latency can now be specified by the device skin */
    n = aconfig_find(root, "network");
    if (n != NULL) {
        skin_network_speed = aconfig_str(n, "speed", 0);
        skin_network_delay = aconfig_str(n, "delay", 0);
    }

#if 0
    /* create a trackball if needed */
    n = aconfig_find(root, "trackball");
    if (n != NULL) {
        SkinTrackBallParameters  params;

        params.x        = aconfig_unsigned(n, "x", 0);
        params.y        = aconfig_unsigned(n, "y", 0);
        params.diameter = aconfig_unsigned(n, "diameter", 20);
        params.ring     = aconfig_unsigned(n, "ring", 1);

        params.ball_color = aconfig_unsigned(n, "ball-color", 0xffe0e0e0);
        params.dot_color  = aconfig_unsigned(n, "dot-color",  0xff202020 );
        params.ring_color = aconfig_unsigned(n, "ring-color", 0xff000000 );

        qemu_disp->trackball = skin_trackball_create( &params );
        skin_trackball_refresh( qemu_disp->trackball );
    }
#endif

    /* add an onion overlay image if needed */
    if (opts->onion) {
        SkinImage*  onion = skin_image_find_simple( opts->onion );
        int         alpha, rotate;

        if ( opts->onion_alpha && 1 == sscanf( opts->onion_alpha, "%d", &alpha ) ) {
            alpha = (256*alpha)/100;
        } else
            alpha = 128;

        if ( opts->onion_rotation && 1 == sscanf( opts->onion_rotation, "%d", &rotate ) ) {
            rotate &= 3;
        } else
            rotate = SKIN_ROTATION_0;

        qemulator_get()->onion          = onion;
        qemulator_get()->onion_alpha    = alpha;
        qemulator_get()->onion_rotation = rotate;
    }
}

