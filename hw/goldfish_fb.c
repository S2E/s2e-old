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
#include "qemu_file.h"
#include "android/android.h"
#include "goldfish_device.h"
#include "framebuffer.h"

enum {
    FB_GET_WIDTH        = 0x00,
    FB_GET_HEIGHT       = 0x04,
    FB_INT_STATUS       = 0x08,
    FB_INT_ENABLE       = 0x0c,
    FB_SET_BASE         = 0x10,
    FB_SET_ROTATION     = 0x14,
    FB_SET_BLANK        = 0x18,
    FB_GET_PHYS_WIDTH   = 0x1c,
    FB_GET_PHYS_HEIGHT  = 0x20,

    FB_INT_VSYNC             = 1U << 0,
    FB_INT_BASE_UPDATE_DONE  = 1U << 1
};

struct goldfish_fb_state {
    struct goldfish_device dev;
    QFrameBuffer*  qfbuff;
    uint32_t fb_base;
    uint32_t base_valid : 1;
    uint32_t need_update : 1;
    uint32_t need_int : 1;
    uint32_t set_rotation : 2;
    uint32_t blank : 1;
    uint32_t int_status;
    uint32_t int_enable;
    int      rotation;   /* 0, 1, 2 or 3 */
};

#define  GOLDFISH_FB_SAVE_VERSION  1

static void goldfish_fb_save(QEMUFile*  f, void*  opaque)
{
    struct goldfish_fb_state*  s = opaque;

    QFrameBuffer*  q = s->qfbuff;

    qemu_put_be32(f, q->width);
    qemu_put_be32(f, q->height);
    qemu_put_be32(f, q->pitch);
    qemu_put_byte(f, q->rotation);

    qemu_put_be32(f, s->fb_base);
    qemu_put_byte(f, s->base_valid);
    qemu_put_byte(f, s->need_update);
    qemu_put_byte(f, s->need_int);
    qemu_put_byte(f, s->set_rotation);
    qemu_put_byte(f, s->blank);
    qemu_put_be32(f, s->int_status);
    qemu_put_be32(f, s->int_enable);
    qemu_put_be32(f, s->rotation);
}

static int  goldfish_fb_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct goldfish_fb_state*  s   = opaque;

    QFrameBuffer*              q   = s->qfbuff;
    int                        ret = -1;
    int                        ds_w, ds_h, ds_pitch, ds_rot;

    if (version_id != GOLDFISH_FB_SAVE_VERSION)
        goto Exit;

    ds_w     = qemu_get_be32(f);
    ds_h     = qemu_get_be32(f);
    ds_pitch = qemu_get_be32(f);
    ds_rot   = qemu_get_byte(f);

    if (q->width != ds_w      ||
        q->height != ds_h     ||
        q->pitch != ds_pitch  ||
        q->rotation != ds_rot )
    {
        /* XXX: We should be able to force a resize/rotation from here ? */
        fprintf(stderr, "%s: framebuffer dimensions mismatch\n", __FUNCTION__);
        goto Exit;
    }

    s->fb_base      = qemu_get_be32(f);
    s->base_valid   = qemu_get_byte(f);
    s->need_update  = qemu_get_byte(f);
    s->need_int     = qemu_get_byte(f);
    s->set_rotation = qemu_get_byte(f);
    s->blank        = qemu_get_byte(f);
    s->int_status   = qemu_get_be32(f);
    s->int_enable   = qemu_get_be32(f);
    s->rotation     = qemu_get_be32(f);

    /* force a refresh */
    s->need_update = 1;

    ret = 0;
Exit:
    return ret;
}


#define  STATS  0

#if STATS
static int   stats_counter;
static long  stats_total;
static int   stats_full_updates;
static long  stats_total_full_updates;
#endif

static void goldfish_fb_update_display(void *opaque)
{
    struct goldfish_fb_state *s = (struct goldfish_fb_state *)opaque;
    uint32_t addr;
    uint32_t base;

    uint8_t*  dst_line;
    uint8_t*  src_line;
    int y_first, y_last = 0;
    int full_update = 0;
    int    width, height, pitch;

    base = s->fb_base;
    if(base == 0)
        return;

    if((s->int_enable & FB_INT_VSYNC) && !(s->int_status & FB_INT_VSYNC)) {
        s->int_status |= FB_INT_VSYNC;
        goldfish_device_set_irq(&s->dev, 0, 1);
    }

    y_first = -1;
    addr  = base;
    if(s->need_update) {
        full_update = 1;
        if(s->need_int) {
            s->int_status |= FB_INT_BASE_UPDATE_DONE;
            if(s->int_enable & FB_INT_BASE_UPDATE_DONE)
                goldfish_device_set_irq(&s->dev, 0, 1);
        }
        s->need_int = 0;
        s->need_update = 0;
    }

    src_line  = qemu_get_ram_ptr( base );
    dst_line  = s->qfbuff->pixels;
    pitch     = s->qfbuff->pitch;
    width     = s->qfbuff->width;
    height    = s->qfbuff->height;

#if STATS
    if (full_update)
        stats_full_updates += 1;
    if (++stats_counter == 120) {
        stats_total               += stats_counter;
        stats_total_full_updates  += stats_full_updates;

        printf( "full update stats:  peak %.2f %%  total %.2f %%\n",
                stats_full_updates*100.0/stats_counter,
                stats_total_full_updates*100.0/stats_total );

        stats_counter      = 0;
        stats_full_updates = 0;
    }
#endif /* STATS */

    if (s->blank)
    {
        memset( dst_line, 0, height*pitch );
        y_first = 0;
        y_last  = height-1;
    }
    else if (full_update)
    {
        int  yy;

        for (yy = 0; yy < height; yy++, dst_line += pitch, src_line += width*2)
        {
            uint16_t*  src = (uint16_t*) src_line;
            uint16_t*  dst = (uint16_t*) dst_line;
            int        nn;

            for (nn = 0; nn < width; nn++) {
                unsigned   spix = src[nn];
                unsigned   dpix = dst[nn];
#if WORDS_BIGENDIAN
                spix = ((spix << 8) | (spix >> 8)) & 0xffff;
#else
                if (spix != dpix)
                    break;
#endif
            }

            if (nn == width)
                continue;

#if WORDS_BIGENDIAN
            for ( ; nn < width; nn++ ) {
                unsigned   spix = src[nn];
                dst[nn] = (uint16_t)((spix << 8) | (spix >> 8));
            }
#else
            memcpy( dst+nn, src+nn, (width-nn)*2 );
#endif

            y_first = (y_first < 0) ? yy : y_first;
            y_last  = yy;
        }
    }
    else  /* not a full update, should not happen very often with Android */
    {
        int  yy;

        for (yy = 0; yy < height; yy++, dst_line += pitch, src_line += width*2)
        {
            uint16_t*  src   = (uint16_t*) src_line;
            uint16_t*  dst   = (uint16_t*) dst_line;
            int        len   = width*2;
#if WORDS_BIGENDIAN
            int        nn;
#endif
            int        dirty = 0;

            while (len > 0) {
                int  len2 = TARGET_PAGE_SIZE - (addr & (TARGET_PAGE_SIZE-1));

                if (len2 > len)
                    len2 = len;

                dirty |= cpu_physical_memory_get_dirty(addr, VGA_DIRTY_FLAG);
                addr  += len2;
                len   -= len2;
            }

            if (!dirty)
                continue;

#if WORDS_BIGENDIAN
            for (nn = 0; nn < width; nn++ ) {
                unsigned   spix = src[nn];
                dst[nn] = (uint16_t)((spix << 8) | (spix >> 8));
            }
#else
            memcpy( dst, src, width*2 );
#endif

            y_first = (y_first < 0) ? yy : y_first;
            y_last  = yy;
        }
    }

    if (y_first < 0)
      return;

    y_last += 1;
    //printf("goldfish_fb_update_display %d %d, base %x\n", first, last, base);

    cpu_physical_memory_reset_dirty(base + y_first * width * 2,
                                    base + y_last * width * 2,
                                    VGA_DIRTY_FLAG);

    qframebuffer_update( s->qfbuff, 0, y_first, width, y_last-y_first );
}

static void goldfish_fb_invalidate_display(void * opaque)
{
    // is this called?
    struct goldfish_fb_state *s = (struct goldfish_fb_state *)opaque;
    s->need_update = 1;
}

static void  goldfish_fb_detach_display(void*  opaque)
{
    struct goldfish_fb_state *s = (struct goldfish_fb_state *)opaque;
    s->qfbuff = NULL;
}

static uint32_t goldfish_fb_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t ret;
    struct goldfish_fb_state *s = opaque;

    switch(offset) {
        case FB_GET_WIDTH:
            ret = s->qfbuff->width;
            //printf("FB_GET_WIDTH => %d\n", ret);
            return ret;

        case FB_GET_HEIGHT:
            ret = s->qfbuff->height;
            //printf( "FB_GET_HEIGHT = %d\n", ret);
            return ret;

        case FB_INT_STATUS:
            ret = s->int_status & s->int_enable;
            if(ret) {
                s->int_status &= ~ret;
                goldfish_device_set_irq(&s->dev, 0, 0);
            }
            return ret;

        case FB_GET_PHYS_WIDTH:
            ret = s->qfbuff->phys_width_mm;
            //printf( "FB_GET_PHYS_WIDTH => %d\n", ret );
            return ret;

        case FB_GET_PHYS_HEIGHT:
            ret = s->qfbuff->phys_height_mm;
            //printf( "FB_GET_PHYS_HEIGHT => %d\n", ret );
            return ret;

        default:
            cpu_abort (cpu_single_env, "goldfish_fb_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_fb_write(void *opaque, target_phys_addr_t offset,
                        uint32_t val)
{
    struct goldfish_fb_state *s = opaque;

    switch(offset) {
        case FB_INT_ENABLE:
            s->int_enable = val;
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            break;
        case FB_SET_BASE: {
            int need_resize = !s->base_valid;
            s->fb_base = val;
            s->int_status &= ~FB_INT_BASE_UPDATE_DONE;
            s->need_update = 1;
            s->need_int = 1;
            s->base_valid = 1;
            if(s->set_rotation != s->rotation) {
                //printf("FB_SET_BASE: rotation : %d => %d\n", s->rotation, s->set_rotation);
                s->rotation = s->set_rotation;
                need_resize = 1;
            }
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            if (need_resize) {
                //printf("FB_SET_BASE: need resize (rotation=%d)\n", s->rotation );
                qframebuffer_rotate( s->qfbuff, s->rotation );
            }
            } break;
        case FB_SET_ROTATION:
            //printf( "FB_SET_ROTATION %d\n", val);
            s->set_rotation = val;
            break;
        case FB_SET_BLANK:
            s->blank = val;
            s->need_update = 1;
            break;
        default:
            cpu_abort (cpu_single_env, "goldfish_fb_write: Bad offset %x\n", offset);
    }
}

static CPUReadMemoryFunc *goldfish_fb_readfn[] = {
   goldfish_fb_read,
   goldfish_fb_read,
   goldfish_fb_read
};

static CPUWriteMemoryFunc *goldfish_fb_writefn[] = {
   goldfish_fb_write,
   goldfish_fb_write,
   goldfish_fb_write
};

void goldfish_fb_init(DisplayState *ds, int id)
{
    struct goldfish_fb_state *s;

    s = (struct goldfish_fb_state *)qemu_mallocz(sizeof(*s));
    s->dev.name = "goldfish_fb";
    s->dev.id = id;
    s->dev.size = 0x1000;
    s->dev.irq_count = 1;

    s->qfbuff = qframebuffer_fifo_get();
    qframebuffer_set_producer( s->qfbuff, s,
                               goldfish_fb_update_display,
                               goldfish_fb_invalidate_display,
                               goldfish_fb_detach_display );

    goldfish_device_add(&s->dev, goldfish_fb_readfn, goldfish_fb_writefn, s);

    register_savevm( "goldfish_fb", 0, GOLDFISH_FB_SAVE_VERSION,
                     goldfish_fb_save, goldfish_fb_load, s);
}

