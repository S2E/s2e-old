/*
 * QEMU graphical console
 *
 * Copyright (c) 2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu-common.h"
#include "console.h"
#include "qemu-timer.h"
#include "android/utils/system.h"

//#define DEBUG_CONSOLE
#define DEFAULT_BACKSCROLL 512
#define MAX_CONSOLES 12

#define QEMU_RGBA(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define QEMU_RGB(r, g, b) QEMU_RGBA(r, g, b, 0xff)

typedef struct TextAttributes {
    uint8_t fgcol:4;
    uint8_t bgcol:4;
    uint8_t bold:1;
    uint8_t uline:1;
    uint8_t blink:1;
    uint8_t invers:1;
    uint8_t unvisible:1;
} TextAttributes;

typedef struct TextCell {
    uint8_t ch;
    TextAttributes t_attrib;
} TextCell;

#define MAX_ESC_PARAMS 3

enum TTYState {
    TTY_STATE_NORM,
    TTY_STATE_ESC,
    TTY_STATE_CSI,
};

typedef struct QEMUFIFO {
    uint8_t *buf;
    int buf_size;
    int count, wptr, rptr;
} QEMUFIFO;

typedef enum {
    GRAPHIC_CONSOLE,
    TEXT_CONSOLE,
    TEXT_CONSOLE_FIXED_SIZE
} console_type_t;

/* ??? This is mis-named.
   It is used for both text and graphical consoles.  */
struct TextConsole {
    console_type_t console_type;
    DisplayState *ds;
    /* Graphic console state.  */
    vga_hw_update_ptr hw_update;
    vga_hw_invalidate_ptr hw_invalidate;
    vga_hw_screen_dump_ptr hw_screen_dump;
    vga_hw_text_update_ptr hw_text_update;
    void *hw;

    int g_width, g_height;
    int width;
    int height;
    int total_height;
    int backscroll_height;
    int x, y;
    int x_saved, y_saved;
    int y_displayed;
    int y_base;
    TextAttributes t_attrib_default; /* default text attributes */
    TextAttributes t_attrib; /* currently active text attributes */
    TextCell *cells;
    int text_x[2], text_y[2], cursor_invalidate;

    int update_x0;
    int update_y0;
    int update_x1;
    int update_y1;

    enum TTYState state;
    int esc_params[MAX_ESC_PARAMS];
    int nb_esc_params;

    /* fifo for key pressed */
    QEMUFIFO out_fifo;
    uint8_t out_fifo_buf[16];
    QEMUTimer *kbd_timer;
};

static DisplayState *display_state;
static TextConsole *active_console;
static TextConsole *consoles[MAX_CONSOLES];
static int nb_consoles = 0;

#ifdef CONFIG_ANDROID
/* Graphic console width, height and bits per pixel.
 * These default values can be changed with the "-android-gui" option.
 */
int android_display_width   = 640;
int android_display_height  = 480;
int android_display_bpp     = 32;
#endif

void vga_hw_update(void)
{
    if (active_console && active_console->hw_update)
        active_console->hw_update(active_console->hw);
}

void vga_hw_invalidate(void)
{
    if (active_console && active_console->hw_invalidate)
        active_console->hw_invalidate(active_console->hw);
}

void vga_hw_screen_dump(const char *filename)
{
    TextConsole *previous_active_console;

    previous_active_console = active_console;
    active_console = consoles[0];
    /* There is currently no way of specifying which screen we want to dump,
       so always dump the first one.  */
    if (consoles[0]->hw_screen_dump)
        consoles[0]->hw_screen_dump(consoles[0]->hw, filename);
    active_console = previous_active_console;
}

void vga_hw_text_update(console_ch_t *chardata)
{
    if (active_console && active_console->hw_text_update)
        active_console->hw_text_update(active_console->hw, chardata);
}

/* convert a RGBA color to a color index usable in graphic primitives */
static unsigned int vga_get_color(DisplayState *ds, unsigned int rgba)
{
    unsigned int r, g, b, color;

    switch(ds_get_bits_per_pixel(ds)) {
#if 0
    case 8:
        r = (rgba >> 16) & 0xff;
        g = (rgba >> 8) & 0xff;
        b = (rgba) & 0xff;
        color = (rgb_to_index[r] * 6 * 6) +
            (rgb_to_index[g] * 6) +
            (rgb_to_index[b]);
        break;
#endif
    case 15:
        r = (rgba >> 16) & 0xff;
        g = (rgba >> 8) & 0xff;
        b = (rgba) & 0xff;
        color = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
        break;
    case 16:
        r = (rgba >> 16) & 0xff;
        g = (rgba >> 8) & 0xff;
        b = (rgba) & 0xff;
        color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        break;
    case 32:
    default:
        color = rgba;
        break;
    }
    return color;
}

/***********************************************************/
/* basic char display */

#define FONT_HEIGHT 16
#define FONT_WIDTH 8

#include "vgafont.h"

#define cbswap_32(__x) \
((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) ))

#ifdef HOST_WORDS_BIGENDIAN
#define PAT(x) x
#else
#define PAT(x) cbswap_32(x)
#endif

static const uint32_t dmask16[16] = {
    PAT(0x00000000),
    PAT(0x000000ff),
    PAT(0x0000ff00),
    PAT(0x0000ffff),
    PAT(0x00ff0000),
    PAT(0x00ff00ff),
    PAT(0x00ffff00),
    PAT(0x00ffffff),
    PAT(0xff000000),
    PAT(0xff0000ff),
    PAT(0xff00ff00),
    PAT(0xff00ffff),
    PAT(0xffff0000),
    PAT(0xffff00ff),
    PAT(0xffffff00),
    PAT(0xffffffff),
};

static const uint32_t dmask4[4] = {
    PAT(0x00000000),
    PAT(0x0000ffff),
    PAT(0xffff0000),
    PAT(0xffffffff),
};

static uint32_t color_table[2][8];

enum color_names {
    COLOR_BLACK   = 0,
    COLOR_RED     = 1,
    COLOR_GREEN   = 2,
    COLOR_YELLOW  = 3,
    COLOR_BLUE    = 4,
    COLOR_MAGENTA = 5,
    COLOR_CYAN    = 6,
    COLOR_WHITE   = 7
};

static const uint32_t color_table_rgb[2][8] = {
    {   /* dark */
        QEMU_RGB(0x00, 0x00, 0x00),  /* black */
        QEMU_RGB(0xaa, 0x00, 0x00),  /* red */
        QEMU_RGB(0x00, 0xaa, 0x00),  /* green */
        QEMU_RGB(0xaa, 0xaa, 0x00),  /* yellow */
        QEMU_RGB(0x00, 0x00, 0xaa),  /* blue */
        QEMU_RGB(0xaa, 0x00, 0xaa),  /* magenta */
        QEMU_RGB(0x00, 0xaa, 0xaa),  /* cyan */
        QEMU_RGB(0xaa, 0xaa, 0xaa),  /* white */
    },
    {   /* bright */
        QEMU_RGB(0x00, 0x00, 0x00),  /* black */
        QEMU_RGB(0xff, 0x00, 0x00),  /* red */
        QEMU_RGB(0x00, 0xff, 0x00),  /* green */
        QEMU_RGB(0xff, 0xff, 0x00),  /* yellow */
        QEMU_RGB(0x00, 0x00, 0xff),  /* blue */
        QEMU_RGB(0xff, 0x00, 0xff),  /* magenta */
        QEMU_RGB(0x00, 0xff, 0xff),  /* cyan */
        QEMU_RGB(0xff, 0xff, 0xff),  /* white */
    }
};

static inline unsigned int col_expand(DisplayState *ds, unsigned int col)
{
    switch(ds_get_bits_per_pixel(ds)) {
    case 8:
        col |= col << 8;
        col |= col << 16;
        break;
    case 15:
    case 16:
        col |= col << 16;
        break;
    default:
        break;
    }

    return col;
}
#ifdef DEBUG_CONSOLE
static void console_print_text_attributes(TextAttributes *t_attrib, char ch)
{
    if (t_attrib->bold) {
        printf("b");
    } else {
        printf(" ");
    }
    if (t_attrib->uline) {
        printf("u");
    } else {
        printf(" ");
    }
    if (t_attrib->blink) {
        printf("l");
    } else {
        printf(" ");
    }
    if (t_attrib->invers) {
        printf("i");
    } else {
        printf(" ");
    }
    if (t_attrib->unvisible) {
        printf("n");
    } else {
        printf(" ");
    }

    printf(" fg: %d bg: %d ch:'%2X' '%c'\n", t_attrib->fgcol, t_attrib->bgcol, ch, ch);
}
#endif

void console_select(unsigned int index)
{
    TextConsole *s;

    if (index >= MAX_CONSOLES)
        return;
    active_console->g_width = ds_get_width(active_console->ds);
    active_console->g_height = ds_get_height(active_console->ds);
    s = consoles[index];
    if (s) {
        DisplayState *ds = s->ds;
        active_console = s;
        if (ds_get_bits_per_pixel(s->ds)) {
            ds->surface = qemu_resize_displaysurface(ds, s->g_width, s->g_height);
        } else {
            s->ds->surface->width = s->width;
            s->ds->surface->height = s->height;
        }
        dpy_resize(s->ds);
        vga_hw_invalidate();
    }
}

static TextConsole *get_graphic_console(DisplayState *ds)
{
    int i;
    TextConsole *s;
    for (i = 0; i < nb_consoles; i++) {
        s = consoles[i];
        if (s->console_type == GRAPHIC_CONSOLE && s->ds == ds)
            return s;
    }
    return NULL;
}

static TextConsole *new_console(DisplayState *ds, console_type_t console_type)
{
    TextConsole *s;
    int i;

    if (nb_consoles >= MAX_CONSOLES)
        return NULL;
    ANEW0(s);
    if (!active_console || ((active_console->console_type != GRAPHIC_CONSOLE) &&
        (console_type == GRAPHIC_CONSOLE))) {
        active_console = s;
    }
    s->ds = ds;
    s->console_type = console_type;
    if (console_type != GRAPHIC_CONSOLE) {
        consoles[nb_consoles++] = s;
    } else {
        /* HACK: Put graphical consoles before text consoles.  */
        for (i = nb_consoles; i > 0; i--) {
            if (consoles[i - 1]->console_type == GRAPHIC_CONSOLE)
                break;
            consoles[i] = consoles[i - 1];
        }
        consoles[i] = s;
        nb_consoles++;
    }
    return s;
}

static DisplaySurface* defaultallocator_create_displaysurface(int width, int height)
{
    DisplaySurface *surface;

    ANEW0(surface);
    surface->width = width;
    surface->height = height;
    surface->linesize = width * 4;
    surface->pf = qemu_default_pixelformat(32);
#ifdef HOST_WORDS_BIGENDIAN
    surface->flags = QEMU_ALLOCATED_FLAG | QEMU_BIG_ENDIAN_FLAG;
#else
    surface->flags = QEMU_ALLOCATED_FLAG;
#endif
    surface->data = (uint8_t*) android_alloc0(surface->linesize * surface->height);

    return surface;
}

static DisplaySurface* defaultallocator_resize_displaysurface(DisplaySurface *surface,
                                          int width, int height)
{
    surface->width = width;
    surface->height = height;
    surface->linesize = width * 4;
    surface->pf = qemu_default_pixelformat(32);
    if (surface->flags & QEMU_ALLOCATED_FLAG)
        surface->data = (uint8_t*) android_realloc(surface->data, surface->linesize * surface->height);
    else
        surface->data = (uint8_t*) android_alloc(surface->linesize * surface->height);
#ifdef HOST_WORDS_BIGENDIAN
    surface->flags = QEMU_ALLOCATED_FLAG | QEMU_BIG_ENDIAN_FLAG;
#else
    surface->flags = QEMU_ALLOCATED_FLAG;
#endif

    return surface;
}

DisplaySurface* qemu_create_displaysurface_from(int width, int height, int bpp,
                                              int linesize, uint8_t *data)
{
    DisplaySurface *surface;

    ANEW0(surface);
    surface->width = width;
    surface->height = height;
    surface->linesize = linesize;
    surface->pf = qemu_default_pixelformat(bpp);
#ifdef HOST_WORDS_BIGENDIAN
    surface->flags = QEMU_BIG_ENDIAN_FLAG;
#endif
    surface->data = data;

    return surface;
}

static void defaultallocator_free_displaysurface(DisplaySurface *surface)
{
    if (surface == NULL)
        return;
    if (surface->flags & QEMU_ALLOCATED_FLAG)
        AFREE(surface->data);
    AFREE(surface);
}

static struct DisplayAllocator default_allocator = {
    defaultallocator_create_displaysurface,
    defaultallocator_resize_displaysurface,
    defaultallocator_free_displaysurface
};

static void dumb_display_init(void)
{
    DisplayState *ds;
    ANEW0(ds);
    ds->allocator = &default_allocator;
    ds->surface = qemu_create_displaysurface(ds, 640, 480);
    register_displaystate(ds);
}

/***********************************************************/
/* register display */

void register_displaystate(DisplayState *ds)
{
    DisplayState **s;
    s = &display_state;
    while (*s != NULL)
        s = &(*s)->next;
    ds->next = NULL;
    *s = ds;
}

DisplayState *get_displaystate(void)
{
    if (!display_state) {
        dumb_display_init ();
    }
    return display_state;
}

DisplayAllocator *register_displayallocator(DisplayState *ds, DisplayAllocator *da)
{
    if(ds->allocator ==  &default_allocator) {
        DisplaySurface *surf;
        surf = da->create_displaysurface(ds_get_width(ds), ds_get_height(ds));
        defaultallocator_free_displaysurface(ds->surface);
        ds->surface = surf;
        ds->allocator = da;
    }
    return ds->allocator;
}

DisplayState *graphic_console_init(vga_hw_update_ptr update,
                                   vga_hw_invalidate_ptr invalidate,
                                   vga_hw_screen_dump_ptr screen_dump,
                                   vga_hw_text_update_ptr text_update,
                                   void *opaque)
{
    TextConsole *s;
    DisplayState *ds;

    ANEW0(ds);
    ds->allocator = &default_allocator;
#ifdef CONFIG_ANDROID
    ds->surface = qemu_create_displaysurface(ds, android_display_width, android_display_height);
#else
    ds->surface = qemu_create_displaysurface(ds, 640, 480);
#endif

    s = new_console(ds, GRAPHIC_CONSOLE);
    if (s == NULL) {
        qemu_free_displaysurface(ds);
        AFREE(ds);
        return NULL;
    }
    s->hw_update = update;
    s->hw_invalidate = invalidate;
    s->hw_screen_dump = screen_dump;
    s->hw_text_update = text_update;
    s->hw = opaque;

    register_displaystate(ds);
    return ds;
}

int is_graphic_console(void)
{
    return active_console && active_console->console_type == GRAPHIC_CONSOLE;
}

int is_fixedsize_console(void)
{
    return active_console && active_console->console_type != TEXT_CONSOLE;
}

void console_color_init(DisplayState *ds)
{
    int i, j;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 8; i++) {
            color_table[j][i] = col_expand(ds,
                   vga_get_color(ds, color_table_rgb[j][i]));
        }
    }
}

void qemu_console_resize(DisplayState *ds, int width, int height)
{
    TextConsole *s = get_graphic_console(ds);
    if (!s) return;

    s->g_width = width;
    s->g_height = height;
    if (is_graphic_console()) {
        ds->surface = qemu_resize_displaysurface(ds, width, height);
        dpy_resize(ds);
    }
}

void qemu_console_copy(DisplayState *ds, int src_x, int src_y,
                       int dst_x, int dst_y, int w, int h)
{
    if (is_graphic_console()) {
        dpy_copy(ds, src_x, src_y, dst_x, dst_y, w, h);
    }
}

PixelFormat qemu_different_endianness_pixelformat(int bpp)
{
    PixelFormat pf;

    memset(&pf, 0x00, sizeof(PixelFormat));

    pf.bits_per_pixel = bpp;
    pf.bytes_per_pixel = bpp / 8;
    pf.depth = bpp == 32 ? 24 : bpp;

    switch (bpp) {
        case 24:
            pf.rmask = 0x000000FF;
            pf.gmask = 0x0000FF00;
            pf.bmask = 0x00FF0000;
            pf.rmax = 255;
            pf.gmax = 255;
            pf.bmax = 255;
            pf.rshift = 0;
            pf.gshift = 8;
            pf.bshift = 16;
            pf.rbits = 8;
            pf.gbits = 8;
            pf.bbits = 8;
            break;
        case 32:
            pf.rmask = 0x0000FF00;
            pf.gmask = 0x00FF0000;
            pf.bmask = 0xFF000000;
            pf.amask = 0x00000000;
            pf.amax = 255;
            pf.rmax = 255;
            pf.gmax = 255;
            pf.bmax = 255;
            pf.ashift = 0;
            pf.rshift = 8;
            pf.gshift = 16;
            pf.bshift = 24;
            pf.rbits = 8;
            pf.gbits = 8;
            pf.bbits = 8;
            pf.abits = 8;
            break;
        default:
            break;
    }
    return pf;
}

PixelFormat qemu_default_pixelformat(int bpp)
{
    PixelFormat pf;

    memset(&pf, 0x00, sizeof(PixelFormat));

    pf.bits_per_pixel = bpp;
    pf.bytes_per_pixel = bpp / 8;
    pf.depth = bpp == 32 ? 24 : bpp;

    switch (bpp) {
        case 15:
            pf.bits_per_pixel = 16;
            pf.bytes_per_pixel = 2;
            pf.rmask = 0x00007c00;
            pf.gmask = 0x000003E0;
            pf.bmask = 0x0000001F;
            pf.rmax = 31;
            pf.gmax = 31;
            pf.bmax = 31;
            pf.rshift = 10;
            pf.gshift = 5;
            pf.bshift = 0;
            pf.rbits = 5;
            pf.gbits = 5;
            pf.bbits = 5;
            break;
        case 16:
            pf.rmask = 0x0000F800;
            pf.gmask = 0x000007E0;
            pf.bmask = 0x0000001F;
            pf.rmax = 31;
            pf.gmax = 63;
            pf.bmax = 31;
            pf.rshift = 11;
            pf.gshift = 5;
            pf.bshift = 0;
            pf.rbits = 5;
            pf.gbits = 6;
            pf.bbits = 5;
            break;
        case 24:
            pf.rmask = 0x00FF0000;
            pf.gmask = 0x0000FF00;
            pf.bmask = 0x000000FF;
            pf.rmax = 255;
            pf.gmax = 255;
            pf.bmax = 255;
            pf.rshift = 16;
            pf.gshift = 8;
            pf.bshift = 0;
            pf.rbits = 8;
            pf.gbits = 8;
            pf.bbits = 8;
        case 32:
            pf.rmask = 0x00FF0000;
            pf.gmask = 0x0000FF00;
            pf.bmask = 0x000000FF;
            pf.amax = 255;
            pf.rmax = 255;
            pf.gmax = 255;
            pf.bmax = 255;
            pf.ashift = 24;
            pf.rshift = 16;
            pf.gshift = 8;
            pf.bshift = 0;
            pf.rbits = 8;
            pf.gbits = 8;
            pf.bbits = 8;
            pf.abits = 8;
            break;
        default:
            break;
    }
    return pf;
}

#ifdef CONFIG_ANDROID
void
android_display_init_from(int width, int height, int rotation, int bpp)
{
    DisplayState *ds;
    ANEW0(ds);
    ds->allocator = &default_allocator;
    ds->surface = qemu_create_displaysurface(ds, width, height);
    register_displaystate(ds);
}
#endif
