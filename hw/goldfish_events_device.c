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
#include "vl.h"
#include "android_events.h"
#include "irq.h"

#if 0
// From kernel...
#define EV_SYN			0x00
#define EV_KEY			0x01
#define EV_REL			0x02
#define EV_ABS			0x03
#define EV_MSC			0x04
#define EV_SW			0x05
#define EV_LED			0x11
#define EV_SND			0x12
#define EV_REP			0x14
#define EV_FF			0x15
#define EV_PWR			0x16
#define EV_FF_STATUS		0x17
#define EV_MAX			0x1f

#define BTN_MISC		0x100
#define BTN_0			0x100
#define BTN_1			0x101
#define BTN_2			0x102
#define BTN_3			0x103
#define BTN_4			0x104
#define BTN_5			0x105
#define BTN_6			0x106
#define BTN_7			0x107
#define BTN_8			0x108
#define BTN_9			0x109

#define BTN_MOUSE		0x110
#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_SIDE		0x113
#define BTN_EXTRA		0x114
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116
#define BTN_TASK		0x117

#define BTN_JOYSTICK		0x120
#define BTN_TRIGGER		0x120
#define BTN_THUMB		0x121
#define BTN_THUMB2		0x122
#define BTN_TOP			0x123
#define BTN_TOP2		0x124
#define BTN_PINKIE		0x125
#define BTN_BASE		0x126
#define BTN_BASE2		0x127
#define BTN_BASE3		0x128
#define BTN_BASE4		0x129
#define BTN_BASE5		0x12a
#define BTN_BASE6		0x12b
#define BTN_DEAD		0x12f

#define BTN_GAMEPAD		0x130
#define BTN_A			0x130
#define BTN_B			0x131
#define BTN_C			0x132
#define BTN_X			0x133
#define BTN_Y			0x134
#define BTN_Z			0x135
#define BTN_TL			0x136
#define BTN_TR			0x137
#define BTN_TL2			0x138
#define BTN_TR2			0x139
#define BTN_SELECT		0x13a
#define BTN_START		0x13b
#define BTN_MODE		0x13c
#define BTN_THUMBL		0x13d
#define BTN_THUMBR		0x13e

#define BTN_DIGI		0x140
#define BTN_TOOL_PEN		0x140
#define BTN_TOOL_RUBBER		0x141
#define BTN_TOOL_BRUSH		0x142
#define BTN_TOOL_PENCIL		0x143
#define BTN_TOOL_AIRBRUSH	0x144
#define BTN_TOOL_FINGER		0x145
#define BTN_TOOL_MOUSE		0x146
#define BTN_TOOL_LENS		0x147
#define BTN_TOUCH		0x14a
#define BTN_STYLUS		0x14b
#define BTN_STYLUS2		0x14c
#define BTN_TOOL_DOUBLETAP	0x14d
#define BTN_TOOL_TRIPLETAP	0x14e

#define BTN_WHEEL		0x150
#define BTN_GEAR_DOWN		0x150
#define BTN_GEAR_UP		0x151

#define REL_X           0x00
#define REL_Y           0x01

#define ABS_X			0x00
#define ABS_Y			0x01
#define ABS_Z			0x02
#define ABS_RX			0x03
#define ABS_RY			0x04
#define ABS_RZ			0x05
#define ABS_THROTTLE		0x06
#define ABS_RUDDER		0x07
#define ABS_WHEEL		0x08
#define ABS_GAS			0x09
#define ABS_BRAKE		0x0a
#define ABS_HAT0X		0x10
#define ABS_HAT0Y		0x11
#define ABS_HAT1X		0x12
#define ABS_HAT1Y		0x13
#define ABS_HAT2X		0x14
#define ABS_HAT2Y		0x15
#define ABS_HAT3X		0x16
#define ABS_HAT3Y		0x17
#define ABS_PRESSURE		0x18
#define ABS_DISTANCE		0x19
#define ABS_TILT_X		0x1a
#define ABS_TILT_Y		0x1b
#define ABS_TOOL_WIDTH		0x1c
#define ABS_VOLUME		0x20
#define ABS_MISC		0x28
#define ABS_MAX			0x3f
#endif

#define MAX_EVENTS 256*4

enum {
    REG_READ        = 0x00,
    REG_SET_PAGE    = 0x00,
    REG_LEN         = 0x04,
    REG_DATA        = 0x08,

    PAGE_NAME       = 0x00000,
    PAGE_EVBITS     = 0x10000,
    PAGE_ABSDATA    = 0x20000 | EV_ABS,
};

typedef struct
{
    uint32_t base;
    qemu_irq  irq;
    int pending;
    int page;

    unsigned events[MAX_EVENTS];
    unsigned first;
    unsigned last;

    const char *name;
    struct {
        size_t len;
        uint8_t *bits;
    } ev_bits[EV_MAX + 1];
    int32_t *abs_info;
    size_t abs_info_count;
} events_state;

/* modify this each time you change the events_device structure. you
 * will also need to upadte events_state_load and events_state_save
 */
#define  EVENTS_STATE_SAVE_VERSION  1

#undef  QFIELD_STRUCT
#define QFIELD_STRUCT  events_state

QFIELD_BEGIN(events_state_fields)
    QFIELD_INT32(pending),
    QFIELD_INT32(page),
    QFIELD_BUFFER(events),
    QFIELD_INT32(first),
    QFIELD_INT32(last),
QFIELD_END

static void  events_state_save(QEMUFile*  f, void*  opaque)
{
    events_state*  s = opaque;

    qemu_put_struct(f, events_state_fields, s);
}

static int  events_state_load(QEMUFile*  f, void* opaque, int  version_id)
{
    events_state*  s = opaque;

    if (version_id != EVENTS_STATE_SAVE_VERSION)
        return -1;

    return qemu_get_struct(f, events_state_fields, s);
}

extern const char*  android_skin_keycharmap;

static void enqueue_event(events_state *s, unsigned int type, unsigned int code, int value)
{
    int  enqueued = s->last - s->first;

    if (enqueued < 0)
        enqueued += MAX_EVENTS;

    if (enqueued + 3 >= MAX_EVENTS-1) {
        fprintf(stderr, "##KBD: Full queue, lose event\n");
        return;
    }

    if(s->first == s->last){
        qemu_irq_raise(s->irq);
    }

    //fprintf(stderr, "##KBD: type=%d code=%d value=%d\n", type, code, value);

    s->events[s->last] = type;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
    s->events[s->last] = code;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
    s->events[s->last] = value;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
}

static unsigned dequeue_event(events_state *s)
{
    unsigned n;

    if(s->first == s->last) {
        return 0;
    }

    n = s->events[s->first];

    s->first = (s->first + 1) & (MAX_EVENTS - 1);

    if(s->first == s->last) {
        qemu_irq_lower(s->irq);
    }

    return n;
}

static int get_page_len(events_state *s)
{
    int page = s->page;
    if (page == PAGE_NAME)
        return strlen(s->name);
    if (page >= PAGE_EVBITS && page <= PAGE_EVBITS + EV_MAX)
        return s->ev_bits[page - PAGE_EVBITS].len;
    if (page == PAGE_ABSDATA)
        return s->abs_info_count * sizeof(s->abs_info[0]);
    return 0;
}

static int get_page_data(events_state *s, int offset)
{
    int page_len = get_page_len(s);
    int page = s->page;
    if (offset > page_len)
        return 0;
    if (page == PAGE_NAME)
        return s->name[offset];
    if (page >= PAGE_EVBITS && page <= PAGE_EVBITS + EV_MAX)
        return s->ev_bits[page - PAGE_EVBITS].bits[offset];
    if (page == PAGE_ABSDATA)
        return s->abs_info[offset / sizeof(s->abs_info[0])];
    return 0;
}

static uint32_t events_read(void *x, target_phys_addr_t off)
{
    events_state *s = (events_state *) x;
    int offset = off - s->base;
    if (offset == REG_READ)
        return dequeue_event(s);
    else if (offset == REG_LEN)
        return get_page_len(s);
    else if (offset >= REG_DATA)
        return get_page_data(s, offset - REG_DATA);
    return 0; // this shouldn't happen, if the driver does the right thing
}

static void events_write(void *x, target_phys_addr_t off, uint32_t val)
{
    events_state *s = (events_state *) x;
    int offset = off - s->base;
    if (offset == REG_SET_PAGE)
        s->page = val;
}

static CPUReadMemoryFunc *events_readfn[] = {
   events_read,
   events_read,
   events_read
};

static CPUWriteMemoryFunc *events_writefn[] = {
   events_write,
   events_write,
   events_write
};

static void events_put_keycode(void *x, int keycode)
{
    events_state *s = (events_state *) x;

    enqueue_event(s, EV_KEY, keycode&0x1ff, (keycode&0x200) ? 1 : 0);
}

static void events_put_mouse(void *opaque, int dx, int dy, int dz, int buttons_state)
{
    events_state *s = (events_state *) opaque;
    if (dz == 0) {
        enqueue_event(s, EV_ABS, ABS_X, dx);
        enqueue_event(s, EV_ABS, ABS_Y, dy);
        enqueue_event(s, EV_ABS, ABS_Z, dz);
        enqueue_event(s, EV_KEY, BTN_TOUCH, buttons_state&1);
    } else {
        enqueue_event(s, EV_REL, REL_X, dx);
        enqueue_event(s, EV_REL, REL_Y, dy);
    }
    enqueue_event(s, EV_SYN, 0, 0);
}

static void  events_put_generic(void*  opaque, int  type, int  code, int  value)
{
   events_state *s = (events_state *) opaque;

    enqueue_event(s, type, code, value);
}

static int events_set_bits(events_state *s, int type, int bitl, int bith)
{
    uint8_t *bits;
    uint8_t maskl, maskh;
    int il, ih;
    il = bitl / 8;
    ih = bith / 8;
    if (ih >= s->ev_bits[type].len) {
        bits = qemu_mallocz(ih + 1);
        if (bits == NULL)
            return -ENOMEM;
        memcpy(bits, s->ev_bits[type].bits, s->ev_bits[type].len);
	qemu_free(s->ev_bits[type].bits);
        s->ev_bits[type].bits = bits;
        s->ev_bits[type].len = ih + 1;
    }
    else
        bits = s->ev_bits[type].bits;
    maskl = 0xffU << (bitl & 7);
    maskh = 0xffU >> (7 - (bith & 7));
    if (il >= ih)
        maskh &= maskl;
    else {
        bits[il] |= maskl;
        while (++il < ih)
            bits[il] = 0xff;
    }
    bits[ih] |= maskh;
    return 0;
}

#if 0
static int events_set_abs_info(events_state *s, int axis, int32_t min, int32_t max, int32_t fuzz, int32_t flat)
{
    int32_t *info;
    if (axis * 4 >= s->abs_info_count) {
        info = qemu_mallocz((axis + 1) * 4 * sizeof(int32_t));
        if (info == NULL)
            return -ENOMEM;
        memcpy(info, s->abs_info, s->abs_info_count);
	qemu_free(s->abs_info);
        s->abs_info = info;
        s->abs_info_count = (axis + 1) * 4;
    }
    else
        info = s->abs_info;
    info += axis * 4;
    *info++ = min;
    *info++ = max;
    *info++ = fuzz;
    *info++ = flat;
}
#endif

void events_dev_init(uint32_t base, qemu_irq irq)
{
    events_state *s;
    int iomemtype;

    s = (events_state *) qemu_mallocz(sizeof(events_state));
    s->name = android_skin_keycharmap;
    events_set_bits(s, EV_SYN, EV_SYN, EV_ABS);
    events_set_bits(s, EV_SYN, EV_SW, EV_SW);
    events_set_bits(s, EV_KEY, 1, 0x1ff);
    events_set_bits(s, EV_REL, REL_X, REL_Y);
    events_set_bits(s, EV_ABS, ABS_X, ABS_Z);
    events_set_bits(s, EV_SW, 0, 0);
    iomemtype = cpu_register_io_memory(0, events_readfn, events_writefn, s);

    cpu_register_physical_memory(base, 0xfff, iomemtype);

    qemu_add_kbd_event_handler(events_put_keycode, s);
    qemu_add_mouse_event_handler(events_put_mouse, s, 1);
    qemu_add_generic_event_handler(events_put_generic, s);

    s->base = base;
    s->irq = irq;

    s->first = 0;
    s->last = 0;

    register_savevm( "events_state", 0, EVENTS_STATE_SAVE_VERSION,
                      events_state_save, events_state_load, s );
}

