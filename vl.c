/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2005 Fabrice Bellard
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
#include "vl.h"

#include "android.h"
#include "charpipe.h"
#include "shaper.h"
#include "modem_driver.h"
#include "android_gps.h"
#include "android_qemud.h"
#include "android_kmsg.h"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#ifndef _WIN32
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <netdb.h>
#ifdef _BSD
#include <sys/stat.h>
#ifndef __APPLE__
#include <libutil.h>
#endif
#else
#ifndef __sun__
#include <linux/if.h>
#include <linux/if_tun.h>
#include <pty.h>
#include <malloc.h>
#include <linux/ppdev.h>
#endif
#endif
#endif

#if defined(CONFIG_SLIRP)
#include "libslirp.h"
const char *bootp_filename;
#endif

#ifdef _WIN32
#include <malloc.h>
#include <sys/timeb.h>
#include <windows.h>
#define getopt_long_only getopt_long
#define memalign(align, size) malloc(size)
#endif

#include "sockets.h"

extern void  android_emulation_setup( void );
extern void  android_emulation_teardown( void );

#ifdef CONFIG_COCOA
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

#ifdef CONFIG_SKINS
#undef main
#define main qemu_main
#endif

#include "disas.h"

#include "exec-all.h"

#ifdef CONFIG_TRACE
#include "trace.h"
#include "dcache.h"
#endif

#ifdef CONFIG_NAND
#include "hw/goldfish_nand.h"
#endif

#define DEFAULT_NETWORK_SCRIPT "/etc/qemu-ifup"

//#define DEBUG_UNUSED_IOPORT
//#define DEBUG_IOPORT

#define PHYS_RAM_MAX_SIZE (2047 * 1024 * 1024)

#ifdef TARGET_PPC
#define DEFAULT_RAM_SIZE 144
#else
#if 1  /* Android */
#define DEFAULT_RAM_SIZE 96
#else
#define DEFAULT_RAM_SIZE 128
#endif
#endif
/* in ms */
#define GUI_REFRESH_INTERVAL (1000/60)

/* Max number of USB devices that can be specified on the commandline.  */
#define MAX_USB_CMDLINE 8

/* XXX: use a two level table to limit memory usage */
#define MAX_IOPORTS 65536

const char *bios_dir = CONFIG_QEMU_SHAREDIR;
char phys_ram_file[1024];
void *ioport_opaque[MAX_IOPORTS];
IOPortReadFunc *ioport_read_table[3][MAX_IOPORTS];
IOPortWriteFunc *ioport_write_table[3][MAX_IOPORTS];
BlockDriverState *bs_table[MAX_DISKS], *fd_table[MAX_FD];
int vga_ram_size;
int bios_size;
static DisplayState display_state;
int nographic;
const char* keyboard_layout = NULL;
int64_t ticks_per_sec;
int boot_device = 'c';
int ram_size;
int pit_min_timer_count = 0;
int nb_nics;
NICInfo nd_table[MAX_NICS];
QEMUTimer *gui_timer;
int vm_running;
int rtc_utc = 1;
#ifdef TARGET_SPARC
int graphic_width = 1024;
int graphic_height = 768;
#else
int graphic_width = 800;
int graphic_height = 600;
#endif
int graphic_depth = 15;
int full_screen = 0;
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
#ifdef TARGET_I386
int win2k_install_hack = 0;
#endif
int usb_enabled = 0;
static VLANState *first_vlan;
int smp_cpus = 1;
int vnc_display = -1;
#if defined(TARGET_SPARC)
#define MAX_CPUS 16
#elif defined(TARGET_I386)
#define MAX_CPUS 255
#else
#define MAX_CPUS 1
#endif
int acpi_enabled = 1;
int fd_bootchk = 1;

static CPUState *cur_cpu;
static int event_pending = 1;

#ifdef CONFIG_TRACE
const char *trace_filename;
int tracing;
int trace_cache_miss;
int trace_all_addr;

int dcache_size = 16 * 1024;
int dcache_ways = 4;
int dcache_line_size = 32;
int dcache_replace_policy = kPolicyRandom;
int dcache_load_miss_penalty = 30;
int dcache_store_miss_penalty = 5;
#endif

extern int   qemu_cpu_delay;

extern void  dprint( const char* format, ... );

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)

/***********************************************************/
/* x86 ISA bus support */

target_phys_addr_t isa_mem_base = 0;
PicState2 *isa_pic;

uint32_t default_ioport_readb(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "inb: port=0x%04x\n", address);
#endif
    return 0xff;
}

void default_ioport_writeb(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "outb: port=0x%04x data=0x%02x\n", address, data);
#endif
}

/* default is to make two byte accesses */
uint32_t default_ioport_readw(void *opaque, uint32_t address)
{
    uint32_t data;
    data = ioport_read_table[0][address](ioport_opaque[address], address);
    address = (address + 1) & (MAX_IOPORTS - 1);
    data |= ioport_read_table[0][address](ioport_opaque[address], address) << 8;
    return data;
}

void default_ioport_writew(void *opaque, uint32_t address, uint32_t data)
{
    ioport_write_table[0][address](ioport_opaque[address], address, data & 0xff);
    address = (address + 1) & (MAX_IOPORTS - 1);
    ioport_write_table[0][address](ioport_opaque[address], address, (data >> 8) & 0xff);
}

uint32_t default_ioport_readl(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "inl: port=0x%04x\n", address);
#endif
    return 0xffffffff;
}

void default_ioport_writel(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "outl: port=0x%04x data=0x%02x\n", address, data);
#endif
}

void init_ioports(void)
{
    int i;

    for(i = 0; i < MAX_IOPORTS; i++) {
        ioport_read_table[0][i] = default_ioport_readb;
        ioport_write_table[0][i] = default_ioport_writeb;
        ioport_read_table[1][i] = default_ioport_readw;
        ioport_write_table[1][i] = default_ioport_writew;
        ioport_read_table[2][i] = default_ioport_readl;
        ioport_write_table[2][i] = default_ioport_writel;
    }
}

/* size is the word size in byte */
int register_ioport_read(int start, int length, int size,
                         IOPortReadFunc *func, void *opaque)
{
    int i, bsize;

    if (size == 1) {
        bsize = 0;
    } else if (size == 2) {
        bsize = 1;
    } else if (size == 4) {
        bsize = 2;
    } else {
        hw_error("register_ioport_read: invalid size");
        return -1;
    }
    for(i = start; i < start + length; i += size) {
        ioport_read_table[bsize][i] = func;
        if (ioport_opaque[i] != NULL && ioport_opaque[i] != opaque)
            hw_error("register_ioport_read: invalid opaque");
        ioport_opaque[i] = opaque;
    }
    return 0;
}

/* size is the word size in byte */
int register_ioport_write(int start, int length, int size,
                          IOPortWriteFunc *func, void *opaque)
{
    int i, bsize;

    if (size == 1) {
        bsize = 0;
    } else if (size == 2) {
        bsize = 1;
    } else if (size == 4) {
        bsize = 2;
    } else {
        hw_error("register_ioport_write: invalid size");
        return -1;
    }
    for(i = start; i < start + length; i += size) {
        ioport_write_table[bsize][i] = func;
        if (ioport_opaque[i] != NULL && ioport_opaque[i] != opaque)
            hw_error("register_ioport_read: invalid opaque");
        ioport_opaque[i] = opaque;
    }
    return 0;
}

void isa_unassign_ioport(int start, int length)
{
    int i;

    for(i = start; i < start + length; i++) {
        ioport_read_table[0][i] = default_ioport_readb;
        ioport_read_table[1][i] = default_ioport_readw;
        ioport_read_table[2][i] = default_ioport_readl;

        ioport_write_table[0][i] = default_ioport_writeb;
        ioport_write_table[1][i] = default_ioport_writew;
        ioport_write_table[2][i] = default_ioport_writel;
    }
}

/***********************************************************/

void pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
        return;

    for(;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1)
            break;
        *q++ = c;
    }
    *q = '\0';
}

/* strcat and truncate. */
char *pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

void cpu_outb(CPUState *env, int addr, int val)
{
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "outb: %04x %02x\n", addr, val);
#endif
    ioport_write_table[0][addr](ioport_opaque[addr], addr, val);
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
}

void cpu_outw(CPUState *env, int addr, int val)
{
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "outw: %04x %04x\n", addr, val);
#endif
    ioport_write_table[1][addr](ioport_opaque[addr], addr, val);
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
}

void cpu_outl(CPUState *env, int addr, int val)
{
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "outl: %04x %08x\n", addr, val);
#endif
    ioport_write_table[2][addr](ioport_opaque[addr], addr, val);
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
}

int cpu_inb(CPUState *env, int addr)
{
    int val;
    val = ioport_read_table[0][addr](ioport_opaque[addr], addr);
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "inb : %04x %02x\n", addr, val);
#endif
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
    return val;
}

int cpu_inw(CPUState *env, int addr)
{
    int val;
    val = ioport_read_table[1][addr](ioport_opaque[addr], addr);
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "inw : %04x %04x\n", addr, val);
#endif
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
    return val;
}

int cpu_inl(CPUState *env, int addr)
{
    int val;
    val = ioport_read_table[2][addr](ioport_opaque[addr], addr);
#ifdef DEBUG_IOPORT
    if (loglevel & CPU_LOG_IOPORT)
        fprintf(logfile, "inl : %04x %08x\n", addr, val);
#endif
#ifdef USE_KQEMU
    if (env)
        env->last_io_time = cpu_get_time_fast();
#endif
    return val;
}

/***********************************************************/
void hw_error(const char *fmt, ...)
{
    va_list ap;
    CPUState *env;

    va_start(ap, fmt);
    fprintf(stderr, "qemu: hardware error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        fprintf(stderr, "CPU #%d:\n", env->cpu_index);
#ifdef TARGET_I386
        cpu_dump_state(env, stderr, fprintf, X86_DUMP_FPU);
#else
        cpu_dump_state(env, stderr, fprintf, 0);
#endif
    }
    va_end(ap);
    abort();
}

/***********************************************************/
/* keyboard/mouse */

static QEMUPutKBDEvent*  qemu_put_kbd_event;
static void*             qemu_put_kbd_event_opaque;

static QEMUPutKBDEventN*  qemu_put_kbd_event_n;
static void*              qemu_put_kbd_event_n_opaque;


static QEMUPutMouseEvent*  qemu_put_mouse_event;
static void*               qemu_put_mouse_event_opaque;

static QEMUPutGenericEvent*  qemu_put_generic_event;
static void*                 qemu_put_generic_event_opaque;

static int qemu_put_mouse_event_absolute;

void qemu_add_kbd_event_handler(QEMUPutKBDEvent *func, void *opaque)
{
    qemu_put_kbd_event_opaque = opaque;
    qemu_put_kbd_event = func;
}

void qemu_add_kbd_event_n_handler(QEMUPutKBDEventN *func, void *opaque)
{
    qemu_put_kbd_event_n_opaque = opaque;
    qemu_put_kbd_event_n = func;
}

void qemu_add_mouse_event_handler(QEMUPutMouseEvent *func, void *opaque, int absolute)
{
    qemu_put_mouse_event_opaque = opaque;
    qemu_put_mouse_event = func;
    qemu_put_mouse_event_absolute = absolute;
}

void qemu_add_generic_event_handler(QEMUPutGenericEvent *func, void*  opaque)
{
    qemu_put_generic_event = func;
    qemu_put_generic_event_opaque = opaque;
}

void kbd_put_keycode(int keycode)
{
    if (qemu_put_kbd_event) {
        qemu_put_kbd_event(qemu_put_kbd_event_opaque, keycode);
    }
}

void kbd_put_keycodes(int*  keycodes, int  count)
{
    if (qemu_put_kbd_event_n)
    {
        qemu_put_kbd_event_n(qemu_put_kbd_event_n_opaque, keycodes, count);
    }
    else if (qemu_put_kbd_event)
    {
        int  nn;

        for (nn = 0; nn < count; nn++)
            qemu_put_kbd_event(qemu_put_kbd_event_opaque, keycodes[nn]);
    }
}



void kbd_mouse_event(int dx, int dy, int dz, int buttons_state)
{
    if (qemu_put_mouse_event) {
        qemu_put_mouse_event(qemu_put_mouse_event_opaque,
                             dx, dy, dz, buttons_state);
    }
}

int kbd_mouse_is_absolute(void)
{
    return qemu_put_mouse_event_absolute;
}

void  kbd_generic_event(int  type, int code, int  value)
{
    if (qemu_put_generic_event)
        qemu_put_generic_event(qemu_put_generic_event_opaque, type, code, value);
}

/* compute with 96 bit intermediate result: (a*b)/c */
uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    union {
        uint64_t ll;
        struct {
#ifdef WORDS_BIGENDIAN
            uint32_t high, low;
#else
            uint32_t low, high;
#endif
        } l;
    } u, res;
    uint64_t rl, rh;

    u.ll = a;
    rl = (uint64_t)u.l.low * (uint64_t)b;
    rh = (uint64_t)u.l.high * (uint64_t)b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;
}


/* this function is called each time the alarm timer is signaled */
static void
cpu_signal_alarm( void )
{
    CPUState *env = cur_cpu;

    if (env) {
        /* stop the currently executing cpu because a timer occured */
        cpu_interrupt(env, CPU_INTERRUPT_EXIT);
#ifdef USE_KQEMU
        if (env->kqemu_enabled) {
            kqemu_cpu_interrupt(env);
        }
#endif
    }
    event_pending = 1;
}




#define  get_clock()  qemu_get_elapsed_ns()

#ifdef CONFIG_TRACE
static int tbflush_requested;
static int exit_requested;

extern void  qemu_trace_enable();
extern void  qemu_trace_enable_gencode();
extern void  qemu_trace_enable_dyngen();
extern void  qemu_trace_disable();
extern void  qemu_trace_disable_gencode();
extern void  qemu_trace_disable_dyngen();

void start_tracing()
{
  if (trace_filename == NULL)
    return;
  if (!tracing) {
    fprintf(stderr,"-- start tracing --\n");
    start_time = Now();
  }
  tracing = 1;
  tbflush_requested = 1;
  qemu_polling_enable();
  if (cpu_single_env)
    cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

void stop_tracing()
{
  if (trace_filename == NULL)
    return;
  if (tracing) {
    end_time = Now();
    elapsed_usecs += end_time - start_time;
    fprintf(stderr,"-- stop tracing --\n");
  }
  tracing = 0;
  tbflush_requested = 1;
  qemu_polling_disable();
  if (cpu_single_env)
    cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

#ifndef _WIN32
/* This is the handler for the SIGUSR1 and SIGUSR2 signals.
 * SIGUSR1 turns tracing on.  SIGUSR2 turns tracing off.
 */
void sigusr_handler(int sig)
{
  if (sig == SIGUSR1)
    start_tracing();
  else
    stop_tracing();
}
#endif

#endif /* CONFIG_TRACE */

/* This is the handler to catch control-C so that we can exit cleanly.
 * This is needed when tracing to flush the buffers to disk.
 */
void sigint_handler(int sig)
{
  exit_requested = 1;
  if (cpu_single_env)
    cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

/***********************************************************/
/* character device */

int qemu_chr_write(CharDriverState *s, const uint8_t *buf, int len)
{
    return s->chr_write(s, buf, len);
}

int qemu_chr_ioctl(CharDriverState *s, int cmd, void *arg)
{
    if (!s->chr_ioctl)
        return -ENOTSUP;
    return s->chr_ioctl(s, cmd, arg);
}

void qemu_chr_printf(CharDriverState *s, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    qemu_chr_write(s, (const uint8_t*)buf, strlen(buf));
    va_end(ap);
}

void qemu_chr_send_event(CharDriverState *s, int event)
{
    if (s->chr_send_event)
        s->chr_send_event(s, event);
}

void qemu_chr_add_read_handler(CharDriverState *s,
                               IOCanRWHandler *fd_can_read,
                               IOReadHandler *fd_read, void *opaque)
{
    s->chr_add_read_handler(s, fd_can_read, fd_read, opaque);
}

void qemu_chr_add_event_handler(CharDriverState *s, IOEventHandler *chr_event)
{
    s->chr_event = chr_event;
}

static int null_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    return len;
}

static void null_chr_add_read_handler(CharDriverState *chr,
                                    IOCanRWHandler *fd_can_read,
                                    IOReadHandler *fd_read, void *opaque)
{
}

CharDriverState *qemu_chr_open_null(void)
{
    CharDriverState *chr;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    chr->chr_write = null_chr_write;
    chr->chr_add_read_handler = null_chr_add_read_handler;
    return chr;
}

#ifdef _WIN32

static int send_all(int fd, const uint8_t *buf, int len1)
{
    int ret, len;

    len = len1;
    while (len > 0) {
        ret = send(fd, buf, len, 0);
        if (ret < 0) {
            int errno;
            errno = WSAGetLastError();
            if (errno != WSAEWOULDBLOCK) {
                return -1;
            }
        } else if (ret == 0) {
            break;
        } else {
            buf += ret;
            len -= ret;
        }
    }
    return len1 - len;
}

#else

static int unix_write(int fd, const uint8_t *buf, int len1)
{
    int ret, len;

    len = len1;
    while (len > 0) {
        ret = write(fd, buf, len);
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN)
                return -1;
        } else if (ret == 0) {
            break;
        } else {
            buf += ret;
            len -= ret;
        }
    }
    return len1 - len;
}

static inline int send_all(int fd, const uint8_t *buf, int len1)
{
    return unix_write(fd, buf, len1);
}

#endif /* !_WIN32 */

#ifndef _WIN32

typedef struct {
    int fd_in, fd_out;
    IOCanRWHandler *fd_can_read;
    IOReadHandler *fd_read;
    void *fd_opaque;
    int max_size;
} FDCharDriver;

#define STDIO_MAX_CLIENTS 2

static int stdio_nb_clients;
static CharDriverState *stdio_clients[STDIO_MAX_CLIENTS];

static int fd_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    FDCharDriver *s = chr->opaque;
    return unix_write(s->fd_out, buf, len);
}

static int fd_chr_read_poll(void *opaque)
{
    CharDriverState *chr = opaque;
    FDCharDriver *s = chr->opaque;

    s->max_size = s->fd_can_read(s->fd_opaque);
    return s->max_size;
}

static void fd_chr_read(void *opaque)
{
    CharDriverState *chr = opaque;
    FDCharDriver *s = chr->opaque;
    int size, len;
    uint8_t buf[1024];

    len = sizeof(buf);
    if (len > s->max_size)
        len = s->max_size;
    if (len == 0)
        return;
    size = read(s->fd_in, buf, len);
    if (size > 0) {
        s->fd_read(s->fd_opaque, buf, size);
    }
}

static void fd_chr_add_read_handler(CharDriverState *chr,
                                    IOCanRWHandler *fd_can_read,
                                    IOReadHandler *fd_read, void *opaque)
{
    FDCharDriver *s = chr->opaque;

    if (s->fd_in >= 0) {
        s->fd_can_read = fd_can_read;
        s->fd_read = fd_read;
        s->fd_opaque = opaque;
        if (nographic && s->fd_in == 0) {
        } else {
            qemu_set_fd_handler2(s->fd_in, fd_chr_read_poll,
                                 fd_chr_read, NULL, chr);
        }
    }
}

/* open a character device to a unix fd */
CharDriverState *qemu_chr_open_fd(int fd_in, int fd_out)
{
    CharDriverState *chr;
    FDCharDriver *s;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    s = qemu_mallocz(sizeof(FDCharDriver));
    if (!s) {
        free(chr);
        return NULL;
    }
    s->fd_in = fd_in;
    s->fd_out = fd_out;
    chr->opaque = s;
    chr->chr_write = fd_chr_write;
    chr->chr_add_read_handler = fd_chr_add_read_handler;
    return chr;
}

CharDriverState *qemu_chr_open_file_out(const char *file_out)
{
    int fd_out;

    fd_out = open(file_out, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, 0666);
    if (fd_out < 0)
        return NULL;
    return qemu_chr_open_fd(-1, fd_out);
}

CharDriverState *qemu_chr_open_pipe(const char *filename)
{
    int fd;

    fd = open(filename, O_RDWR | O_BINARY);
    if (fd < 0)
        return NULL;
    return qemu_chr_open_fd(fd, fd);
}

CharDriverState *qemu_chr_open_fdpair(const char *fd_pair)
{
    int fd_in, fd_out;
    char *endptr;

    /* fd_pair should contain two decimal fd values, separated by
     * a colon. */
    endptr = NULL;
    fd_in = strtol(fd_pair, &endptr, 10);
    if (endptr == NULL || endptr == fd_pair || *endptr != ':')
        return NULL;
    endptr++;   // skip colon
    fd_pair = endptr;
    endptr = NULL;
    fd_out = strtol(fd_pair, &endptr, 10);
    if (endptr == NULL || endptr == fd_pair || *endptr != '\0')
        return NULL;

    return qemu_chr_open_fd(fd_in, fd_out);
}


/* for STDIO, we handle the case where several clients use it
   (nographic mode) */

#define TERM_ESCAPE 0x01 /* ctrl-a is used for escape */

#define TERM_FIFO_MAX_SIZE 1

static int term_got_escape, client_index;
static uint8_t term_fifo[TERM_FIFO_MAX_SIZE];
static int term_fifo_size;
static int term_timestamps;
static int64_t term_timestamps_start;

void term_print_help(void)
{
    printf("\n"
           "C-a h    print this help\n"
           "C-a x    exit emulator\n"
           "C-a s    save disk data back to file (if -snapshot)\n"
           "C-a b    send break (magic sysrq)\n"
           "C-a t    toggle console timestamps\n"
           "C-a c    switch between console and monitor\n"
           "C-a C-a  send C-a\n"
           );
}

/* called when a char is received */
static void stdio_received_byte(int ch)
{
    if (term_got_escape) {
        term_got_escape = 0;
        switch(ch) {
        case 'h':
            term_print_help();
            break;
        case 'x':
            exit(0);
            break;
        case 's':
            {
                int i;
                for (i = 0; i < MAX_DISKS; i++) {
                    if (bs_table[i])
                        bdrv_commit(bs_table[i]);
                }
            }
            break;
        case 'b':
            if (client_index < stdio_nb_clients) {
                CharDriverState *chr;
                FDCharDriver *s;

                chr = stdio_clients[client_index];
                s = chr->opaque;
                chr->chr_event(s->fd_opaque, CHR_EVENT_BREAK);
            }
            break;
        case 'c':
            client_index++;
            if (client_index >= stdio_nb_clients)
                client_index = 0;
            if (client_index == 0) {
                /* send a new line in the monitor to get the prompt */
                ch = '\r';
                goto send_char;
            }
            break;
        case 't':
            term_timestamps = !term_timestamps;
            term_timestamps_start = -1;
            break;
        case TERM_ESCAPE:
            goto send_char;
        }
    } else if (ch == TERM_ESCAPE) {
        term_got_escape = 1;
    } else {
    send_char:
        if (client_index < stdio_nb_clients) {
            uint8_t buf[1];
            CharDriverState *chr;
            FDCharDriver *s;

            chr = stdio_clients[client_index];
            s = chr->opaque;
            if (s->fd_can_read(s->fd_opaque) > 0) {
                buf[0] = ch;
                s->fd_read(s->fd_opaque, buf, 1);
            } else if (term_fifo_size == 0) {
                term_fifo[term_fifo_size++] = ch;
            }
        }
    }
}

static int stdio_read_poll(void *opaque)
{
    CharDriverState *chr;
    FDCharDriver *s;

    if (client_index < stdio_nb_clients) {
        chr = stdio_clients[client_index];
        s = chr->opaque;
        /* try to flush the queue if needed */
        if (term_fifo_size != 0 && s->fd_can_read(s->fd_opaque) > 0) {
            s->fd_read(s->fd_opaque, term_fifo, 1);
            term_fifo_size = 0;
        }
        /* see if we can absorb more chars */
        if (term_fifo_size == 0)
            return 1;
        else
            return 0;
    } else {
        return 1;
    }
}

static void stdio_read(void *opaque)
{
    int size;
    uint8_t buf[1];

    size = read(0, buf, 1);
    if (size > 0)
        stdio_received_byte(buf[0]);
}

static int stdio_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    FDCharDriver *s = chr->opaque;
    if (!term_timestamps) {
        return unix_write(s->fd_out, buf, len);
    } else {
        int i;
        char buf1[64];

        for(i = 0; i < len; i++) {
            unix_write(s->fd_out, buf + i, 1);
            if (buf[i] == '\n') {
                int64_t ti;
                int secs;

                ti = get_clock();
                if (term_timestamps_start == -1)
                    term_timestamps_start = ti;
                ti -= term_timestamps_start;
                secs = ti / 1000000000;
                snprintf(buf1, sizeof(buf1),
                         "[%02d:%02d:%02d.%03d] ",
                         secs / 3600,
                         (secs / 60) % 60,
                         secs % 60,
                         (int)((ti / 1000000) % 1000));
                unix_write(s->fd_out, (const uint8_t*)buf1, strlen(buf1));
            }
        }
        return len;
    }
}

/* init terminal so that we can grab keys */
static struct termios oldtty;
static int old_fd0_flags;

static void term_exit(void)
{
    tcsetattr (0, TCSANOW, &oldtty);
    fcntl(0, F_SETFL, old_fd0_flags);
}

static void term_init(void)
{
    struct termios tty;

    tcgetattr (0, &tty);
    oldtty = tty;
    old_fd0_flags = fcntl(0, F_GETFL);

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                          |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
    /* if graphical mode, we allow Ctrl-C handling */
    if (nographic)
        tty.c_lflag &= ~ISIG;
    tty.c_cflag &= ~(CSIZE|PARENB);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr (0, TCSANOW, &tty);

    atexit(term_exit);

    fcntl(0, F_SETFL, O_NONBLOCK);
}

CharDriverState *qemu_chr_open_stdio(void)
{
    CharDriverState *chr;

    if (nographic) {
        if (stdio_nb_clients >= STDIO_MAX_CLIENTS)
            return NULL;
        chr = qemu_chr_open_fd(0, 1);
        chr->chr_write = stdio_write;
        if (stdio_nb_clients == 0)
            qemu_set_fd_handler2(0, stdio_read_poll, stdio_read, NULL, NULL);
        client_index = stdio_nb_clients;
    } else {
        if (stdio_nb_clients != 0)
            return NULL;
        chr = qemu_chr_open_fd(0, 1);
    }
    stdio_clients[stdio_nb_clients++] = chr;
    if (stdio_nb_clients == 1) {
        /* set the terminal in raw mode */
        term_init();
    }
    return chr;
}

#if defined(__linux__)
CharDriverState *qemu_chr_open_pty(void)
{
    struct termios tty;
    char slave_name[1024];
    int master_fd, slave_fd;

    /* Not satisfying */
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) < 0) {
        return NULL;
    }

    /* Disabling local echo and line-buffered output */
    tcgetattr (master_fd, &tty);
    tty.c_lflag &= ~(ECHO|ICANON|ISIG);
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;
    tcsetattr (master_fd, TCSAFLUSH, &tty);

    fprintf(stderr, "char device redirected to %s\n", slave_name);
    return qemu_chr_open_fd(master_fd, master_fd);
}
#endif

static void tty_serial_init(int fd, int speed,
                            int parity, int data_bits, int stop_bits)
{
    struct termios tty;
    speed_t spd;

#if 0
    printf("tty_serial_init: speed=%d parity=%c data=%d stop=%d\n",
           speed, parity, data_bits, stop_bits);
#endif
    tcgetattr (fd, &tty);

    switch(speed) {
    case 50:
        spd = B50;
        break;
    case 75:
        spd = B75;
        break;
    case 300:
        spd = B300;
        break;
    case 600:
        spd = B600;
        break;
    case 1200:
        spd = B1200;
        break;
    case 2400:
        spd = B2400;
        break;
    case 4800:
        spd = B4800;
        break;
    case 9600:
        spd = B9600;
        break;
    case 19200:
        spd = B19200;
        break;
    case 38400:
        spd = B38400;
        break;
    case 57600:
        spd = B57600;
        break;
    default:
    case 115200:
        spd = B115200;
        break;
    }

    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                          |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN|ISIG);
    tty.c_cflag &= ~(CSIZE|PARENB|PARODD|CRTSCTS);
    switch(data_bits) {
    default:
    case 8:
        tty.c_cflag |= CS8;
        break;
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 6:
        tty.c_cflag |= CS6;
        break;
    case 5:
        tty.c_cflag |= CS5;
        break;
    }
    switch(parity) {
    default:
    case 'N':
        break;
    case 'E':
        tty.c_cflag |= PARENB;
        break;
    case 'O':
        tty.c_cflag |= PARENB | PARODD;
        break;
    }

    tcsetattr (fd, TCSANOW, &tty);
}

static int tty_serial_ioctl(CharDriverState *chr, int cmd, void *arg)
{
    FDCharDriver *s = chr->opaque;

    switch(cmd) {
    case CHR_IOCTL_SERIAL_SET_PARAMS:
        {
            QEMUSerialSetParams *ssp = arg;
            tty_serial_init(s->fd_in, ssp->speed, ssp->parity,
                            ssp->data_bits, ssp->stop_bits);
        }
        break;
    case CHR_IOCTL_SERIAL_SET_BREAK:
        {
            int enable = *(int *)arg;
            if (enable)
                tcsendbreak(s->fd_in, 1);
        }
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}

CharDriverState *qemu_chr_open_tty(const char *filename)
{
    CharDriverState *chr;
    int fd;

    fd = open(filename, O_RDWR | O_NONBLOCK | O_BINARY);
    if (fd < 0)
        return NULL;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    tty_serial_init(fd, 115200, 'N', 8, 1);
    chr = qemu_chr_open_fd(fd, fd);
    if (!chr)
        return NULL;
    chr->chr_ioctl = tty_serial_ioctl;
    return chr;
}

#if defined(__linux__)
static int pp_ioctl(CharDriverState *chr, int cmd, void *arg)
{
    int fd = (int)chr->opaque;
    uint8_t b;

    switch(cmd) {
    case CHR_IOCTL_PP_READ_DATA:
        if (ioctl(fd, PPRDATA, &b) < 0)
            return -ENOTSUP;
        *(uint8_t *)arg = b;
        break;
    case CHR_IOCTL_PP_WRITE_DATA:
        b = *(uint8_t *)arg;
        if (ioctl(fd, PPWDATA, &b) < 0)
            return -ENOTSUP;
        break;
    case CHR_IOCTL_PP_READ_CONTROL:
        if (ioctl(fd, PPRCONTROL, &b) < 0)
            return -ENOTSUP;
        *(uint8_t *)arg = b;
        break;
    case CHR_IOCTL_PP_WRITE_CONTROL:
        b = *(uint8_t *)arg;
        if (ioctl(fd, PPWCONTROL, &b) < 0)
            return -ENOTSUP;
        break;
    case CHR_IOCTL_PP_READ_STATUS:
        if (ioctl(fd, PPRSTATUS, &b) < 0)
            return -ENOTSUP;
        *(uint8_t *)arg = b;
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}

CharDriverState *qemu_chr_open_pp(const char *filename)
{
    CharDriverState *chr;
    int fd;

    fd = open(filename, O_RDWR);
    if (fd < 0)
        return NULL;

    if (ioctl(fd, PPCLAIM) < 0) {
        close(fd);
        return NULL;
    }

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr) {
        close(fd);
        return NULL;
    }
    chr->opaque = (void *)fd;
    chr->chr_write = null_chr_write;
    chr->chr_add_read_handler = null_chr_add_read_handler;
    chr->chr_ioctl = pp_ioctl;
    return chr;
}
#else
CharDriverState *qemu_chr_open_pty(void)
{
    return NULL;
}
#endif

#endif /* !defined(_WIN32) */

#ifdef _WIN32

typedef int  (*WinPollFunc)(void*  opaque);

typedef struct {
    IOCanRWHandler *fd_can_read;
    IOReadHandler *fd_read;
    void *win_opaque;
    int max_size;
    HANDLE hcom, rcom, hrecv, hsend;
    OVERLAPPED orecv, osend;
    BOOL fpipe;
    DWORD len;
    WinPollFunc  poll_func;
} WinCharState;

#define NSENDBUF 2048
#define NRECVBUF 2048
#define MAXCONNECT 1
#define NTIMEOUT 5000

static int win_chr_poll(void *opaque);
static int win_chr_pipe_poll(void *opaque);

static void win_chr_close2(WinCharState *s)
{
    if (s->hsend) {
        CloseHandle(s->hsend);
        s->hsend = NULL;
    }
    if (s->hrecv) {
        CloseHandle(s->hrecv);
        s->hrecv = NULL;
    }
    if (s->rcom && s->rcom != s->hcom) {
        CloseHandle(s->rcom);
        s->rcom = NULL;
    }
    if (s->hcom) {
        CloseHandle(s->hcom);
        s->hcom = NULL;
    }
    if (s->poll_func)
        qemu_del_polling_cb(s->poll_func, s);
}

static void win_chr_close(CharDriverState *chr)
{
    WinCharState *s = chr->opaque;
    win_chr_close2(s);
}

static int win_chr_init(WinCharState *s, const char *filename)
{
    COMMCONFIG comcfg;
    COMMTIMEOUTS cto = { 0, 0, 0, 0, 0};
    COMSTAT comstat;
    DWORD size;
    DWORD err;

    s->hsend = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!s->hsend) {
        fprintf(stderr, "Failed CreateEvent\n");
        goto fail;
    }
    s->hrecv = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!s->hrecv) {
        fprintf(stderr, "Failed CreateEvent\n");
        goto fail;
    }

    s->hcom = CreateFile(filename, GENERIC_READ|GENERIC_WRITE, 0, NULL,
                      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (s->hcom == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed CreateFile (%lu)\n", GetLastError());
        s->hcom = NULL;
        goto fail;
    }
    s->rcom = s->hcom;

    if (!SetupComm(s->hcom, NRECVBUF, NSENDBUF)) {
        fprintf(stderr, "Failed SetupComm\n");
        goto fail;
    }

    ZeroMemory(&comcfg, sizeof(COMMCONFIG));
    size = sizeof(COMMCONFIG);
    GetDefaultCommConfig(filename, &comcfg, &size);
    comcfg.dcb.DCBlength = sizeof(DCB);
    CommConfigDialog(filename, NULL, &comcfg);

    if (!SetCommState(s->hcom, &comcfg.dcb)) {
        fprintf(stderr, "Failed SetCommState\n");
        goto fail;
    }

    if (!SetCommMask(s->hcom, EV_ERR)) {
        fprintf(stderr, "Failed SetCommMask\n");
        goto fail;
    }

    cto.ReadIntervalTimeout = MAXDWORD;
    if (!SetCommTimeouts(s->hcom, &cto)) {
        fprintf(stderr, "Failed SetCommTimeouts\n");
        goto fail;
    }

    if (!ClearCommError(s->hcom, &err, &comstat)) {
        fprintf(stderr, "Failed ClearCommError\n");
        goto fail;
    }
    s->poll_func = win_chr_poll;
    qemu_add_polling_cb(s->poll_func, s);
    return 0;

 fail:
    win_chr_close2(s);
    return -1;
}

static int win_chr_write(CharDriverState *chr, const uint8_t *buf, int len1)
{
    WinCharState *s = chr->opaque;
    DWORD len, ret, size, err;

    len = len1;
    ZeroMemory(&s->osend, sizeof(s->osend));
    s->osend.hEvent = s->hsend;
    while (len > 0) {
        if (s->hsend)
            ret = WriteFile(s->hcom, buf, len, &size, &s->osend);
        else
            ret = WriteFile(s->hcom, buf, len, &size, NULL);
        if (!ret) {
            err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                ret = GetOverlappedResult(s->hcom, &s->osend, &size, TRUE);
                if (ret) {
                    buf += size;
                    len -= size;
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            buf += size;
            len -= size;
        }
    }
    return len1 - len;
}

static int win_chr_read_poll(WinCharState *s)
{
    s->max_size = s->fd_can_read(s->win_opaque);
    return s->max_size;
}

static void win_chr_readfile(WinCharState *s)
{
    int ret, err;
    uint8_t buf[1024];
    DWORD size;

    ZeroMemory(&s->orecv, sizeof(s->orecv));
    s->orecv.hEvent = s->hrecv;
    ret = ReadFile(s->rcom, buf, s->len, &size, &s->orecv);
    if (!ret) {
        err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            ret = GetOverlappedResult(s->rcom, &s->orecv, &size, TRUE);
        }
    }

    if (size > 0) {
        s->fd_read(s->win_opaque, buf, size);
    }
}

static void win_chr_read(WinCharState *s)
{
    if (s->len > s->max_size)
        s->len = s->max_size;
    if (s->len == 0)
        return;

    win_chr_readfile(s);
}

static int win_chr_poll(void *opaque)
{
    WinCharState *s = opaque;
    COMSTAT status;
    DWORD comerr;

    ClearCommError(s->rcom, &comerr, &status);
    if (status.cbInQue > 0) {
        s->len = status.cbInQue;
        win_chr_read_poll(s);
        win_chr_read(s);
        return 1;
    }
    return 0;
}

static void win_chr_add_read_handler(CharDriverState *chr,
                                    IOCanRWHandler *fd_can_read,
                                    IOReadHandler *fd_read, void *opaque)
{
    WinCharState *s = chr->opaque;

    s->fd_can_read = fd_can_read;
    s->fd_read = fd_read;
    s->win_opaque = opaque;
}

CharDriverState *qemu_chr_open_win(const char *filename)
{
    CharDriverState *chr;
    WinCharState *s;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    s = qemu_mallocz(sizeof(WinCharState));
    if (!s) {
        free(chr);
        return NULL;
    }
    chr->opaque = s;
    chr->chr_write = win_chr_write;
    chr->chr_add_read_handler = win_chr_add_read_handler;
    chr->chr_close = win_chr_close;

    if (win_chr_init(s, filename) < 0) {
        free(s);
        free(chr);
        return NULL;
    }
    return chr;
}

static int win_chr_pipe_poll(void *opaque)
{
    WinCharState *s = opaque;
    DWORD size;

    PeekNamedPipe(s->hcom, NULL, 0, NULL, &size, NULL);
    if (size > 0) {
        s->len = size;
        win_chr_read_poll(s);
        win_chr_read(s);
        return 1;
    }
    return 0;
}

static int win_chr_pipe_init(WinCharState *s, const char *filename)
{
    OVERLAPPED ov;
    int ret;
    DWORD size;
    char openname[256];

    s->fpipe = TRUE;

    s->hsend = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!s->hsend) {
        fprintf(stderr, "Failed CreateEvent\n");
        goto fail;
    }
    s->hrecv = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!s->hrecv) {
        fprintf(stderr, "Failed CreateEvent\n");
        goto fail;
    }

    snprintf(openname, sizeof(openname), "\\\\.\\pipe\\%s", filename);
    s->hcom = CreateNamedPipe(openname, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE |
                              PIPE_WAIT,
                              MAXCONNECT, NSENDBUF, NRECVBUF, NTIMEOUT, NULL);
    if (s->hcom == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed CreateNamedPipe (%lu)\n", GetLastError());
        s->hcom = NULL;
        goto fail;
    }
    s->rcom = s->hcom;

    ZeroMemory(&ov, sizeof(ov));
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ret = ConnectNamedPipe(s->hcom, &ov);
    if (ret) {
        fprintf(stderr, "Failed ConnectNamedPipe\n");
        goto fail;
    }

    ret = GetOverlappedResult(s->hcom, &ov, &size, TRUE);
    if (!ret) {
        fprintf(stderr, "Failed GetOverlappedResult\n");
        if (ov.hEvent) {
            CloseHandle(ov.hEvent);
            ov.hEvent = NULL;
        }
        goto fail;
    }

    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
        ov.hEvent = NULL;
    }
    s->poll_func = win_chr_pipe_poll;
    qemu_add_polling_cb(s->poll_func, s);
    return 0;

 fail:
    win_chr_close2(s);
    return -1;
}


CharDriverState *qemu_chr_open_win_pipe(const char *filename)
{
    CharDriverState *chr;
    WinCharState *s;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    s = qemu_mallocz(sizeof(WinCharState));
    if (!s) {
        free(chr);
        return NULL;
    }
    chr->opaque = s;
    chr->chr_write = win_chr_write;
    chr->chr_add_read_handler = win_chr_add_read_handler;
    chr->chr_close = win_chr_close;

    if (win_chr_pipe_init(s, filename) < 0) {
        free(s);
        free(chr);
        return NULL;
    }
    return chr;
}

CharDriverState *qemu_chr_open_win_file(HANDLE fd_out)
{
    CharDriverState *chr;
    WinCharState *s;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    s = qemu_mallocz(sizeof(WinCharState));
    if (!s) {
        free(chr);
        return NULL;
    }
    s->hcom = fd_out;
    s->rcom = fd_out;
    chr->opaque = s;
    chr->chr_write = win_chr_write;
    chr->chr_add_read_handler = win_chr_add_read_handler;
    return chr;
}

static int win_chr_stdio_poll(void *opaque)
{
    WinCharState *s      = opaque;
    int           result = 0;

    for (;;) {
        INPUT_RECORD  rec;
        DWORD         count;

        if ( !PeekConsoleInput(s->rcom, &rec, 1, &count) || count == 0 )
            break;

        ReadConsoleInput(s->rcom, &rec, 1, &count);

        if (rec.EventType == KEY_EVENT)
        {
            if (rec.Event.KeyEvent.bKeyDown       &&
                rec.Event.KeyEvent.uChar.AsciiChar)
            {
                char  c = rec.Event.KeyEvent.uChar.AsciiChar;
                s->len = 1;
                win_chr_read_poll(s);
                s->fd_read( s->win_opaque, &c, 1);
                result = 1;
            }
        }
    }
    return  result;
}

CharDriverState *qemu_chr_open_win_stdio(void)
{
    CharDriverState *chr;
    WinCharState *s;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        return NULL;
    s = qemu_mallocz(sizeof(WinCharState));
    if (!s) {
        free(chr);
        return NULL;
    }
    s->hcom = GetStdHandle( STD_OUTPUT_HANDLE );
    s->rcom = GetStdHandle( STD_INPUT_HANDLE );
    chr->opaque = s;
    chr->chr_write = win_chr_write;
    chr->chr_add_read_handler = win_chr_add_read_handler;
    s->poll_func = win_chr_stdio_poll;
    qemu_add_polling_cb(s->poll_func, s);
    return chr;
}

CharDriverState *qemu_chr_open_win_file_out(const char *file_out)
{
    HANDLE fd_out;

    fd_out = CreateFile(file_out, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fd_out == INVALID_HANDLE_VALUE)
        return NULL;

    return qemu_chr_open_win_file(fd_out);
}
#endif

/***********************************************************/
/* UDP Net console */

typedef struct {
    IOCanRWHandler *fd_can_read;
    IOReadHandler *fd_read;
    void *fd_opaque;
    int fd;
    struct sockaddr_in daddr;
    char buf[1024];
    int bufcnt;
    int bufptr;
    int max_size;
} NetCharDriver;

static int udp_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    NetCharDriver *s = chr->opaque;

    return sendto(s->fd, buf, len, 0,
                  (struct sockaddr *)&s->daddr, sizeof(struct sockaddr_in));
}

static int udp_chr_read_poll(void *opaque)
{
    CharDriverState *chr = opaque;
    NetCharDriver *s = chr->opaque;

    s->max_size = s->fd_can_read(s->fd_opaque);

    /* If there were any stray characters in the queue process them
     * first
     */
    while (s->max_size > 0 && s->bufptr < s->bufcnt) {
        s->fd_read(s->fd_opaque, (uint8_t*)&s->buf[s->bufptr], 1);
        s->bufptr++;
        s->max_size = s->fd_can_read(s->fd_opaque);
    }
    return s->max_size;
}

static void udp_chr_read(void *opaque)
{
    CharDriverState *chr = opaque;
    NetCharDriver *s = chr->opaque;

    if (s->max_size == 0)
        return;
    s->bufcnt = recv(s->fd, s->buf, sizeof(s->buf), 0);
    s->bufptr = s->bufcnt;
    if (s->bufcnt <= 0)
        return;

    s->bufptr = 0;
    while (s->max_size > 0 && s->bufptr < s->bufcnt) {
        s->fd_read(s->fd_opaque, (uint8_t*)&s->buf[s->bufptr], 1);
        s->bufptr++;
        s->max_size = s->fd_can_read(s->fd_opaque);
    }
}

static void udp_chr_add_read_handler(CharDriverState *chr,
                                    IOCanRWHandler *fd_can_read,
                                    IOReadHandler *fd_read, void *opaque)
{
    NetCharDriver *s = chr->opaque;

    if (s->fd >= 0) {
        s->fd_can_read = fd_can_read;
        s->fd_read = fd_read;
        s->fd_opaque = opaque;
        qemu_set_fd_handler2(s->fd, udp_chr_read_poll,
                             udp_chr_read, NULL, chr);
    }
}

int parse_host_port(struct sockaddr_in *saddr, const char *str);
int parse_host_src_port(struct sockaddr_in *haddr,
                        struct sockaddr_in *saddr,
                        const char *str);

CharDriverState *qemu_chr_open_udp(const char *def)
{
    CharDriverState *chr = NULL;
    NetCharDriver *s = NULL;
    int fd = -1;
    struct sockaddr_in saddr;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        goto return_err;
    s = qemu_mallocz(sizeof(NetCharDriver));
    if (!s)
        goto return_err;

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(PF_INET, SOCK_DGRAM)");
        goto return_err;
    }

    if (parse_host_src_port(&s->daddr, &saddr, def) < 0) {
        printf("Could not parse: %s\n", def);
        goto return_err;
    }

    if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind");
        goto return_err;
    }

    s->fd = fd;
    s->bufcnt = 0;
    s->bufptr = 0;
    chr->opaque = s;
    chr->chr_write = udp_chr_write;
    chr->chr_add_read_handler = udp_chr_add_read_handler;
    return chr;

return_err:
    if (chr)
        free(chr);
    if (s)
        free(s);
    if (fd >= 0)
        socket_close(fd);
    return NULL;
}

/***********************************************************/
/* TCP Net console */

typedef struct {
    IOCanRWHandler *fd_can_read;
    IOReadHandler *fd_read;
    void *fd_opaque;
    int fd, listen_fd;
    int connected;
    int max_size;
    int do_telnetopt;
} TCPCharDriver;

static void tcp_chr_accept(void *opaque);

static int tcp_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    TCPCharDriver *s = chr->opaque;
    if (s->connected) {
        return send_all(s->fd, buf, len);
    } else {
        /* XXX: indicate an error ? */
        return len;
    }
}

static int tcp_chr_read_poll(void *opaque)
{
    CharDriverState *chr = opaque;
    TCPCharDriver *s = chr->opaque;
    if (!s->connected)
        return 0;
    s->max_size = s->fd_can_read(s->fd_opaque);
    return s->max_size;
}

#define IAC 255
#define IAC_BREAK 243
static void tcp_chr_process_IAC_bytes(CharDriverState *chr,
                                      TCPCharDriver *s,
                                      char *buf, int *size)
{
    /* Handle any telnet client's basic IAC options to satisfy char by
     * char mode with no echo.  All IAC options will be removed from
     * the buf and the do_telnetopt variable will be used to track the
     * state of the width of the IAC information.
     *
     * IAC commands come in sets of 3 bytes with the exception of the
     * "IAC BREAK" command and the double IAC.
     */

    int i;
    int j = 0;

    for (i = 0; i < *size; i++) {
        if (s->do_telnetopt > 1) {
            if ((unsigned char)buf[i] == IAC && s->do_telnetopt == 2) {
                /* Double IAC means send an IAC */
                if (j != i)
                    buf[j] = buf[i];
                j++;
                s->do_telnetopt = 1;
            } else {
                if ((unsigned char)buf[i] == IAC_BREAK && s->do_telnetopt == 2) {
                    /* Handle IAC break commands by sending a serial break */
                    chr->chr_event(s->fd_opaque, CHR_EVENT_BREAK);
                    s->do_telnetopt++;
                }
                s->do_telnetopt++;
            }
            if (s->do_telnetopt >= 4) {
                s->do_telnetopt = 1;
            }
        } else {
            if ((unsigned char)buf[i] == IAC) {
                s->do_telnetopt = 2;
            } else {
                if (j != i)
                    buf[j] = buf[i];
                j++;
            }
        }
    }
    *size = j;
}

static void tcp_chr_read(void *opaque)
{
    CharDriverState *chr = opaque;
    TCPCharDriver *s = chr->opaque;
    uint8_t buf[1024];
    int len, size;

    if (!s->connected || s->max_size <= 0)
        return;
    len = sizeof(buf);
    if (len > s->max_size)
        len = s->max_size;
    size = recv(s->fd, buf, len, 0);
    if (size == 0) {
        /* connection closed */
        s->connected = 0;
        if (s->listen_fd >= 0) {
            qemu_set_fd_handler(s->listen_fd, tcp_chr_accept, NULL, chr);
        }
        qemu_set_fd_handler(s->fd, NULL, NULL, NULL);
        socket_close(s->fd);
        s->fd = -1;
    } else if (size > 0) {
        if (s->do_telnetopt)
            tcp_chr_process_IAC_bytes(chr, s, (char*)buf, &size);
        if (size > 0)
            s->fd_read(s->fd_opaque, buf, size);
    }
}

static void tcp_chr_add_read_handler(CharDriverState *chr,
                                     IOCanRWHandler *fd_can_read,
                                    IOReadHandler *fd_read, void *opaque)
{
    TCPCharDriver *s = chr->opaque;

    s->fd_can_read = fd_can_read;
    s->fd_read = fd_read;
    s->fd_opaque = opaque;
}

static void tcp_chr_connect(void *opaque)
{
    CharDriverState *chr = opaque;
    TCPCharDriver *s = chr->opaque;

    s->connected = 1;
    qemu_set_fd_handler2(s->fd, tcp_chr_read_poll,
                         tcp_chr_read, NULL, chr);
}

#define IACSET(x,a,b,c) x[0] = a; x[1] = b; x[2] = c;
static void tcp_chr_telnet_init(int fd)
{
    char buf[3];
    /* Send the telnet negotion to put telnet in binary, no echo, single char mode */
    IACSET(buf, 0xff, 0xfb, 0x01);  /* IAC WILL ECHO */
    send(fd, (char *)buf, 3, 0);
    IACSET(buf, 0xff, 0xfb, 0x03);  /* IAC WILL Suppress go ahead */
    send(fd, (char *)buf, 3, 0);
    IACSET(buf, 0xff, 0xfb, 0x00);  /* IAC WILL Binary */
    send(fd, (char *)buf, 3, 0);
    IACSET(buf, 0xff, 0xfd, 0x00);  /* IAC DO Binary */
    send(fd, (char *)buf, 3, 0);
}

static void tcp_chr_accept(void *opaque)
{
    CharDriverState *chr = opaque;
    TCPCharDriver *s = chr->opaque;
    struct sockaddr_in saddr;
    socklen_t len;
    int fd;

    for(;;) {
        len = sizeof(saddr);
        fd = accept(s->listen_fd, (struct sockaddr *)&saddr, &len);
        if (fd < 0 && errno != EINTR) {
            return;
        } else if (fd >= 0) {
            if (s->do_telnetopt)
                tcp_chr_telnet_init(fd);
            break;
        }
    }
    socket_set_nonblock(fd);
    s->fd = fd;
    qemu_set_fd_handler(s->listen_fd, NULL, NULL, NULL);
    tcp_chr_connect(chr);
}

static void tcp_chr_close(CharDriverState *chr)
{
    TCPCharDriver *s = chr->opaque;
    if (s->fd >= 0)
        socket_close(s->fd);
    if (s->listen_fd >= 0)
        socket_close(s->listen_fd);
    qemu_free(s);
}

static CharDriverState *qemu_chr_open_tcp(const char *host_str,
                                          int is_telnet)
{
    CharDriverState *chr = NULL;
    TCPCharDriver *s = NULL;
    int fd = -1, ret, err;
    int is_listen = 0;
    int is_waitconnect = 1;
    const char *ptr;
    struct sockaddr_in saddr;

    if (parse_host_port(&saddr, host_str) < 0)
        goto fail;

    ptr = host_str;
    while((ptr = strchr(ptr,','))) {
        ptr++;
        if (!strncmp(ptr,"server",6)) {
            is_listen = 1;
        } else if (!strncmp(ptr,"nowait",6)) {
            is_waitconnect = 0;
        } else {
            printf("Unknown option: %s\n", ptr);
            goto fail;
        }
    }
    if (!is_listen)
        is_waitconnect = 0;

    chr = qemu_mallocz(sizeof(CharDriverState));
    if (!chr)
        goto fail;
    s = qemu_mallocz(sizeof(TCPCharDriver));
    if (!s)
        goto fail;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        goto fail;

    if (!is_waitconnect)
        socket_set_nonblock(fd);

    s->connected = 0;
    s->fd = -1;
    s->listen_fd = -1;
    if (is_listen) {
        /* allow fast reuse */
        socket_set_xreuseaddr(fd);

        ret = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));
        if (ret < 0)
            goto fail;
        ret = listen(fd, 0);
        if (ret < 0)
            goto fail;
        s->listen_fd = fd;
        qemu_set_fd_handler(s->listen_fd, tcp_chr_accept, NULL, chr);
        if (is_telnet)
            s->do_telnetopt = 1;
    } else {
        for(;;) {
            ret = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
            if (ret < 0) {
                err = socket_errno;
                if (err == EINTR || err == EWOULDBLOCK) {
                } else if (err == EINPROGRESS) {
                    break;
                } else {
                    goto fail;
                }
            } else {
                s->connected = 1;
                break;
            }
        }
        s->fd = fd;
        if (s->connected)
            tcp_chr_connect(chr);
        else
            qemu_set_fd_handler(s->fd, NULL, tcp_chr_connect, chr);
    }

    chr->opaque = s;
    chr->chr_write = tcp_chr_write;
    chr->chr_add_read_handler = tcp_chr_add_read_handler;
    chr->chr_close = tcp_chr_close;
    if (is_listen && is_waitconnect) {
        printf("QEMU waiting for connection on: %s\n", host_str);
        tcp_chr_accept(chr);
        socket_set_nonblock(s->listen_fd);
    }

    return chr;
 fail:
    if (fd >= 0)
        socket_close(fd);
    qemu_free(s);
    qemu_free(chr);
    return NULL;
}

CharDriverState *qemu_chr_open(const char *filename)
{
    const char *p;

    if (!strcmp(filename, "vc")) {
        return text_console_init(&display_state);
    } else if (!strcmp(filename, "null")) {
        return qemu_chr_open_null();
    } else
    if (strstart(filename, "tcp:", &p)) {
        return qemu_chr_open_tcp(p, 0);
    } else
    if (strstart(filename, "telnet:", &p)) {
        return qemu_chr_open_tcp(p, 1);
    } else
    if (strstart(filename, "udp:", &p)) {
        return qemu_chr_open_udp(p);
    } else
#ifndef _WIN32
    if (strstart(filename, "file:", &p)) {
        return qemu_chr_open_file_out(p);
    } else if (strstart(filename, "pipe:", &p)) {
        return qemu_chr_open_pipe(p);
    } else if (!strcmp(filename, "pty")) {
        return qemu_chr_open_pty();
    } else if (!strcmp(filename, "stdio")) {
        return qemu_chr_open_stdio();
    } else if (strstart(filename, "fdpair:", &p)) {
        return qemu_chr_open_fdpair(p);
    } else
#endif
#if defined(__linux__)
    if (strstart(filename, "/dev/parport", NULL)) {
        return qemu_chr_open_pp(filename);
    } else
#endif
#ifndef _WIN32
    if (strstart(filename, "/dev/", NULL)) {
        return qemu_chr_open_tty(filename);
    } else
#endif
    if (!strcmp(filename, "android-modem")) {
        CharDriverState*  cs;
        qemu_chr_open_charpipe( &cs, &android_modem_cs );
        return cs;
    } else if (!strcmp(filename, "android-gps")) {
        CharDriverState*  cs;
        qemu_chr_open_charpipe( &cs, &android_gps_cs );
        return cs;
    } else if (!strcmp(filename, "android-kmsg")) {
        return android_kmsg_get_cs();
    } else if (!strcmp(filename, "android-qemud")) {
        return android_qemud_get_cs();
    } else
#ifdef _WIN32
    if (strstart(filename, "COM", NULL)) {
        return qemu_chr_open_win(filename);
    } else
    if (strstart(filename, "pipe:", &p)) {
        return qemu_chr_open_win_pipe(p);
    } else
    if (strstart(filename, "file:", &p)) {
        return qemu_chr_open_win_file_out(p);
    } else
    if (!strcmp(filename, "stdio")) {
        return qemu_chr_open_win_stdio();
    }
#endif
    {
        return NULL;
    }
}

void qemu_chr_close(CharDriverState *chr)
{
    if (chr->chr_close)
        chr->chr_close(chr);
}

/***********************************************************/
/* network device redirectors */

void hex_dump(FILE *f, const uint8_t *buf, int size)
{
    int len, i, j, c;

    for(i=0;i<size;i+=16) {
        len = size - i;
        if (len > 16)
            len = 16;
        fprintf(f, "%08x ", i);
        for(j=0;j<16;j++) {
            if (j < len)
                fprintf(f, " %02x", buf[i+j]);
            else
                fprintf(f, "   ");
        }
        fprintf(f, " ");
        for(j=0;j<len;j++) {
            c = buf[i+j];
            if (c < ' ' || c > '~')
                c = '.';
            fprintf(f, "%c", c);
        }
        fprintf(f, "\n");
    }
}

static int parse_macaddr(uint8_t *macaddr, const char *p)
{
    int i;
    for(i = 0; i < 6; i++) {
        macaddr[i] = strtol(p, (char **)&p, 16);
        if (i == 5) {
            if (*p != '\0')
                return -1;
        } else {
            if (*p != ':')
                return -1;
            p++;
        }
    }
    return 0;
}

static int get_str_sep(char *buf, int buf_size, const char **pp, int sep)
{
    const char *p, *p1;
    int len;
    p = *pp;
    p1 = strchr(p, sep);
    if (!p1)
        return -1;
    len = p1 - p;
    p1++;
    if (buf_size > 0) {
        if (len > buf_size - 1)
            len = buf_size - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
    }
    *pp = p1;
    return 0;
}

int parse_host_src_port(struct sockaddr_in *haddr,
                        struct sockaddr_in *saddr,
                        const char *input_str)
{
    char *str = strdup(input_str);
    char *host_str = str;
    char *src_str;
    char *ptr;

    /*
     * Chop off any extra arguments at the end of the string which
     * would start with a comma, then fill in the src port information
     * if it was provided else use the "any address" and "any port".
     */
    if ((ptr = strchr(str,',')))
        *ptr = '\0';

    if ((src_str = strchr(input_str,'@'))) {
        *src_str = '\0';
        src_str++;
    }

    if (parse_host_port(haddr, host_str) < 0)
        goto fail;

    if (!src_str || *src_str == '\0')
        src_str = ":0";

    if (parse_host_port(saddr, src_str) < 0)
        goto fail;

    free(str);
    return(0);

fail:
    free(str);
    return -1;
}

int parse_host_port(struct sockaddr_in *saddr, const char *str)
{
    char buf[512];
    struct hostent *he;
    const char *p, *r;
    int port;

    p = str;
    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0)
        return -1;
    saddr->sin_family = AF_INET;
    if (buf[0] == '\0') {
        saddr->sin_addr.s_addr = 0;
    } else {
        if (isdigit(buf[0])) {
            if (!inet_aton(buf, &saddr->sin_addr))
                return -1;
        } else {
            if ((he = gethostbyname(buf)) == NULL)
                return - 1;
            saddr->sin_addr = *(struct in_addr *)he->h_addr;
        }
    }
    port = strtol(p, (char **)&r, 0);
    if (r == p)
        return -1;
    saddr->sin_port = htons(port);
    return 0;
}

/* find or alloc a new VLAN */
VLANState *qemu_find_vlan(int id)
{
    VLANState **pvlan, *vlan;
    for(vlan = first_vlan; vlan != NULL; vlan = vlan->next) {
        if (vlan->id == id)
            return vlan;
    }
    vlan = qemu_mallocz(sizeof(VLANState));
    if (!vlan)
        return NULL;
    vlan->id = id;
    vlan->next = NULL;
    pvlan = &first_vlan;
    while (*pvlan != NULL)
        pvlan = &(*pvlan)->next;
    *pvlan = vlan;
    return vlan;
}

VLANClientState *qemu_new_vlan_client(VLANState *vlan,
                                      IOReadHandler *fd_read,
                                      IOCanRWHandler *fd_can_read,
                                      void *opaque)
{
    VLANClientState *vc, **pvc;
    vc = qemu_mallocz(sizeof(VLANClientState));
    if (!vc)
        return NULL;
    vc->fd_read = fd_read;
    vc->fd_can_read = fd_can_read;
    vc->opaque = opaque;
    vc->vlan = vlan;

    vc->tx_packets = 0;
    vc->rx_packets = 0;
    vc->tx_bytes   = 0;
    vc->rx_bytes   = 0;
    vc->tx_max_byte_rate = 0;
    vc->rx_max_byte_rate = 0;
    vc->tx_max_packet_rate = 0;
    vc->rx_max_packet_rate = 0;
    vc->clock_last = vc->clock_start = clock();

    vc->next = NULL;
    pvc = &vlan->first_client;
    while (*pvc != NULL)
        pvc = &(*pvc)->next;
    *pvc = vc;
    return vc;
}


//static void tap_receive(void *opaque, const uint8_t *buf, int size);
static void net_socket_receive_dgram(void *opaque, const uint8_t *buf, int size);
static void net_socket_receive(void *opaque, const uint8_t *buf, int size);
static void slirp_receive(void *opaque, const uint8_t *buf, int size);

#if 0
static char*
vlan_client_get_name( VLANClientState  *vc,
                      char*             tmp_name )
{
    char*  name = NULL;

    extern IOReadHandler  ne2000_receive;
    extern IOReadHandler  pcnet_receive;
    extern IOReadHandler  lance_receive;
    extern IOReadHandler  rtl8139_receive;
    extern IOReadHandler  smc91c111_receive;

    /* hackish, but should work */
    if (vc->fd_read == tap_receive)
        name = "tap";
    else if (vc->fd_read == net_socket_receive_dgram)
        name = "socket-dgram";
    else if (vc->fd_read == net_socket_receive)
        name = "socket";
    else if (vc->fd_read == slirp_receive) {
        name = "slirp";
        name = "upload";
    }
    else
    {
        sprintf( tmp_name, "<%p>", vc );
        name = tmp_name;
        name = "download";
    }
    return name;
}
#endif

static uint64_t           vc_clock_start;
static uint64_t           vc_clock_last;
static VLANClientState*   vc_upload;
static VLANClientState*   vc_download;

static void
vlan_dump_stats( void )
{
    long     upload_bytes     = 0;
    long     upload_packets   = 0;
    long     download_bytes   = 0;
    long     download_packets = 0;
    int      sec, msec, usec;
    double   nowf;

    uint64_t  now  = get_clock();
    int64_t   diff;

    if (vc_clock_start == 0) {
        vc_clock_start = now;
        vc_clock_last  = now;
    }

    diff  = now - vc_clock_last;

    if (vc_upload != NULL) {
        upload_bytes   = vc_upload->rx_bytes;
        upload_packets = vc_upload->rx_packets;

        vc_upload->rx_bytes   = 0;
        vc_upload->rx_packets = 0;
    }
    if (vc_download != NULL) {
        download_bytes   = vc_download->rx_bytes;
        download_packets = vc_download->rx_packets;

        vc_download->rx_bytes   = 0;
        vc_download->rx_packets = 0;
    }

    nowf  = (now - vc_clock_start)*(1.0/QEMU_TIMER_BASE);
    sec   = (int)nowf;
    nowf  = (nowf - sec)*1000;
    msec  = (int)nowf;
    nowf  = (nowf - msec)*1000;
    usec  = (int)nowf;
    if (usec >= 500) {
        msec += 1;
        if (msec >= 1000) {
            msec = 0;
            sec += 1;
        }
    }

    printf( "%03d.%03d: ", sec, msec );

    nowf = diff*(1.0/QEMU_TIMER_BASE);
    sec  = (int)nowf;
    nowf = (nowf - sec)*1e3;
    msec = (int)nowf;
    nowf = (nowf - msec)*1e3;
    usec = (int)nowf;
    if (usec >= 500) {
        msec += 1;
        if (msec >= 1000) {
            msec = 0;
            sec += 1;
        }
    }
    printf( "%03d.%03d: ", sec, msec );

    printf( "download: %6ld bytes, %4ld packets (%5.1f KB/s)",
            download_bytes, download_packets, diff > 0 ? (download_bytes*(QEMU_TIMER_BASE/1024.)/diff) : 0. );
    printf( " | " );
    printf( "upload:   %6ld bytes, %4ld packets (%5.1f KB/s)",
            upload_bytes, upload_packets, diff > 0 ? (upload_bytes*(QEMU_TIMER_BASE/1024.)/diff) : 0. );
    printf( "\n" );
    fflush(stdout);

    vc_clock_last = now;
}


static void
vlan_client_update_rx( VLANClientState  *vc,
                       unsigned long     bytes )
{
    static int  debug_vlan;

    if ( !vc_upload && vc->fd_read == slirp_receive )
        vc_upload = vc;

    if ( !vc_download && vc->fd_read != slirp_receive )
        vc_download = vc;

    vc->rx_bytes   += bytes;
    vc->rx_packets += 1;

   /* if the environment variable QEMU_NETWORK_STATS is defined and not 0,
    * we dump network statistics
    */
    if ( !debug_vlan )
    {
        const char*  env;
        debug_vlan = 1;
        env = getenv( "QEMU_NETWORK_STATS" );
        if (env != NULL && env[0] != 0)
        {
            int  val = atoi(env);

            if (val != 0)
                debug_vlan = 2;
        }
    }

    if (debug_vlan > 1)
    {
        uint64_t   now    = get_clock();

        if (now - vc_clock_last > (QEMU_TIMER_BASE*0.25) || vc->rx_packets > 100) {
#if 1
            vlan_dump_stats();
#else
            clock_t period      = now - vc->clock_start;
            double  byte_rate   = vc->rx_bytes*1e6/period;
            double  packet_rate = vc->rx_packets*1e6/period;
            char    tmp[14];

            if ( byte_rate > vc->rx_max_byte_rate )
                vc->rx_max_byte_rate = byte_rate;

            if ( packet_rate > vc->rx_max_packet_rate )
                vc->rx_max_packet_rate = packet_rate;

            if (debug_vlan > 1)
                fprintf( stderr, ">>>> %14s  %6.1f KB/s  %6.1f packet/s  (max %6.1f KB/s, bytes %ld, packets %ld)\n",
                                 vlan_client_get_name(vc, tmp),
                                 byte_rate/1024., packet_rate, vc->rx_max_byte_rate/1024.,
                                 vc->rx_bytes, vc->rx_packets );
            if (vc->rx_packets > 100 || now - vc->clock_start > (CLOCKS_PER_SEC*0.5))
            {
                vc->rx_packets  = 0;
                vc->rx_bytes    = 0;
                vc->clock_start = now;
            }
            vc->clock_last = now;
#endif
        }
    }
}


int qemu_can_send_packet(VLANClientState *vc1)
{
    VLANState *vlan = vc1->vlan;
    VLANClientState *vc;

    for(vc = vlan->first_client; vc != NULL; vc = vc->next) {
        if (vc != vc1) {
            if (vc->fd_can_read && !vc->fd_can_read(vc->opaque))
                return 0;
        }
    }
    return 1;
}

void qemu_send_packet(VLANClientState *vc1, const uint8_t *buf, int size)
{
    VLANState *vlan = vc1->vlan;
    VLANClientState *vc;

#if 0
    printf("vlan %d send:\n", vlan->id);
    hex_dump(stdout, buf, size);
#endif
    for(vc = vlan->first_client; vc != NULL; vc = vc->next) {
        if (vc != vc1) {
            vc->fd_read(vc->opaque, buf, size);
            vlan_client_update_rx( vc, size );
        }
    }
}

#if defined(CONFIG_SLIRP)

/* slirp network adapter */

int slirp_inited;
static VLANClientState *slirp_vc;

double   qemu_net_upload_speed   = 0.;
double   qemu_net_download_speed = 0.;
int      qemu_net_min_latency = 0;
int      qemu_net_max_latency = 0;
int      qemu_net_disable = 0;

int
ip_packet_is_internal( const uint8_t*  data, size_t  size )
{
    const uint8_t*  end = data + size;

    /* must have room for Mac + IP header */
    if (data + 40 > end)
        return 0;

    if (data[12] != 0x08 || data[13] != 0x00 )
        return 0;

    /* must have valid IP header */
    data += 14;
    if ((data[0] >> 4) != 4 || (data[0] & 15) < 5)
        return 0;

    /* internal if both source and dest addresses are in 10.x.x.x */
    return ( data[12] == 10 && data[16] == 10);
}

#ifdef CONFIG_SHAPER

/* see http://en.wikipedia.org/wiki/List_of_device_bandwidths or a complete list */
const NetworkSpeed  android_netspeeds[] = {
    { "gsm", "GSM/CSD", 14400, 14400 },
    { "hscsd", "HSCSD", 14400, 43200 },
    { "gprs", "GPRS", 40000, 80000 },
    { "edge", "EDGE/EGPRS", 118400, 236800 },
    { "umts", "UMTS/3G", 128000, 1920000 },
    { "hsdpa", "HSDPA", 348000, 14400000 },
    { "full", "no limit", 0, 0 },
    { NULL, NULL, 0, 0 }
};

const NetworkLatency  android_netdelays[] = {
    /* FIXME: these numbers are totally imaginary */
    { "gprs", "GPRS", 150, 550 },
    { "edge", "EDGE/EGPRS", 80, 400 },
    { "umts", "UMTS/3G", 35, 200 },
    { "none", "no latency", 0, 0 },
    { NULL, NULL, 0, 0 }
};


NetShaper  slirp_shaper_in;
NetShaper  slirp_shaper_out;
NetDelay   slirp_delay_in;

static void
slirp_delay_in_cb( void*   data,
                   size_t  size,
                   void*   opaque )
{
    slirp_input( (const uint8_t*)data, (int)size );
    opaque = opaque;
}

static void
slirp_shaper_in_cb( void*   data,
                    size_t  size,
                    void*   opaque )
{
    netdelay_send_aux( slirp_delay_in, data, size, opaque );
}

static void
slirp_shaper_out_cb( void*   data,
                     size_t  size,
                     void*   opaque )
{
    qemu_send_packet( slirp_vc, (const uint8_t*)data, (int)size );
}

void
slirp_init_shapers( void )
{
    slirp_delay_in   = netdelay_create( slirp_delay_in_cb );
    slirp_shaper_in  = netshaper_create( 1, slirp_shaper_in_cb );
    slirp_shaper_out = netshaper_create( 1, slirp_shaper_out_cb );

    netdelay_set_latency( slirp_delay_in, qemu_net_min_latency, qemu_net_max_latency );
    netshaper_set_rate( slirp_shaper_out, qemu_net_download_speed );
    netshaper_set_rate( slirp_shaper_in,  qemu_net_upload_speed  );
}

#endif /* CONFIG_SHAPER */

int slirp_can_output(void)
{
#ifdef CONFIG_SHAPER
    return !slirp_vc                                ||
           ( netshaper_can_send( slirp_shaper_out ) &&
             qemu_can_send_packet(slirp_vc) );
#else
    return !slirp_vc || qemu_can_send_packet(slirp_vc);
#endif
}



void slirp_output(const uint8_t *pkt, int pkt_len)
{
#if 0
    printf("slirp output:\n");
    hex_dump(stdout, pkt, pkt_len);
#endif
    if (!slirp_vc)
        return;

    /* always send internal packets */
    if ( ip_packet_is_internal( pkt, pkt_len ) ) {
        qemu_send_packet( slirp_vc, pkt, pkt_len );
        return;
    }

    if ( qemu_net_disable )
        return;

#ifdef CONFIG_SHAPER
    netshaper_send( slirp_shaper_out, (void*)pkt, pkt_len );
#else
    qemu_send_packet(slirp_vc, pkt, pkt_len);
#endif
}

static void slirp_receive(void *opaque, const uint8_t *buf, int size)
{
#if 0
    printf("slirp input:\n");
    hex_dump(stdout, buf, size);
#endif
    if ( ip_packet_is_internal( buf, size ) ) {
        slirp_input(buf, size);
        return;
    }

    if (qemu_net_disable)
        return;

#ifdef CONFIG_SHAPER
    netshaper_send( slirp_shaper_in, (char*)buf, size );
#else
    slirp_input(buf, size);
#endif
}

static int net_slirp_init(VLANState *vlan)
{
    if (!slirp_inited) {
        slirp_inited = 1;
        slirp_init();
    }
    slirp_vc = qemu_new_vlan_client(vlan,
                                    slirp_receive, NULL, NULL);
    snprintf(slirp_vc->info_str, sizeof(slirp_vc->info_str), "user redirector");
    return 0;
}

static int net_slirp_redir(const char *redir_str)
{
    int is_udp;
    char buf[256], *r;
    const char *p;
    struct in_addr guest_addr;
    int host_port, guest_port;

    if (!slirp_inited) {
        slirp_inited = 1;
        slirp_init();
    }

    p = redir_str;
    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0)
        goto fail;
    if (!strcmp(buf, "tcp")) {
        is_udp = 0;
    } else if (!strcmp(buf, "udp")) {
        is_udp = 1;
    } else {
        goto fail;
    }

    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0)
        goto fail;
    host_port = strtol(buf, &r, 0);
    if (r == buf)
        goto fail;

    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0)
        goto fail;
    if (buf[0] == '\0') {
        pstrcpy(buf, sizeof(buf), "10.0.2.15");
    }
    if (!inet_aton(buf, &guest_addr))
        goto fail;

    guest_port = strtol(p, &r, 0);
    if (r == p)
        goto fail;

    if (slirp_redir(is_udp, host_port, guest_addr, guest_port) < 0) {
        return -1;
    }

    return 0;
 fail:
    fprintf(stderr, "qemu: syntax: -redir [tcp|udp]:host-port:[guest-host]:guest-port\n");
    exit(1);
    return -1;
}

#if 0  /* ANDROID disabled */

char smb_dir[1024];

static void smb_exit(void)
{
    DIR *d;
    struct dirent *de;
    char filename[1024];

    /* erase all the files in the directory */
    d = opendir(smb_dir);
    for(;;) {
        de = readdir(d);
        if (!de)
            break;
        if (strcmp(de->d_name, ".") != 0 &&
            strcmp(de->d_name, "..") != 0) {
            snprintf(filename, sizeof(filename), "%s/%s",
                     smb_dir, de->d_name);
            unlink(filename);
        }
    }
    closedir(d);
    rmdir(smb_dir);
}

/* automatic user mode samba server configuration */
void net_slirp_smb(const char *exported_dir)
{
    char smb_conf[1024];
    char smb_cmdline[1024];
    FILE *f;

    if (!slirp_inited) {
        slirp_inited = 1;
        slirp_init();
    }

    /* XXX: better tmp dir construction */
    snprintf(smb_dir, sizeof(smb_dir), "/tmp/qemu-smb.%d", getpid());
    if (mkdir(smb_dir, 0700) < 0) {
        fprintf(stderr, "qemu: could not create samba server dir '%s'\n", smb_dir);
        exit(1);
    }
    snprintf(smb_conf, sizeof(smb_conf), "%s/%s", smb_dir, "smb.conf");

    f = fopen(smb_conf, "w");
    if (!f) {
        fprintf(stderr, "qemu: could not create samba server configuration file '%s'\n", smb_conf);
        exit(1);
    }
    fprintf(f,
            "[global]\n"
            "private dir=%s\n"
            "smb ports=0\n"
            "socket address=127.0.0.1\n"
            "pid directory=%s\n"
            "lock directory=%s\n"
            "log file=%s/log.smbd\n"
            "smb passwd file=%s/smbpasswd\n"
            "security = share\n"
            "[qemu]\n"
            "path=%s\n"
            "read only=no\n"
            "guest ok=yes\n",
            smb_dir,
            smb_dir,
            smb_dir,
            smb_dir,
            smb_dir,
            exported_dir
            );
    fclose(f);
    atexit(smb_exit);

    snprintf(smb_cmdline, sizeof(smb_cmdline), "/usr/sbin/smbd -s %s",
             smb_conf);

    slirp_add_exec(0, smb_cmdline, 4, 139);
}

#endif /* !defined(_WIN32) */

#endif /* CONFIG_SLIRP */

#if !defined(_WIN32)

typedef struct TAPState {
    VLANClientState *vc;
    int fd;
} TAPState;

static void tap_receive(void *opaque, const uint8_t *buf, int size)
{
    TAPState *s = opaque;
    int ret;
    for(;;) {
        ret = write(s->fd, buf, size);
        if (ret < 0 && (errno == EINTR || errno == EAGAIN)) {
        } else {
            break;
        }
    }
}

static void tap_send(void *opaque)
{
    TAPState *s = opaque;
    uint8_t buf[4096];
    int size;

    size = read(s->fd, buf, sizeof(buf));
    if (size > 0) {
        qemu_send_packet(s->vc, buf, size);
    }
}

/* fd support */

static TAPState *net_tap_fd_init(VLANState *vlan, int fd)
{
    TAPState *s;

    s = qemu_mallocz(sizeof(TAPState));
    if (!s)
        return NULL;
    s->fd = fd;
    s->vc = qemu_new_vlan_client(vlan, tap_receive, NULL, s);
    qemu_set_fd_handler(s->fd, tap_send, NULL, s);
    snprintf(s->vc->info_str, sizeof(s->vc->info_str), "tap: fd=%d", fd);
    return s;
}

#ifdef _BSD
static int tap_open(char *ifname, int ifname_size)
{
    int fd;
    char *dev;
    struct stat s;

    fd = open("/dev/tap", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "warning: could not open /dev/tap: no virtual network emulation\n");
        return -1;
    }

    fstat(fd, &s);
    dev = devname(s.st_rdev, S_IFCHR);
    pstrcpy(ifname, ifname_size, dev);

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}
#elif defined(__sun__)
static int tap_open(char *ifname, int ifname_size)
{
    fprintf(stderr, "warning: tap_open not yet implemented\n");
    return -1;
}
#else
static int tap_open(char *ifname, int ifname_size)
{
    struct ifreq ifr;
    int fd, ret;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "warning: could not open /dev/net/tun: no virtual network emulation\n");
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (ifname[0] != '\0')
        pstrcpy(ifr.ifr_name, IFNAMSIZ, ifname);
    else
        pstrcpy(ifr.ifr_name, IFNAMSIZ, "tap%d");
    ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (ret != 0) {
        fprintf(stderr, "warning: could not configure /dev/net/tun: no virtual network emulation\n");
        close(fd);
        return -1;
    }
    pstrcpy(ifname, ifname_size, ifr.ifr_name);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}
#endif

static int net_tap_init(VLANState *vlan, const char *ifname1,
                        const char *setup_script)
{
    TAPState *s;
    int pid, status, fd;
    char *args[3];
    char **parg;
    char ifname[128];

    if (ifname1 != NULL)
        pstrcpy(ifname, sizeof(ifname), ifname1);
    else
        ifname[0] = '\0';
    fd = tap_open(ifname, sizeof(ifname));
    if (fd < 0)
        return -1;

    if (!setup_script)
        setup_script = "";
    if (setup_script[0] != '\0') {
        /* try to launch network init script */
        pid = fork();
        if (pid >= 0) {
            if (pid == 0) {
                parg = args;
                *parg++ = (char *)setup_script;
                *parg++ = ifname;
                *parg++ = NULL;
                execv(setup_script, args);
                _exit(1);
            }
            while (waitpid(pid, &status, 0) != pid);
            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {
                fprintf(stderr, "%s: could not launch network script\n",
                        setup_script);
                return -1;
            }
        }
    }
    s = net_tap_fd_init(vlan, fd);
    if (!s)
        return -1;
    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
             "tap: ifname=%s setup_script=%s", ifname, setup_script);
    return 0;
}

#endif /* !_WIN32 */

/* network connection */
typedef struct NetSocketState {
    VLANClientState *vc;
    int fd;
    int state; /* 0 = getting length, 1 = getting data */
    int index;
    int packet_len;
    uint8_t buf[4096];
    struct sockaddr_in dgram_dst; /* contains inet host and port destination iff connectionless (SOCK_DGRAM) */
} NetSocketState;

typedef struct NetSocketListenState {
    VLANState *vlan;
    int fd;
} NetSocketListenState;

/* XXX: we consider we can send the whole packet without blocking */
static void net_socket_receive(void *opaque, const uint8_t *buf, int size)
{
    NetSocketState *s = opaque;
    uint32_t len;
    len = htonl(size);

    send_all(s->fd, (const uint8_t *)&len, sizeof(len));
    send_all(s->fd, buf, size);
}

static void net_socket_receive_dgram(void *opaque, const uint8_t *buf, int size)
{
    NetSocketState *s = opaque;
    sendto(s->fd, buf, size, 0,
           (struct sockaddr *)&s->dgram_dst, sizeof(s->dgram_dst));
}

static void net_socket_send(void *opaque)
{
    NetSocketState *s = opaque;
    int l, size, err;
    uint8_t buf1[4096];
    const uint8_t *buf;

    size = recv(s->fd, buf1, sizeof(buf1), 0);
    if (size < 0) {
        err = socket_errno;
        if (err != EWOULDBLOCK)
            goto eoc;
    } else if (size == 0) {
        /* end of connection */
    eoc:
        qemu_set_fd_handler(s->fd, NULL, NULL, NULL);
        socket_close(s->fd);
        return;
    }
    buf = buf1;
    while (size > 0) {
        /* reassemble a packet from the network */
        switch(s->state) {
        case 0:
            l = 4 - s->index;
            if (l > size)
                l = size;
            memcpy(s->buf + s->index, buf, l);
            buf += l;
            size -= l;
            s->index += l;
            if (s->index == 4) {
                /* got length */
                s->packet_len = ntohl(*(uint32_t *)s->buf);
                s->index = 0;
                s->state = 1;
            }
            break;
        case 1:
            l = s->packet_len - s->index;
            if (l > size)
                l = size;
            memcpy(s->buf + s->index, buf, l);
            s->index += l;
            buf += l;
            size -= l;
            if (s->index >= s->packet_len) {
                qemu_send_packet(s->vc, s->buf, s->packet_len);
                s->index = 0;
                s->state = 0;
            }
            break;
        }
    }
}

static void net_socket_send_dgram(void *opaque)
{
    NetSocketState *s = opaque;
    int size;

    size = recv(s->fd, s->buf, sizeof(s->buf), 0);
    if (size < 0)
        return;
    if (size == 0) {
        /* end of connection */
        qemu_set_fd_handler(s->fd, NULL, NULL, NULL);
        return;
    }
    qemu_send_packet(s->vc, s->buf, size);
}

static int net_socket_mcast_create(struct sockaddr_in *mcastaddr)
{
    struct ip_mreq imr;
    int fd;
    int val, ret;
    if (!IN_MULTICAST(ntohl(mcastaddr->sin_addr.s_addr))) {
	fprintf(stderr, "qemu: error: specified mcastaddr \"%s\" (0x%08x) does not contain a multicast address\n",
		inet_ntoa(mcastaddr->sin_addr),
                (int)ntohl(mcastaddr->sin_addr.s_addr));
	return -1;

    }
    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(PF_INET, SOCK_DGRAM)");
        return -1;
    }

    ret=socket_set_xreuseaddr(fd);
    if (ret < 0) {
	perror("setsockopt(SOL_SOCKET, SO_REUSEADDR)");
	goto fail;
    }

    ret = bind(fd, (struct sockaddr *)mcastaddr, sizeof(*mcastaddr));
    if (ret < 0) {
        perror("bind");
        goto fail;
    }

    /* Add host to multicast group */
    imr.imr_multiaddr = mcastaddr->sin_addr;
    imr.imr_interface.s_addr = htonl(INADDR_ANY);

    ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     (const char *)&imr, sizeof(struct ip_mreq));
    if (ret < 0) {
	perror("setsockopt(IP_ADD_MEMBERSHIP)");
	goto fail;
    }

    /* Force mcast msgs to loopback (eg. several QEMUs in same host */
    val = 1;
    ret=setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
                   (const char *)&val, sizeof(val));
    if (ret < 0) {
	perror("setsockopt(SOL_IP, IP_MULTICAST_LOOP)");
	goto fail;
    }

    socket_set_nonblock(fd);
    return fd;
fail:
    if (fd >= 0)
        socket_close(fd);
    return -1;
}

static NetSocketState *net_socket_fd_init_dgram(VLANState *vlan, int fd,
                                          int is_connected)
{
    struct sockaddr_in saddr;
    int newfd;
    socklen_t saddr_len;
    NetSocketState *s;

    /* fd passed: multicast: "learn" dgram_dst address from bound address and save it
     * Because this may be "shared" socket from a "master" process, datagrams would be recv()
     * by ONLY ONE process: we must "clone" this dgram socket --jjo
     */

    if (is_connected) {
	if (getsockname(fd, (struct sockaddr *) &saddr, &saddr_len) == 0) {
	    /* must be bound */
	    if (saddr.sin_addr.s_addr==0) {
		fprintf(stderr, "qemu: error: init_dgram: fd=%d unbound, cannot setup multicast dst addr\n",
			fd);
		return NULL;
	    }
	    /* clone dgram socket */
	    newfd = net_socket_mcast_create(&saddr);
	    if (newfd < 0) {
		/* error already reported by net_socket_mcast_create() */
		close(fd);
		return NULL;
	    }
	    /* clone newfd to fd, close newfd */
	    dup2(newfd, fd);
	    close(newfd);

	} else {
	    fprintf(stderr, "qemu: error: init_dgram: fd=%d failed getsockname(): %s\n",
		    fd, strerror(errno));
	    return NULL;
	}
    }

    s = qemu_mallocz(sizeof(NetSocketState));
    if (!s)
        return NULL;
    s->fd = fd;

    s->vc = qemu_new_vlan_client(vlan, net_socket_receive_dgram, NULL, s);
    qemu_set_fd_handler(s->fd, net_socket_send_dgram, NULL, s);

    /* mcast: save bound address as dst */
    if (is_connected) s->dgram_dst=saddr;

    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
	    "socket: fd=%d (%s mcast=%s:%d)",
	    fd, is_connected? "cloned" : "",
	    inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    return s;
}

static void net_socket_connect(void *opaque)
{
    NetSocketState *s = opaque;
    qemu_set_fd_handler(s->fd, net_socket_send, NULL, s);
}

static NetSocketState *net_socket_fd_init_stream(VLANState *vlan, int fd,
                                          int is_connected)
{
    NetSocketState *s;
    s = qemu_mallocz(sizeof(NetSocketState));
    if (!s)
        return NULL;
    s->fd = fd;
    s->vc = qemu_new_vlan_client(vlan,
                                 net_socket_receive, NULL, s);
    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
             "socket: fd=%d", fd);
    if (is_connected) {
        net_socket_connect(s);
    } else {
        qemu_set_fd_handler(s->fd, NULL, net_socket_connect, s);
    }
    return s;
}

static NetSocketState *net_socket_fd_init(VLANState *vlan, int fd,
                                          int is_connected)
{
    int so_type=-1, optlen=sizeof(so_type);

    if(getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *)&so_type, (unsigned*)&optlen)< 0) {
	fprintf(stderr, "qemu: error: setsockopt(SO_TYPE) for fd=%d failed\n", fd);
	return NULL;
    }
    switch(so_type) {
    case SOCK_DGRAM:
        return net_socket_fd_init_dgram(vlan, fd, is_connected);
    case SOCK_STREAM:
        return net_socket_fd_init_stream(vlan, fd, is_connected);
    default:
        /* who knows ... this could be a eg. a pty, do warn and continue as stream */
        fprintf(stderr, "qemu: warning: socket type=%d for fd=%d is not SOCK_DGRAM or SOCK_STREAM\n", so_type, fd);
        return net_socket_fd_init_stream(vlan, fd, is_connected);
    }
    return NULL;
}

static void net_socket_accept(void *opaque)
{
    NetSocketListenState *s = opaque;
    NetSocketState *s1;
    struct sockaddr_in saddr;
    socklen_t len;
    int fd;

    for(;;) {
        len = sizeof(saddr);
        fd = accept(s->fd, (struct sockaddr *)&saddr, &len);
        if (fd < 0 && errno != EINTR) {
            return;
        } else if (fd >= 0) {
            break;
        }
    }
    s1 = net_socket_fd_init(s->vlan, fd, 1);
    if (!s1) {
        socket_close(fd);
    } else {
        snprintf(s1->vc->info_str, sizeof(s1->vc->info_str),
                 "socket: connection from %s:%d",
                 inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    }
}

static int net_socket_listen_init(VLANState *vlan, const char *host_str)
{
    NetSocketListenState *s;
    int fd, ret;
    struct sockaddr_in saddr;

    if (parse_host_port(&saddr, host_str) < 0)
        return -1;

    s = qemu_mallocz(sizeof(NetSocketListenState));
    if (!s)
        return -1;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    socket_set_nonblock(fd);

    socket_set_xreuseaddr(fd);

    ret = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }
    ret = listen(fd, 0);
    if (ret < 0) {
        perror("listen");
        return -1;
    }
    s->vlan = vlan;
    s->fd = fd;
    qemu_set_fd_handler(fd, net_socket_accept, NULL, s);
    return 0;
}

static int net_socket_connect_init(VLANState *vlan, const char *host_str)
{
    NetSocketState *s;
    int fd, connected, ret, err;
    struct sockaddr_in saddr;

    if (parse_host_port(&saddr, host_str) < 0)
        return -1;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    socket_set_nonblock(fd);

    connected = 0;
    for(;;) {
        ret = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
        if (ret < 0) {
            err = socket_errno;
            if (err == EINTR || err == EWOULDBLOCK) {
            } else if (err == EINPROGRESS) {
                break;
            } else {
                perror("connect");
                socket_close(fd);
                return -1;
            }
        } else {
            connected = 1;
            break;
        }
    }
    s = net_socket_fd_init(vlan, fd, connected);
    if (!s)
        return -1;
    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
             "socket: connect to %s:%d",
             inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    return 0;
}

static int net_socket_mcast_init(VLANState *vlan, const char *host_str)
{
    NetSocketState *s;
    int fd;
    struct sockaddr_in saddr;

    if (parse_host_port(&saddr, host_str) < 0)
        return -1;


    fd = net_socket_mcast_create(&saddr);
    if (fd < 0)
	return -1;

    s = net_socket_fd_init(vlan, fd, 0);
    if (!s)
        return -1;

    s->dgram_dst = saddr;

    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
             "socket: mcast=%s:%d",
             inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    return 0;

}

static int get_param_value(char *buf, int buf_size,
                           const char *tag, const char *str)
{
    const char *p;
    char *q;
    char option[128];

    p = str;
    for(;;) {
        q = option;
        while (*p != '\0' && *p != '=') {
            if ((q - option) < sizeof(option) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        if (*p != '=')
            break;
        p++;
        if (!strcmp(tag, option)) {
            q = buf;
            while (*p != '\0' && *p != ',') {
                if ((q - buf) < buf_size - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            return q - buf;
        } else {
            while (*p != '\0' && *p != ',') {
                p++;
            }
        }
        if (*p != ',')
            break;
        p++;
    }
    return 0;
}

int net_client_init(const char *str)
{
    const char *p;
    char *q;
    char device[64];
    char buf[1024];
    int vlan_id, ret;
    VLANState *vlan;

    p = str;
    q = device;
    while (*p != '\0' && *p != ',') {
        if ((q - device) < sizeof(device) - 1)
            *q++ = *p;
        p++;
    }
    *q = '\0';
    if (*p == ',')
        p++;
    vlan_id = 0;
    if (get_param_value(buf, sizeof(buf), "vlan", p)) {
        vlan_id = strtol(buf, NULL, 0);
    }
    vlan = qemu_find_vlan(vlan_id);
    if (!vlan) {
        fprintf(stderr, "Could not create vlan %d\n", vlan_id);
        return -1;
    }
    if (!strcmp(device, "nic")) {
        NICInfo *nd;
        uint8_t *macaddr;

        if (nb_nics >= MAX_NICS) {
            fprintf(stderr, "Too Many NICs\n");
            return -1;
        }
        nd = &nd_table[nb_nics];
        macaddr = nd->macaddr;
        macaddr[0] = 0x52;
        macaddr[1] = 0x54;
        macaddr[2] = 0x00;
        macaddr[3] = 0x12;
        macaddr[4] = 0x34;
        macaddr[5] = 0x56 + nb_nics;

        if (get_param_value(buf, sizeof(buf), "macaddr", p)) {
            if (parse_macaddr(macaddr, buf) < 0) {
                fprintf(stderr, "invalid syntax for ethernet address\n");
                return -1;
            }
        }
        if (get_param_value(buf, sizeof(buf), "model", p)) {
            nd->model = strdup(buf);
        }
        nd->vlan = vlan;
        nb_nics++;
        ret = 0;
    } else
    if (!strcmp(device, "none")) {
        /* does nothing. It is needed to signal that no network cards
           are wanted */
#if 1 /* ANDROID */
        fprintf(stderr, "sorry, you need to enable the network to use the Android emulator\n");
        return -1;
#else
        ret = 0;
#endif
    } else
#ifdef CONFIG_SLIRP
    if (!strcmp(device, "user")) {
        if (get_param_value(buf, sizeof(buf), "hostname", p)) {
            pstrcpy(slirp_hostname, sizeof(slirp_hostname), buf);
        }
        ret = net_slirp_init(vlan);
    } else
#endif
#ifdef _WIN32
    if (!strcmp(device, "tap")) {
        char ifname[64];
        if (get_param_value(ifname, sizeof(ifname), "ifname", p) <= 0) {
            fprintf(stderr, "tap: no interface name\n");
            return -1;
        }
        ret = tap_win32_init(vlan, ifname);
    } else
#else
    if (!strcmp(device, "tap")) {
        char ifname[64];
        char setup_script[1024];
        int fd;
        if (get_param_value(buf, sizeof(buf), "fd", p) > 0) {
            fd = strtol(buf, NULL, 0);
            ret = -1;
            if (net_tap_fd_init(vlan, fd))
                ret = 0;
        } else {
            get_param_value(ifname, sizeof(ifname), "ifname", p);
            if (get_param_value(setup_script, sizeof(setup_script), "script", p) == 0) {
                pstrcpy(setup_script, sizeof(setup_script), DEFAULT_NETWORK_SCRIPT);
            }
            ret = net_tap_init(vlan, ifname, setup_script);
        }
    } else
#endif
    if (!strcmp(device, "socket")) {
        if (get_param_value(buf, sizeof(buf), "fd", p) > 0) {
            int fd;
            fd = strtol(buf, NULL, 0);
            ret = -1;
            if (net_socket_fd_init(vlan, fd, 1))
                ret = 0;
        } else if (get_param_value(buf, sizeof(buf), "listen", p) > 0) {
            ret = net_socket_listen_init(vlan, buf);
        } else if (get_param_value(buf, sizeof(buf), "connect", p) > 0) {
            ret = net_socket_connect_init(vlan, buf);
        } else if (get_param_value(buf, sizeof(buf), "mcast", p) > 0) {
            ret = net_socket_mcast_init(vlan, buf);
        } else {
            fprintf(stderr, "Unknown socket options: %s\n", p);
            return -1;
        }
    } else
    {
        fprintf(stderr, "Unknown network device: %s\n", device);
        return -1;
    }
    if (ret < 0) {
        fprintf(stderr, "Could not initialize device '%s'\n", device);
    }

    return ret;
}

void do_info_network(void)
{
    VLANState *vlan;
    VLANClientState *vc;

    for(vlan = first_vlan; vlan != NULL; vlan = vlan->next) {
        term_printf("VLAN %d devices:\n", vlan->id);
        for(vc = vlan->first_client; vc != NULL; vc = vc->next)
            term_printf("  %s\n", vc->info_str);
    }
}

/***********************************************************/
/* USB devices */

static USBPort *used_usb_ports;
static USBPort *free_usb_ports;

/* ??? Maybe change this to register a hub to keep track of the topology.  */
void qemu_register_usb_port(USBPort *port, void *opaque, int index,
                            usb_attachfn attach)
{
    port->opaque = opaque;
    port->index = index;
    port->attach = attach;
    port->next = free_usb_ports;
    free_usb_ports = port;
}

static int usb_device_add(const char *devname)
{
    const char *p;
    USBDevice *dev;
    USBPort *port;

    if (!free_usb_ports)
        return -1;

    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(p);
    } else if (!strcmp(devname, "mouse")) {
        dev = usb_mouse_init();
    } else if (!strcmp(devname, "tablet")) {
	dev = usb_tablet_init();
    } else if (strstart(devname, "disk:", &p)) {
        dev = usb_msd_init(p);
    } else {
        return -1;
    }
    if (!dev)
        return -1;

    /* Find a USB port to add the device to.  */
    port = free_usb_ports;
    if (!port->next) {
        USBDevice *hub;

        /* Create a new hub and chain it on.  */
        free_usb_ports = NULL;
        port->next = used_usb_ports;
        used_usb_ports = port;

        hub = usb_hub_init(VM_USB_HUB_SIZE);
        usb_attach(port, hub);
        port = free_usb_ports;
    }

    free_usb_ports = port->next;
    port->next = used_usb_ports;
    used_usb_ports = port;
    usb_attach(port, dev);
    return 0;
}

static int usb_device_del(const char *devname)
{
    USBPort *port;
    USBPort **lastp;
    int bus_num, addr;
    const char *p;

    if (!used_usb_ports)
        return -1;

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);
    if (bus_num != 0)
        return -1;

    lastp = &used_usb_ports;
    port = used_usb_ports;
    while (port && port->dev->addr != addr) {
        lastp = &port->next;
        port = port->next;
    }

    if (!port)
        return -1;

    *lastp = port->next;
    usb_attach(port, NULL);
    port->next = free_usb_ports;
    free_usb_ports = port;
    return 0;
}

void do_usb_add(const char *devname)
{
    int ret;
    ret = usb_device_add(devname);
    if (ret < 0)
        term_printf("Could not add USB device '%s'\n", devname);
}

void do_usb_del(const char *devname)
{
    int ret;
    ret = usb_device_del(devname);
    if (ret < 0)
        term_printf("Could not remove USB device '%s'\n", devname);
}

void usb_info(void)
{
    USBDevice *dev;
    USBPort *port;
    const char *speed_str;

    if (!usb_enabled) {
        term_printf("USB support not enabled\n");
        return;
    }

    for (port = used_usb_ports; port; port = port->next) {
        dev = port->dev;
        if (!dev)
            continue;
        switch(dev->speed) {
        case USB_SPEED_LOW:
            speed_str = "1.5";
            break;
        case USB_SPEED_FULL:
            speed_str = "12";
            break;
        case USB_SPEED_HIGH:
            speed_str = "480";
            break;
        default:
            speed_str = "?";
            break;
        }
        term_printf("  Device %d.%d, Speed %s Mb/s, Product %s\n",
                    0, dev->addr, speed_str, dev->devname);
    }
}

/***********************************************************/
/* pid file */

static char *pid_filename;

/* Remove PID file. Called on normal exit */

static void remove_pidfile(void)
{
    unlink (pid_filename);
}

static void create_pidfile(const char *filename)
{
    struct stat pidstat;
    FILE *f;

    /* Try to write our PID to the named file */
    if (stat(filename, &pidstat) < 0) {
        if (errno == ENOENT) {
            if ((f = fopen (filename, "w")) == NULL) {
                perror("Opening pidfile");
                exit(1);
            }
            fprintf(f, "%d\n", getpid());
            fclose(f);
            pid_filename = qemu_strdup(filename);
            if (!pid_filename) {
                fprintf(stderr, "Could not save PID filename");
                exit(1);
            }
            atexit(remove_pidfile);
        }
    } else {
        fprintf(stderr, "%s already exists. Remove it and try again.\n",
                filename);
        exit(1);
    }
}

/***********************************************************/
/* dumb display */

static void dumb_update(DisplayState *ds, int x, int y, int w, int h)
{
}

static void dumb_resize(DisplayState *ds, int w, int h, int rotation)
{
}

static void dumb_refresh(DisplayState *ds)
{
    vga_hw_update();
}

void dumb_display_init(DisplayState *ds)
{
    ds->data = NULL;
    ds->linesize = 0;
    ds->depth = 0;
    ds->dpy_update = dumb_update;
    ds->dpy_resize = dumb_resize;
    ds->dpy_refresh = dumb_refresh;
}

/***********************************************************/
/* I/O handling */

#define MAX_IO_HANDLERS 64

typedef struct IOHandlerRecord {
    int fd;
    IOCanRWHandler *fd_read_poll;
    IOHandler *fd_read;
    IOHandler *fd_write;
    void *opaque;
    unsigned state;
    /* temporary data */
    struct pollfd *ufd;
    struct IOHandlerRecord *next;
} IOHandlerRecord;

typedef enum {
    IOH_STATE_PENDING = (1 << 0),
    IOH_STATE_CLOSED  = (1 << 1)
} IOHandlerState;

static IOHandlerRecord *first_io_handler;

/* XXX: fd_read_poll should be suppressed, but an API change is
   necessary in the character devices to suppress fd_can_read(). */
int qemu_set_fd_handler2(int fd,
                         IOCanRWHandler *fd_read_poll,
                         IOHandler *fd_read,
                         IOHandler *fd_write,
                         void *opaque)
{
    IOHandlerRecord **pioh, *ioh;

    if (!fd_read && !fd_write) {
        pioh = &first_io_handler;
        for(;;) {
            ioh = *pioh;
            if (ioh == NULL)
                break;
            if (ioh->fd == fd) {
                *pioh = ioh->next;

                if (ioh->state == IOH_STATE_PENDING) {
                    /* do not delete this ioh immediately if we're currently
                     * processing it.
                     */
                    ioh->state = IOH_STATE_CLOSED;
                } else {
                    qemu_free(ioh);
                }
                break;
            }
            pioh = &ioh->next;
        }
    } else {
        for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
            if (ioh->fd == fd)
                goto found;
        }
        ioh = qemu_mallocz(sizeof(IOHandlerRecord));
        if (!ioh)
            return -1;
        ioh->next = first_io_handler;
        first_io_handler = ioh;
    found:
        ioh->fd = fd;
        ioh->fd_read_poll = fd_read_poll;
        ioh->fd_read = fd_read;
        ioh->fd_write = fd_write;
        ioh->opaque = opaque;
    }
    return 0;
}

int qemu_set_fd_handler(int fd,
                        IOHandler *fd_read,
                        IOHandler *fd_write,
                        void *opaque)
{
    return qemu_set_fd_handler2(fd, NULL, fd_read, fd_write, opaque);
}

/***********************************************************/
/* Polling handling */

typedef struct PollingEntry {
    PollingFunc*          func;
    void*                 opaque;
    struct PollingEntry*  next;
} PollingEntry;

static PollingEntry *first_polling_entry;

int qemu_add_polling_cb(PollingFunc *func, void *opaque)
{
    PollingEntry **ppe, *pe;
    pe = qemu_mallocz(sizeof(PollingEntry));
    if (!pe)
        return -1;
    pe->func = func;
    pe->opaque = opaque;
    for(ppe = &first_polling_entry; *ppe != NULL; ppe = &(*ppe)->next);
    *ppe = pe;
    return 0;
}

void qemu_del_polling_cb(PollingFunc *func, void *opaque)
{
    PollingEntry **ppe, *pe;
    for(ppe = &first_polling_entry; *ppe != NULL; ppe = &(*ppe)->next) {
        pe = *ppe;
        if (pe->func == func && pe->opaque == opaque) {
            *ppe = pe->next;
            qemu_free(pe);
            break;
        }
    }
}

#ifdef _WIN32
/***********************************************************/
/* Wait objects support */
typedef struct WaitObjects {
    int num;
    HANDLE events[MAXIMUM_WAIT_OBJECTS + 1];
    WaitObjectFunc *func[MAXIMUM_WAIT_OBJECTS + 1];
    void *opaque[MAXIMUM_WAIT_OBJECTS + 1];
} WaitObjects;

static WaitObjects wait_objects = {0};

int qemu_add_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque)
{
    WaitObjects *w = &wait_objects;

    if (w->num >= MAXIMUM_WAIT_OBJECTS)
        return -1;
    w->events[w->num] = handle;
    w->func[w->num] = func;
    w->opaque[w->num] = opaque;
    w->num++;
    return 0;
}

void qemu_del_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque)
{
    int i, found;
    WaitObjects *w = &wait_objects;

    found = 0;
    for (i = 0; i < w->num; i++) {
        if (w->events[i] == handle)
            found = 1;
        if (found) {
            w->events[i] = w->events[i + 1];
            w->func[i] = w->func[i + 1];
            w->opaque[i] = w->opaque[i + 1];
        }
    }
    if (found)
        w->num--;
}
#endif

/***********************************************************/
/* savevm/loadvm support */

void qemu_put_buffer(QEMUFile *f, const uint8_t *buf, int size)
{
    fwrite(buf, 1, size, f);
}

void qemu_put_byte(QEMUFile *f, int v)
{
    fputc(v, f);
}

void qemu_put_be16(QEMUFile *f, unsigned int v)
{
    qemu_put_byte(f, v >> 8);
    qemu_put_byte(f, v);
}

void qemu_put_be32(QEMUFile *f, unsigned int v)
{
    qemu_put_byte(f, v >> 24);
    qemu_put_byte(f, v >> 16);
    qemu_put_byte(f, v >> 8);
    qemu_put_byte(f, v);
}

void qemu_put_be64(QEMUFile *f, uint64_t v)
{
    qemu_put_be32(f, v >> 32);
    qemu_put_be32(f, v);
}

int qemu_get_buffer(QEMUFile *f, uint8_t *buf, int size)
{
    return fread(buf, 1, size, f);
}

int qemu_get_byte(QEMUFile *f)
{
    int v;
    v = fgetc(f);
    if (v == EOF)
        return 0;
    else
        return v;
}

unsigned int qemu_get_be16(QEMUFile *f)
{
    unsigned int v;
    v = qemu_get_byte(f) << 8;
    v |= qemu_get_byte(f);
    return v;
}

unsigned int qemu_get_be32(QEMUFile *f)
{
    unsigned int v;
    v = qemu_get_byte(f) << 24;
    v |= qemu_get_byte(f) << 16;
    v |= qemu_get_byte(f) << 8;
    v |= qemu_get_byte(f);
    return v;
}

uint64_t qemu_get_be64(QEMUFile *f)
{
    uint64_t v;
    v = (uint64_t)qemu_get_be32(f) << 32;
    v |= qemu_get_be32(f);
    return v;
}

int64_t qemu_ftell(QEMUFile *f)
{
    return ftell(f);
}

int64_t qemu_fseek(QEMUFile *f, int64_t pos, int whence)
{
    if (fseek(f, pos, whence) < 0)
        return -1;
    return ftell(f);
}

void  qemu_put_struct(QEMUFile*  f, const QField*  fields, const void*  s)
{
    const QField*  qf = fields;

    for (;;) {
        uint8_t*  p = (uint8_t*)s + qf->offset;

        switch (qf->type) {
            case Q_FIELD_END:
                break;
            case Q_FIELD_BYTE:
                qemu_put_byte(f, p[0]);
                break;
            case Q_FIELD_INT16:
                qemu_put_be16(f, ((uint16_t*)p)[0]);
                break;
            case Q_FIELD_INT32:
                qemu_put_be32(f, ((uint32_t*)p)[0]);
                break;
            case Q_FIELD_INT64:
                qemu_put_be64(f, ((uint64_t*)p)[0]);
                break;
            case Q_FIELD_BUFFER:
                if (fields[1].type != Q_FIELD_BUFFER_SIZE ||
                    fields[2].type != Q_FIELD_BUFFER_SIZE)
                {
                    fprintf(stderr, "%s: invalid QFIELD_BUFFER item passed as argument. aborting\n",
                            __FUNCTION__ );
                    exit(1);
                }
                else
                {
                    uint32_t  size = ((uint32_t)fields[1].offset << 16) | (uint32_t)fields[2].offset;

                    qemu_put_buffer(f, p, size);
                    qf += 2;
                }
                break;
            default:
                fprintf(stderr, "%s: invalid fields list passed as argument. aborting\n", __FUNCTION__);
                exit(1);
        }
        qf++;
    }
}

int   qemu_get_struct(QEMUFile*  f, const QField*  fields, void*  s)
{
    const QField*  qf = fields;

    for (;;) {
        uint8_t*  p = (uint8_t*)s + qf->offset;

        switch (qf->type) {
            case Q_FIELD_END:
                break;
            case Q_FIELD_BYTE:
                p[0] = qemu_get_byte(f);
                break;
            case Q_FIELD_INT16:
                ((uint16_t*)p)[0] = qemu_get_be16(f);
                break;
            case Q_FIELD_INT32:
                ((uint32_t*)p)[0] = qemu_get_be32(f);
                break;
            case Q_FIELD_INT64:
                ((uint64_t*)p)[0] = qemu_get_be64(f);
                break;
            case Q_FIELD_BUFFER:
                if (fields[1].type != Q_FIELD_BUFFER_SIZE ||
                    fields[2].type != Q_FIELD_BUFFER_SIZE)
                {
                    fprintf(stderr, "%s: invalid QFIELD_BUFFER item passed as argument.\n",
                            __FUNCTION__ );
                    return -1;
                }
                else
                {
                    uint32_t  size = ((uint32_t)fields[1].offset << 16) | (uint32_t)fields[2].offset;
                    int       ret  = qemu_get_buffer(f, p, size);

                    if (ret != size) {
                        fprintf(stderr, "%s: not enough bytes to load structure\n", __FUNCTION__);
                        return -1;
                    }
                    qf += 2;
                }
                break;
            default:
                fprintf(stderr, "%s: invalid fields list passed as argument. aborting\n", __FUNCTION__);
                exit(1);
        }
        qf++;
    }
    return 0;
}

typedef struct SaveStateEntry {
    char idstr[256];
    int instance_id;
    int version_id;
    SaveStateHandler *save_state;
    LoadStateHandler *load_state;
    void *opaque;
    struct SaveStateEntry *next;
} SaveStateEntry;

static SaveStateEntry *first_se;

int register_savevm(const char *idstr,
                    int instance_id,
                    int version_id,
                    SaveStateHandler *save_state,
                    LoadStateHandler *load_state,
                    void *opaque)
{
    SaveStateEntry *se, **pse;

    se = qemu_malloc(sizeof(SaveStateEntry));
    if (!se)
        return -1;
    pstrcpy(se->idstr, sizeof(se->idstr), idstr);
    se->instance_id = instance_id;
    se->version_id = version_id;
    se->save_state = save_state;
    se->load_state = load_state;
    se->opaque = opaque;
    se->next = NULL;

    /* add at the end of list */
    pse = &first_se;
    while (*pse != NULL)
        pse = &(*pse)->next;
    *pse = se;
    return 0;
}

#define QEMU_VM_FILE_MAGIC   0x5145564d
#define QEMU_VM_FILE_VERSION 0x00000001

int qemu_savevm(const char *filename)
{
    SaveStateEntry *se;
    QEMUFile *f;
    int len, len_pos, cur_pos, saved_vm_running, ret;

    saved_vm_running = vm_running;
    vm_stop(0);

    f = fopen(filename, "wb");
    if (!f) {
        ret = -1;
        goto the_end;
    }

    qemu_put_be32(f, QEMU_VM_FILE_MAGIC);
    qemu_put_be32(f, QEMU_VM_FILE_VERSION);

    for(se = first_se; se != NULL; se = se->next) {
        /* ID string */
        len = strlen(se->idstr);
        qemu_put_byte(f, len);
        qemu_put_buffer(f, (uint8_t*)se->idstr, len);

        qemu_put_be32(f, se->instance_id);
        qemu_put_be32(f, se->version_id);

        /* record size: filled later */
        len_pos = ftell(f);
        qemu_put_be32(f, 0);

        se->save_state(f, se->opaque);

        /* fill record size */
        cur_pos = ftell(f);
        len = ftell(f) - len_pos - 4;
        fseek(f, len_pos, SEEK_SET);
        qemu_put_be32(f, len);
        fseek(f, cur_pos, SEEK_SET);
    }

    fclose(f);
    ret = 0;
 the_end:
    if (saved_vm_running)
        vm_start();
    return ret;
}

static SaveStateEntry *find_se(const char *idstr, int instance_id)
{
    SaveStateEntry *se;

    for(se = first_se; se != NULL; se = se->next) {
        if (!strcmp(se->idstr, idstr) &&
            instance_id == se->instance_id)
            return se;
    }
    return NULL;
}

int qemu_loadvm(const char *filename)
{
    SaveStateEntry *se;
    QEMUFile *f;
    int len, cur_pos, ret, instance_id, record_len, version_id;
    int saved_vm_running;
    unsigned int v;
    char idstr[256];

    saved_vm_running = vm_running;
    vm_stop(0);

    f = fopen(filename, "rb");
    if (!f) {
        ret = -1;
        goto the_end;
    }

    v = qemu_get_be32(f);
    if (v != QEMU_VM_FILE_MAGIC)
        goto fail;
    v = qemu_get_be32(f);
    if (v != QEMU_VM_FILE_VERSION) {
    fail:
        fclose(f);
        ret = -1;
        goto the_end;
    }
    for(;;) {
        len = qemu_get_byte(f);
        if (feof(f))
            break;
        qemu_get_buffer(f, (uint8_t*)idstr, len);
        idstr[len] = '\0';
        instance_id = qemu_get_be32(f);
        version_id = qemu_get_be32(f);
        record_len = qemu_get_be32(f);
#if 0
        printf("idstr=%s instance=0x%x version=%d len=%d\n",
               idstr, instance_id, version_id, record_len);
#endif
        cur_pos = ftell(f);
        se = find_se(idstr, instance_id);
        if (!se) {
            fprintf(stderr, "qemu: warning: instance 0x%x of device '%s' not present in current VM\n",
                    instance_id, idstr);
        } else {
            ret = se->load_state(f, se->opaque, version_id);
            if (ret < 0) {
                fprintf(stderr, "qemu: warning: error while loading state for instance 0x%x of device '%s'\n",
                        instance_id, idstr);
            }
        }
        /* always seek to exact end of record */
        qemu_fseek(f, cur_pos + record_len, SEEK_SET);
    }
    fclose(f);
    ret = 0;
 the_end:
    if (saved_vm_running)
        vm_start();
    return ret;
}

/***********************************************************/
/* cpu save/restore */

#if defined(TARGET_I386)

static void cpu_put_seg(QEMUFile *f, SegmentCache *dt)
{
    qemu_put_be32(f, dt->selector);
    qemu_put_betl(f, dt->base);
    qemu_put_be32(f, dt->limit);
    qemu_put_be32(f, dt->flags);
}

static void cpu_get_seg(QEMUFile *f, SegmentCache *dt)
{
    dt->selector = qemu_get_be32(f);
    dt->base = qemu_get_betl(f);
    dt->limit = qemu_get_be32(f);
    dt->flags = qemu_get_be32(f);
}

void cpu_save(QEMUFile *f, void *opaque)
{
    CPUState *env = opaque;
    uint16_t fptag, fpus, fpuc, fpregs_format;
    uint32_t hflags;
    int i;

    for(i = 0; i < CPU_NB_REGS; i++)
        qemu_put_betls(f, &env->regs[i]);
    qemu_put_betls(f, &env->eip);
    qemu_put_betls(f, &env->eflags);
    hflags = env->hflags; /* XXX: suppress most of the redundant hflags */
    qemu_put_be32s(f, &hflags);

    /* FPU */
    fpuc = env->fpuc;
    fpus = (env->fpus & ~0x3800) | (env->fpstt & 0x7) << 11;
    fptag = 0;
    for(i = 0; i < 8; i++) {
        fptag |= ((!env->fptags[i]) << i);
    }

    qemu_put_be16s(f, &fpuc);
    qemu_put_be16s(f, &fpus);
    qemu_put_be16s(f, &fptag);

#ifdef USE_X86LDOUBLE
    fpregs_format = 0;
#else
    fpregs_format = 1;
#endif
    qemu_put_be16s(f, &fpregs_format);

    for(i = 0; i < 8; i++) {
#ifdef USE_X86LDOUBLE
        {
            uint64_t mant;
            uint16_t exp;
            /* we save the real CPU data (in case of MMX usage only 'mant'
               contains the MMX register */
            cpu_get_fp80(&mant, &exp, env->fpregs[i].d);
            qemu_put_be64(f, mant);
            qemu_put_be16(f, exp);
        }
#else
        /* if we use doubles for float emulation, we save the doubles to
           avoid losing information in case of MMX usage. It can give
           problems if the image is restored on a CPU where long
           doubles are used instead. */
        qemu_put_be64(f, env->fpregs[i].mmx.MMX_Q(0));
#endif
    }

    for(i = 0; i < 6; i++)
        cpu_put_seg(f, &env->segs[i]);
    cpu_put_seg(f, &env->ldt);
    cpu_put_seg(f, &env->tr);
    cpu_put_seg(f, &env->gdt);
    cpu_put_seg(f, &env->idt);

    qemu_put_be32s(f, &env->sysenter_cs);
    qemu_put_be32s(f, &env->sysenter_esp);
    qemu_put_be32s(f, &env->sysenter_eip);

    qemu_put_betls(f, &env->cr[0]);
    qemu_put_betls(f, &env->cr[2]);
    qemu_put_betls(f, &env->cr[3]);
    qemu_put_betls(f, &env->cr[4]);

    for(i = 0; i < 8; i++)
        qemu_put_betls(f, &env->dr[i]);

    /* MMU */
    qemu_put_be32s(f, &env->a20_mask);

    /* XMM */
    qemu_put_be32s(f, &env->mxcsr);
    for(i = 0; i < CPU_NB_REGS; i++) {
        qemu_put_be64s(f, &env->xmm_regs[i].XMM_Q(0));
        qemu_put_be64s(f, &env->xmm_regs[i].XMM_Q(1));
    }

#ifdef TARGET_X86_64
    qemu_put_be64s(f, &env->efer);
    qemu_put_be64s(f, &env->star);
    qemu_put_be64s(f, &env->lstar);
    qemu_put_be64s(f, &env->cstar);
    qemu_put_be64s(f, &env->fmask);
    qemu_put_be64s(f, &env->kernelgsbase);
#endif
}

#ifdef USE_X86LDOUBLE
/* XXX: add that in a FPU generic layer */
union x86_longdouble {
    uint64_t mant;
    uint16_t exp;
};

#define MANTD1(fp)	(fp & ((1LL << 52) - 1))
#define EXPBIAS1 1023
#define EXPD1(fp)	((fp >> 52) & 0x7FF)
#define SIGND1(fp)	((fp >> 32) & 0x80000000)

static void fp64_to_fp80(union x86_longdouble *p, uint64_t temp)
{
    int e;
    /* mantissa */
    p->mant = (MANTD1(temp) << 11) | (1LL << 63);
    /* exponent + sign */
    e = EXPD1(temp) - EXPBIAS1 + 16383;
    e |= SIGND1(temp) >> 16;
    p->exp = e;
}
#endif

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    CPUState *env = opaque;
    int i, guess_mmx;
    uint32_t hflags;
    uint16_t fpus, fpuc, fptag, fpregs_format;

    if (version_id != 3)
        return -EINVAL;
    for(i = 0; i < CPU_NB_REGS; i++)
        qemu_get_betls(f, &env->regs[i]);
    qemu_get_betls(f, &env->eip);
    qemu_get_betls(f, &env->eflags);
    qemu_get_be32s(f, &hflags);

    qemu_get_be16s(f, &fpuc);
    qemu_get_be16s(f, &fpus);
    qemu_get_be16s(f, &fptag);
    qemu_get_be16s(f, &fpregs_format);

    /* NOTE: we cannot always restore the FPU state if the image come
       from a host with a different 'USE_X86LDOUBLE' define. We guess
       if we are in an MMX state to restore correctly in that case. */
    guess_mmx = ((fptag == 0xff) && (fpus & 0x3800) == 0);
    for(i = 0; i < 8; i++) {
        uint64_t mant;
        uint16_t exp;

        switch(fpregs_format) {
        case 0:
            mant = qemu_get_be64(f);
            exp = qemu_get_be16(f);
#ifdef USE_X86LDOUBLE
            env->fpregs[i].d = cpu_set_fp80(mant, exp);
#else
            /* difficult case */
            if (guess_mmx)
                env->fpregs[i].mmx.MMX_Q(0) = mant;
            else
                env->fpregs[i].d = cpu_set_fp80(mant, exp);
#endif
            break;
        case 1:
            mant = qemu_get_be64(f);
#ifdef USE_X86LDOUBLE
            {
                union x86_longdouble *p;
                /* difficult case */
                p = (void *)&env->fpregs[i];
                if (guess_mmx) {
                    p->mant = mant;
                    p->exp = 0xffff;
                } else {
                    fp64_to_fp80(p, mant);
                }
            }
#else
            env->fpregs[i].mmx.MMX_Q(0) = mant;
#endif
            break;
        default:
            return -EINVAL;
        }
    }

    env->fpuc = fpuc;
    /* XXX: restore FPU round state */
    env->fpstt = (fpus >> 11) & 7;
    env->fpus = fpus & ~0x3800;
    fptag ^= 0xff;
    for(i = 0; i < 8; i++) {
        env->fptags[i] = (fptag >> i) & 1;
    }

    for(i = 0; i < 6; i++)
        cpu_get_seg(f, &env->segs[i]);
    cpu_get_seg(f, &env->ldt);
    cpu_get_seg(f, &env->tr);
    cpu_get_seg(f, &env->gdt);
    cpu_get_seg(f, &env->idt);

    qemu_get_be32s(f, &env->sysenter_cs);
    qemu_get_be32s(f, &env->sysenter_esp);
    qemu_get_be32s(f, &env->sysenter_eip);

    qemu_get_betls(f, &env->cr[0]);
    qemu_get_betls(f, &env->cr[2]);
    qemu_get_betls(f, &env->cr[3]);
    qemu_get_betls(f, &env->cr[4]);

    for(i = 0; i < 8; i++)
        qemu_get_betls(f, &env->dr[i]);

    /* MMU */
    qemu_get_be32s(f, &env->a20_mask);

    qemu_get_be32s(f, &env->mxcsr);
    for(i = 0; i < CPU_NB_REGS; i++) {
        qemu_get_be64s(f, &env->xmm_regs[i].XMM_Q(0));
        qemu_get_be64s(f, &env->xmm_regs[i].XMM_Q(1));
    }

#ifdef TARGET_X86_64
    qemu_get_be64s(f, &env->efer);
    qemu_get_be64s(f, &env->star);
    qemu_get_be64s(f, &env->lstar);
    qemu_get_be64s(f, &env->cstar);
    qemu_get_be64s(f, &env->fmask);
    qemu_get_be64s(f, &env->kernelgsbase);
#endif

    /* XXX: compute hflags from scratch, except for CPL and IIF */
    env->hflags = hflags;
    tlb_flush(env, 1);
    return 0;
}

#elif defined(TARGET_PPC)
void cpu_save(QEMUFile *f, void *opaque)
{
}

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    return 0;
}

#elif defined(TARGET_MIPS)
void cpu_save(QEMUFile *f, void *opaque)
{
}

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    return 0;
}

#elif defined(TARGET_SPARC)
void cpu_save(QEMUFile *f, void *opaque)
{
    CPUState *env = opaque;
    int i;
    uint32_t tmp;

    for(i = 0; i < 8; i++)
        qemu_put_betls(f, &env->gregs[i]);
    for(i = 0; i < NWINDOWS * 16; i++)
        qemu_put_betls(f, &env->regbase[i]);

    /* FPU */
    for(i = 0; i < TARGET_FPREGS; i++) {
        union {
            float32 f;
            uint32_t i;
        } u;
        u.f = env->fpr[i];
        qemu_put_be32(f, u.i);
    }

    qemu_put_betls(f, &env->pc);
    qemu_put_betls(f, &env->npc);
    qemu_put_betls(f, &env->y);
    tmp = GET_PSR(env);
    qemu_put_be32(f, tmp);
    qemu_put_betls(f, &env->fsr);
    qemu_put_betls(f, &env->tbr);
#ifndef TARGET_SPARC64
    qemu_put_be32s(f, &env->wim);
    /* MMU */
    for(i = 0; i < 16; i++)
        qemu_put_be32s(f, &env->mmuregs[i]);
#endif
}

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    CPUState *env = opaque;
    int i;
    uint32_t tmp;

    for(i = 0; i < 8; i++)
        qemu_get_betls(f, &env->gregs[i]);
    for(i = 0; i < NWINDOWS * 16; i++)
        qemu_get_betls(f, &env->regbase[i]);

    /* FPU */
    for(i = 0; i < TARGET_FPREGS; i++) {
        union {
            float32 f;
            uint32_t i;
        } u;
        u.i = qemu_get_be32(f);
        env->fpr[i] = u.f;
    }

    qemu_get_betls(f, &env->pc);
    qemu_get_betls(f, &env->npc);
    qemu_get_betls(f, &env->y);
    tmp = qemu_get_be32(f);
    env->cwp = 0; /* needed to ensure that the wrapping registers are
                     correctly updated */
    PUT_PSR(env, tmp);
    qemu_get_betls(f, &env->fsr);
    qemu_get_betls(f, &env->tbr);
#ifndef TARGET_SPARC64
    qemu_get_be32s(f, &env->wim);
    /* MMU */
    for(i = 0; i < 16; i++)
        qemu_get_be32s(f, &env->mmuregs[i]);
#endif
    tlb_flush(env, 1);
    return 0;
}

#elif defined(TARGET_ARM)

/* ??? Need to implement these.  */
void cpu_save(QEMUFile *f, void *opaque)
{
    int i;
    CPUARMState *env = (CPUARMState *)opaque;

    for (i = 0; i < 16; i++) {
        qemu_put_be32(f, env->regs[i]);
    }
    qemu_put_be32(f, cpsr_read(env));
    qemu_put_be32(f, env->spsr);
    for (i = 0; i < 6; i++) {
        qemu_put_be32(f, env->banked_spsr[i]);
        qemu_put_be32(f, env->banked_r13[i]);
        qemu_put_be32(f, env->banked_r14[i]);
    }
    for (i = 0; i < 5; i++) {
        qemu_put_be32(f, env->usr_regs[i]);
        qemu_put_be32(f, env->fiq_regs[i]);
    }

    qemu_put_be32(f, env->cp15.c0_cpuid);
    qemu_put_be32(f, env->cp15.c1_sys);
    qemu_put_be32(f, env->cp15.c1_coproc);
    qemu_put_be32(f, env->cp15.c2);
    qemu_put_be32(f, env->cp15.c3);
    qemu_put_be32(f, env->cp15.c5_insn);
    qemu_put_be32(f, env->cp15.c5_data);
    qemu_put_be32(f, env->cp15.c6_insn);
    qemu_put_be32(f, env->cp15.c6_data);
    qemu_put_be32(f, env->cp15.c9_insn);
    qemu_put_be32(f, env->cp15.c9_data);
    qemu_put_be32(f, env->cp15.c13_fcse);
    qemu_put_be32(f, env->cp15.c13_context);

    qemu_put_be32(f, env->features);

    if (arm_feature(env, ARM_FEATURE_VFP)) {
        for (i = 0;  i < 16; i++) {
            CPU_DoubleU u;
            u.d = env->vfp.regs[i];
            qemu_put_be32(f, u.l.upper);
            qemu_put_be32(f, u.l.lower);
        }
        for (i = 0; i < 16; i++) {
            qemu_put_be32(f, env->vfp.xregs[i]);
        }

        /* TODO: Should use proper FPSCR access functions.  */
        qemu_put_be32(f, env->vfp.vec_len);
        qemu_put_be32(f, env->vfp.vec_stride);
    }
}

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    CPUARMState *env = (CPUARMState *)opaque;
    int i;

    if (version_id != ARM_CPU_SAVE_VERSION)
        return -EINVAL;

    for (i = 0; i < 16; i++) {
        env->regs[i] = qemu_get_be32(f);
    }
    cpsr_write(env, qemu_get_be32(f), 0xffffffff);
    env->spsr = qemu_get_be32(f);
    for (i = 0; i < 6; i++) {
        env->banked_spsr[i] = qemu_get_be32(f);
        env->banked_r13[i] = qemu_get_be32(f);
        env->banked_r14[i] = qemu_get_be32(f);
    }
    for (i = 0; i < 5; i++) {
        env->usr_regs[i] = qemu_get_be32(f);
        env->fiq_regs[i] = qemu_get_be32(f);
    }
    env->cp15.c0_cpuid    = qemu_get_be32(f);
    env->cp15.c1_sys      = qemu_get_be32(f);
    env->cp15.c1_coproc   = qemu_get_be32(f);
    env->cp15.c2          = qemu_get_be32(f);
    env->cp15.c3          = qemu_get_be32(f);
    env->cp15.c5_insn     = qemu_get_be32(f);
    env->cp15.c5_data     = qemu_get_be32(f);
    env->cp15.c6_insn     = qemu_get_be32(f);
    env->cp15.c6_data     = qemu_get_be32(f);
    env->cp15.c9_insn     = qemu_get_be32(f);
    env->cp15.c9_data     = qemu_get_be32(f);
    env->cp15.c13_fcse    = qemu_get_be32(f);
    env->cp15.c13_context = qemu_get_be32(f);

    env->features = qemu_get_be32(f);

    if (arm_feature(env, ARM_FEATURE_VFP)) {
        for (i = 0;  i < 16; i++) {
            CPU_DoubleU u;
            u.l.upper = qemu_get_be32(f);
            u.l.lower = qemu_get_be32(f);
            env->vfp.regs[i] = u.d;
        }
        for (i = 0; i < 16; i++) {
            env->vfp.xregs[i] = qemu_get_be32(f);
        }

        /* TODO: Should use proper FPSCR access functions.  */
        env->vfp.vec_len = qemu_get_be32(f);
        env->vfp.vec_stride = qemu_get_be32(f);
    }

    return 0;
}

#else

#warning No CPU save/restore functions

#endif

/***********************************************************/
/* ram save/restore */

/* we just avoid storing empty pages */
static void ram_put_page(QEMUFile *f, const uint8_t *buf, int len)
{
    int i, v;

    v = buf[0];
    for(i = 1; i < len; i++) {
        if (buf[i] != v)
            goto normal_save;
    }
    qemu_put_byte(f, 1);
    qemu_put_byte(f, v);
    return;
 normal_save:
    qemu_put_byte(f, 0);
    qemu_put_buffer(f, buf, len);
}

static int ram_get_page(QEMUFile *f, uint8_t *buf, int len)
{
    int v;

    v = qemu_get_byte(f);
    switch(v) {
    case 0:
        if (qemu_get_buffer(f, buf, len) != len)
            return -EIO;
        break;
    case 1:
        v = qemu_get_byte(f);
        memset(buf, v, len);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static void ram_save(QEMUFile *f, void *opaque)
{
    int i;
    qemu_put_be32(f, phys_ram_size);
    for(i = 0; i < phys_ram_size; i+= TARGET_PAGE_SIZE) {
        ram_put_page(f, phys_ram_base + i, TARGET_PAGE_SIZE);
    }
}

static int ram_load(QEMUFile *f, void *opaque, int version_id)
{
    int i, ret;

    if (version_id != 1)
        return -EINVAL;
    if (qemu_get_be32(f) != phys_ram_size)
        return -EINVAL;
    for(i = 0; i < phys_ram_size; i+= TARGET_PAGE_SIZE) {
        ret = ram_get_page(f, phys_ram_base + i, TARGET_PAGE_SIZE);
        if (ret)
            return ret;
    }
    return 0;
}

/***********************************************************/
/* machine registration */

QEMUMachine *first_machine = NULL;

int qemu_register_machine(QEMUMachine *m)
{
    QEMUMachine **pm;
    pm = &first_machine;
    while (*pm != NULL)
        pm = &(*pm)->next;
    m->next = NULL;
    *pm = m;
    return 0;
}

QEMUMachine *find_machine(const char *name)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (!strcmp(m->name, name))
            return m;
    }
    return NULL;
}

/***********************************************************/
/* main execution loop */

void gui_update(void *opaque)
{
    display_state.dpy_refresh(&display_state);
    qemu_mod_timer(gui_timer, GUI_REFRESH_INTERVAL + qemu_get_clock(rt_clock));
}

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    LIST_ENTRY (vm_change_state_entry) entries;
};

static LIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = qemu_mallocz(sizeof (*e));
    if (!e)
        return NULL;

    e->cb = cb;
    e->opaque = opaque;
    LIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    LIST_REMOVE (e, entries);
    qemu_free (e);
}

static void vm_state_notify(int running)
{
    VMChangeStateEntry *e;

    for (e = vm_change_state_head.lh_first; e; e = e->entries.le_next) {
        e->cb(e->opaque, running);
    }
}

/* XXX: support several handlers */
static VMStopHandler *vm_stop_cb;
static void *vm_stop_opaque;

int qemu_add_vm_stop_handler(VMStopHandler *cb, void *opaque)
{
    vm_stop_cb = cb;
    vm_stop_opaque = opaque;
    return 0;
}

void qemu_del_vm_stop_handler(VMStopHandler *cb, void *opaque)
{
    vm_stop_cb = NULL;
}

void vm_start(void)
{
    if (!vm_running) {
        cpu_enable_ticks();
        vm_running = 1;
        vm_state_notify(1);
    }
}

void vm_stop(int reason)
{
    if (vm_running) {
        cpu_disable_ticks();
        vm_running = 0;
        if (reason != 0) {
            if (vm_stop_cb) {
                vm_stop_cb(vm_stop_opaque, reason);
            }
        }
        vm_state_notify(0);
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QEMUResetHandler *func;
    void *opaque;
    struct QEMUResetEntry *next;
} QEMUResetEntry;

static QEMUResetEntry *first_reset_entry;
static int reset_requested;
static int shutdown_requested;
static int powerdown_requested;

void qemu_register_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry **pre, *re;

    pre = &first_reset_entry;
    while (*pre != NULL)
        pre = &(*pre)->next;
    re = qemu_mallocz(sizeof(QEMUResetEntry));
    re->func = func;
    re->opaque = opaque;
    re->next = NULL;
    *pre = re;
}

void qemu_system_reset(void)
{
    QEMUResetEntry *re;

    /* reset all devices */
    for(re = first_reset_entry; re != NULL; re = re->next) {
        re->func(re->opaque);
    }
}

void qemu_system_reset_request(void)
{
    reset_requested = 1;
    if (cpu_single_env)
        cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

#ifdef HAS_AUDIO
extern void  AUD_cleanup();
#endif

void qemu_system_shutdown_request(void)
{
    int i;
    shutdown_requested = 1;

    for (i = 0; i < MAX_DISKS; i++) {
        if (bs_table[i])
            bdrv_close(bs_table[i]);
    }

#ifdef HAS_AUDIO
    // this is necessary to avoid a deadlock in SDL when quitting
    AUD_cleanup();
#endif
    if (cpu_single_env)
        cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

void qemu_system_powerdown_request(void)
{
    powerdown_requested = 1;
    if (cpu_single_env)
        cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}

#define  MAIN_LOOP_STATS  0

#if  MAIN_LOOP_STATS
typedef struct {
    int         counter;
	int64_t     reset_time;       // time when counter is reset
    int64_t     spent_time_total; // total time spent since last counter reset
	int64_t     spent_time_min;   // minimum time spent in call
	int64_t     spent_time_max;   // maximum time spent in call
	int64_t     wait_time_total;  // total time spent waiting for select()
} MainLoopStats;

static __inline__ int64_t
mainloopstats_now( void )
{
	return  qemu_get_clock( vm_clock );
}

static __inline__ double
mainloopstats_to_ms( int64_t  duration )
{
	return duration / 1000000.;
}

static void
mainloopstats_reset( MainLoopStats*  s )
{
	int64_t   now = qemu_get_clock( vm_clock );

	s->counter          = 0;
	s->reset_time       = now;
	s->spent_time_total = 0;
	s->wait_time_total  = 0;
	s->spent_time_min   = INT_MAX;
	s->spent_time_max   = 0;
}

static MainLoopStats   main_loop_stats;
#endif  /* MAIN_LOOP_STATS */

void main_loop_wait(int timeout)
{
    IOHandlerRecord *ioh, *ioh_next;
    fd_set rfds, wfds, xfds;
    int ret, nfds;
    struct timeval tv;
    PollingEntry *pe;

#if MAIN_LOOP_STATS
	int64   time_before = mainloopstats_now();
	int64   time_after_select;
#endif

#if 0
    /* disable timeout when we're in polling mode */
	if (qemu_milli_needed > 0)
		timeout = 0;
#endif

    /* XXX: need to suppress polling by better using win32 events */
    ret = 0;
    for(pe = first_polling_entry; pe != NULL; pe = pe->next) {
        ret |= pe->func(pe->opaque);
    }
#ifdef _WIN32
    if (ret == 0) {
        int err;
        WaitObjects *w = &wait_objects;

        ret = WaitForMultipleObjects(w->num, w->events, FALSE, timeout);
        if (WAIT_OBJECT_0 + 0 <= ret && ret <= WAIT_OBJECT_0 + w->num - 1) {
            /* Check for additional signaled events */
            int  i;
            for(i = (ret - WAIT_OBJECT_0 + 1); i < w->num; i++) {

                /* Check if event is signaled */
                int  ret2 = WaitForSingleObject(w->events[i], 0);
                if(ret2 == WAIT_OBJECT_0) {
                    if (w->func[i])
                        w->func[i](w->opaque[i]);
                } else if (ret2 == WAIT_TIMEOUT) {
                } else {
                    err = GetLastError();
                    fprintf(stderr, "WaitForSingleObject error %d %d\n", i, err);
                }
            }
        } else if (ret == WAIT_TIMEOUT) {
        } else {
            err = GetLastError();
            fprintf(stderr, "Wait error %d %d\n", ret, err);
        }
    }
#endif
    /* poll any events */
    /* XXX: separate device handlers from system ones */
    nfds = -1;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
    for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
        if (ioh->fd_read &&
            (!ioh->fd_read_poll ||
             ioh->fd_read_poll(ioh->opaque) != 0)) {
            FD_SET(ioh->fd, &rfds);
            if (ioh->fd > nfds)
                nfds = ioh->fd;
        }
        if (ioh->fd_write) {
            FD_SET(ioh->fd, &wfds);
            if (ioh->fd > nfds)
                nfds = ioh->fd;
        }
    }

    tv.tv_sec = 0;
#ifdef _WIN32
    tv.tv_usec = 0;
#else
    tv.tv_usec = timeout * 1000;
#endif
#if defined(CONFIG_SLIRP)
    if (slirp_inited) {
        slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
    }
#endif
    ret = select(nfds + 1, &rfds, &wfds, &xfds, &tv);
#if MAIN_LOOP_STATS
	time_after_select = mainloopstats_now();
#endif
    if (ret > 0) {
        for(ioh = first_io_handler; ioh != NULL; ioh = ioh_next) {
            ioh_next = ioh->next;

            ioh->state = IOH_STATE_PENDING;

            if (FD_ISSET(ioh->fd, &rfds)) {
                ioh->fd_read(ioh->opaque);
                if (ioh->state == IOH_STATE_CLOSED) {
                    qemu_free(ioh);
                    continue;
                }
            }
            if (FD_ISSET(ioh->fd, &wfds)) {
                ioh->fd_write(ioh->opaque);
                if (ioh->state == IOH_STATE_CLOSED) {
                    qemu_free(ioh);
                    continue;
                }
            }

            ioh->state = 0;
        }
    }

#if 0
    if (qemu_milli_needed && qemu_milli_check())
        _check_timers();
#endif

#if defined(CONFIG_SLIRP)
    if (slirp_inited) {
        if (ret < 0) {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&xfds);
        }
        slirp_select_poll(&rfds, &wfds, &xfds);
    }
#endif
#ifdef _WIN32
    tap_win32_poll();
#endif
    charpipe_poll();

    if (vm_running) {
        qemu_run_virtual_timers();
        /* run dma transfers, if any */
        DMA_run();
    }

    qemu_run_realtime_timers();
    qemu_rearm_alarm_timer();

#if MAIN_LOOP_STATS
	{
		MainLoopStats*  s = &main_loop_stats;
		int64_t         time_after = mainloopstats_now();
		int64_t         time_diff  = time_after - time_before;

		s->spent_time_total += time_diff;
		if (time_diff < s->spent_time_min)
			s->spent_time_min = time_diff;
	    if (time_diff > s->spent_time_max)
		    s->spent_time_max = time_diff;

		time_diff = time_after_select - time_before;
		s->wait_time_total += time_diff;

		if (++s->counter == 1000) {
			double  period        = time_after - s->reset_time;
			double  average_spent = s->spent_time_total * 1. / s->counter;
			double  average_wait  = s->wait_time_total * 1. / s->counter;

			printf( "main loop stats:  iterations: %8ld,  period: %10.2f ms, avg wait time: %10.2f ms  (%.3f %%), avg exec time %10.2f ms (%.3f %%)\n",
			        s->counter,
					mainloopstats_to_ms(period),
					mainloopstats_to_ms(average_wait),
					s->wait_time_total * 100. / period,
					mainloopstats_to_ms(average_spent),
					s->spent_time_total * 100. / period );

			mainloopstats_reset( s );
		}
	}
#endif  /* MAIN_LOOP_STATS */
}

int main_loop(void)
{
    int ret, timeout;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif
    CPUState *env;

    cur_cpu = first_cpu;
    for(;;) {
        if (vm_running) {

            env = cur_cpu;
            for(;;) {
                /* get next cpu */
                env = env->next_cpu;
                if (!env)
                    env = first_cpu;
#ifdef CONFIG_PROFILER
                ti = profile_getclock();
#endif
                ret = cpu_exec(env);
#ifdef CONFIG_PROFILER
                qemu_time += profile_getclock() - ti;
#endif
                if (event_pending) {
                    ret = EXCP_INTERRUPT;
                    event_pending = 0;
                    break;
                }
                if (ret != EXCP_HALTED)
                    break;
                /* all CPUs are halted ? */
                if (env == cur_cpu) {
                    ret = EXCP_HLT;
                    break;
                }
            }
            cur_cpu = env;

#ifdef CONFIG_TRACE
            if (tbflush_requested) {
                tbflush_requested = 0;
                tb_flush(env);
                ret = EXCP_INTERRUPT;
            } else if (exit_requested)
				goto ExitRequested;
#endif

            if (shutdown_requested) {
                ret = EXCP_INTERRUPT;
                break;
            }
            if (reset_requested) {
                reset_requested = 0;
                qemu_system_reset();
                ret = EXCP_INTERRUPT;
            }
            if (powerdown_requested) {
                powerdown_requested = 0;
                qemu_system_powerdown();
                ret = EXCP_INTERRUPT;
            }
            if (ret == EXCP_DEBUG) {
                vm_stop(EXCP_DEBUG);
            }
            /* if hlt instruction, we wait until the next IRQ */
            if (ret == EXCP_HLT) {
                timeout = 10 /* _get_timers_timeout() */;
                //write(2,"%",1);
            }
            else {
                timeout = 0;
                //write(2,"&",1);
            }
        } else {
            timeout = 10;
        }
#ifdef CONFIG_PROFILER
        ti = profile_getclock();
#endif
        main_loop_wait(timeout);
#ifdef CONFIG_PROFILER
        dev_time += profile_getclock() - ti;
#endif
    }
    cpu_disable_ticks();
    return ret;

#ifdef CONFIG_TRACE
ExitRequested:
#  ifdef HAS_AUDIO
    AUD_cleanup();
#  endif
    exit(1);
	return 0;
#endif
}

void help(void)
{
    printf("QEMU PC emulator version " QEMU_VERSION ", Copyright (c) 2003-2005 Fabrice Bellard\n"
           "usage: %s [options] [disk_image]\n"
           "\n"
           "'disk_image' is a raw hard image image for IDE hard disk 0\n"
           "\n"
           "Standard options:\n"
           "-M machine      select emulated machine (-M ? for list)\n"
           "-fda/-fdb file  use 'file' as floppy disk 0/1 image\n"
           "-hda/-hdb file  use 'file' as IDE hard disk 0/1 image\n"
           "-hdc/-hdd file  use 'file' as IDE hard disk 2/3 image\n"
           "-cdrom file     use 'file' as IDE cdrom image (cdrom is ide1 master)\n"
           "-boot [a|c|d]   boot on floppy (a), hard disk (c) or CD-ROM (d)\n"
	   "-snapshot       write to temporary files instead of disk image files\n"
#ifdef TARGET_I386
           "-no-fd-bootchk  disable boot signature checking for floppy disks\n"
#endif
           "-m megs         set virtual RAM size to megs MB [default=%d]\n"
           "-smp n          set the number of CPUs to 'n' [default=1]\n"
           "-nographic      disable graphical output and redirect serial I/Os to console\n"
#ifndef _WIN32
	   "-k language     use keyboard layout (for example \"fr\" for French)\n"
#endif
#ifdef HAS_AUDIO
           "-audio-help     print list of audio drivers and their options\n"
           "-soundhw c1,... enable audio support\n"
           "                and only specified sound cards (comma separated list)\n"
           "                use -soundhw ? to get the list of supported cards\n"
           "                use -soundhw all to enable all of them\n"
#endif
           "-localtime      set the real time clock to local time [default=utc]\n"
           "-full-screen    start in full screen\n"
#ifdef TARGET_I386
           "-win2k-hack     use it when installing Windows 2000 to avoid a disk full bug\n"
#endif
           "-usb            enable the USB driver (will be the default soon)\n"
           "-usbdevice name add the host or guest USB device 'name'\n"
#if defined(TARGET_PPC) || defined(TARGET_SPARC)
           "-g WxH[xDEPTH]  Set the initial graphical resolution and depth\n"
#endif
           "\n"
           "Network options:\n"
           "-net nic[,vlan=n][,macaddr=addr][,model=type]\n"
           "                create a new Network Interface Card and connect it to VLAN 'n'\n"
#ifdef CONFIG_SLIRP
           "-net user[,vlan=n][,hostname=host]\n"
           "                connect the user mode network stack to VLAN 'n' and send\n"
           "                hostname 'host' to DHCP clients\n"
#endif
#ifdef _WIN32
           "-net tap[,vlan=n],ifname=name\n"
           "                connect the host TAP network interface to VLAN 'n'\n"
#else
           "-net tap[,vlan=n][,fd=h][,ifname=name][,script=file]\n"
           "                connect the host TAP network interface to VLAN 'n' and use\n"
           "                the network script 'file' (default=%s);\n"
           "                use 'fd=h' to connect to an already opened TAP interface\n"
#endif
           "-net socket[,vlan=n][,fd=h][,listen=[host]:port][,connect=host:port]\n"
           "                connect the vlan 'n' to another VLAN using a socket connection\n"
           "-net socket[,vlan=n][,fd=h][,mcast=maddr:port]\n"
           "                connect the vlan 'n' to multicast maddr and port\n"
           "-net none       use it alone to have zero network devices; if no -net option\n"
           "                is provided, the default is '-net nic -net user'\n"
           "\n"
#ifdef CONFIG_SLIRP
           "-tftp prefix    allow tftp access to files starting with prefix [-net user]\n"
           "-bootp file     advertise file in BOOTP replies\n"
#ifndef _WIN32
           "-smb dir        allow SMB access to files in 'dir' [-net user]\n"
#endif
           "-redir [tcp|udp]:host-port:[guest-host]:guest-port\n"
           "                redirect TCP or UDP connections from host to guest [-net user]\n"
#endif
           "\n"
           "Linux boot specific:\n"
           "-kernel bzImage use 'bzImage' as kernel image\n"
           "-append cmdline use 'cmdline' as kernel command line\n"
           "-initrd file    use 'file' as initial ram disk\n"
           "\n"
           "Debug/Expert options:\n"
           "-monitor dev    redirect the monitor to char device 'dev'\n"
           "-serial dev     redirect the serial port to char device 'dev'\n"
           "-parallel dev   redirect the parallel port to char device 'dev'\n"
           "-pidfile file   Write PID to 'file'\n"
           "-S              freeze CPU at startup (use 'c' to start execution)\n"
           "-s              wait gdb connection to port %d\n"
           "-p port         change gdb connection port\n"
           "-d item1,...    output log to %s (use -d ? for a list of log items)\n"
           "-hdachs c,h,s[,t]  force hard disk 0 physical geometry and the optional BIOS\n"
           "                translation (t=none or lba) (usually qemu can guess them)\n"
           "-L path         set the directory for the BIOS and VGA BIOS\n"
#ifdef USE_KQEMU
           "-kernel-kqemu   enable KQEMU full virtualization (default is user mode only)\n"
           "-no-kqemu       disable KQEMU kernel module usage\n"
#endif
#ifdef USE_CODE_COPY
           "-no-code-copy   disable code copy acceleration\n"
#endif
#ifdef TARGET_I386
           "-std-vga        simulate a standard VGA card with VESA Bochs Extensions\n"
           "                (default is CL-GD5446 PCI VGA)\n"
           "-no-acpi        disable ACPI\n"
#endif
           "-loadvm file    start right away with a saved state (loadvm in monitor)\n"
	   "-vnc display    start a VNC server on display\n"
#ifdef CONFIG_TRACE
	   "-trace file     create an execution trace in 'file' (implies -tracing on)\n"
	   "-tracing off    start with tracing off\n"
	   "-trace_miss     include tracing of cache miss addresses\n"
	   "-trace_addr     include tracing of all load/store addresses\n"
	   "-dcache_load_miss cycles\n"
           "                set the dcache load miss penalty to 'cycles'\n"
	   "-dcache_store_miss cycles\n"
           "                set the dcache store miss penalty to 'cycles'\n"
#endif
#ifdef CONFIG_NAND
           "-nand name[,readonly][,size=size][,pagesize=size][,extrasize=size][,erasepages=pages][,initfile=file][,file=file]"
#endif
           "\n"
           "During emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n"
           ,
           "qemu",
           DEFAULT_RAM_SIZE,
#ifndef _WIN32
           DEFAULT_NETWORK_SCRIPT,
#endif
           DEFAULT_GDBSTUB_PORT,
           "/tmp/qemu.log");
    exit(1);
}

#define HAS_ARG 0x0001

enum {
    QEMU_OPTION_h,

    QEMU_OPTION_M,
    QEMU_OPTION_fda,
    QEMU_OPTION_fdb,
    QEMU_OPTION_hda,
    QEMU_OPTION_hdb,
    QEMU_OPTION_hdc,
    QEMU_OPTION_hdd,
    QEMU_OPTION_cdrom,
    QEMU_OPTION_boot,
    QEMU_OPTION_snapshot,
#ifdef TARGET_I386
    QEMU_OPTION_no_fd_bootchk,
#endif
    QEMU_OPTION_m,
    QEMU_OPTION_nographic,
#ifdef HAS_AUDIO
    QEMU_OPTION_audio_help,
    QEMU_OPTION_soundhw,
#endif

    QEMU_OPTION_net,
    QEMU_OPTION_tftp,
    QEMU_OPTION_bootp,
    QEMU_OPTION_smb,
    QEMU_OPTION_redir,

    QEMU_OPTION_kernel,
    QEMU_OPTION_append,
    QEMU_OPTION_initrd,

    QEMU_OPTION_S,
    QEMU_OPTION_s,
    QEMU_OPTION_p,
    QEMU_OPTION_d,
    QEMU_OPTION_hdachs,
    QEMU_OPTION_L,
    QEMU_OPTION_no_code_copy,
    QEMU_OPTION_k,
    QEMU_OPTION_localtime,
    QEMU_OPTION_g,
    QEMU_OPTION_monitor,
    QEMU_OPTION_serial,
    QEMU_OPTION_parallel,
    QEMU_OPTION_loadvm,
    QEMU_OPTION_full_screen,
    QEMU_OPTION_pidfile,
    QEMU_OPTION_no_kqemu,
    QEMU_OPTION_kernel_kqemu,
    QEMU_OPTION_win2k_hack,
    QEMU_OPTION_usb,
    QEMU_OPTION_usbdevice,
    QEMU_OPTION_smp,
    QEMU_OPTION_vnc,
    QEMU_OPTION_no_acpi,
    QEMU_OPTION_noaudio,
    QEMU_OPTION_mic,
#ifdef CONFIG_TRACE
    QEMU_OPTION_trace_file,
    QEMU_OPTION_tracing,
    QEMU_OPTION_trace_miss,
    QEMU_OPTION_trace_addr,
    QEMU_OPTION_dcache_load_miss,
    QEMU_OPTION_dcache_store_miss,
#endif
#ifdef CONFIG_NAND
    QEMU_OPTION_nand,
#endif
    QEMU_OPTION_clock,
};

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
} QEMUOption;

const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h },

    { "M", HAS_ARG, QEMU_OPTION_M },
    { "fda", HAS_ARG, QEMU_OPTION_fda },
    { "fdb", HAS_ARG, QEMU_OPTION_fdb },
    { "hda", HAS_ARG, QEMU_OPTION_hda },
    { "hdb", HAS_ARG, QEMU_OPTION_hdb },
    { "hdc", HAS_ARG, QEMU_OPTION_hdc },
    { "hdd", HAS_ARG, QEMU_OPTION_hdd },
    { "cdrom", HAS_ARG, QEMU_OPTION_cdrom },
    { "boot", HAS_ARG, QEMU_OPTION_boot },
    { "snapshot", 0, QEMU_OPTION_snapshot },
#ifdef TARGET_I386
    { "no-fd-bootchk", 0, QEMU_OPTION_no_fd_bootchk },
#endif
    { "m", HAS_ARG, QEMU_OPTION_m },
    { "nographic", 0, QEMU_OPTION_nographic },
    { "k", HAS_ARG, QEMU_OPTION_k },
#ifdef HAS_AUDIO
    { "audio-help", 0, QEMU_OPTION_audio_help },
    { "soundhw", HAS_ARG, QEMU_OPTION_soundhw },
#endif

    { "net", HAS_ARG, QEMU_OPTION_net},
#ifdef CONFIG_SLIRP
    { "tftp", HAS_ARG, QEMU_OPTION_tftp },
    { "bootp", HAS_ARG, QEMU_OPTION_bootp },
#ifndef _WIN32
    { "smb", HAS_ARG, QEMU_OPTION_smb },
#endif
    { "redir", HAS_ARG, QEMU_OPTION_redir },
#endif

    { "kernel", HAS_ARG, QEMU_OPTION_kernel },
    { "append", HAS_ARG, QEMU_OPTION_append },
    { "initrd", HAS_ARG, QEMU_OPTION_initrd },

    { "S", 0, QEMU_OPTION_S },
    { "s", 0, QEMU_OPTION_s },
    { "p", HAS_ARG, QEMU_OPTION_p },
    { "d", HAS_ARG, QEMU_OPTION_d },
    { "hdachs", HAS_ARG, QEMU_OPTION_hdachs },
    { "L", HAS_ARG, QEMU_OPTION_L },
    { "no-code-copy", 0, QEMU_OPTION_no_code_copy },
#ifdef USE_KQEMU
    { "no-kqemu", 0, QEMU_OPTION_no_kqemu },
    { "kernel-kqemu", 0, QEMU_OPTION_kernel_kqemu },
#endif
#if defined(TARGET_PPC) || defined(TARGET_SPARC)
    { "g", HAS_ARG, QEMU_OPTION_g },
#endif
    { "localtime", 0, QEMU_OPTION_localtime },
    { "monitor", HAS_ARG, QEMU_OPTION_monitor },
    { "serial", HAS_ARG, QEMU_OPTION_serial },
    { "parallel", HAS_ARG, QEMU_OPTION_parallel },
    { "loadvm", HAS_ARG, QEMU_OPTION_loadvm },
    { "full-screen", 0, QEMU_OPTION_full_screen },
    { "pidfile", HAS_ARG, QEMU_OPTION_pidfile },
    { "win2k-hack", 0, QEMU_OPTION_win2k_hack },
    { "usbdevice", HAS_ARG, QEMU_OPTION_usbdevice },
    { "smp", HAS_ARG, QEMU_OPTION_smp },
    { "vnc", HAS_ARG, QEMU_OPTION_vnc },

    /* temporary options */
    { "usb", 0, QEMU_OPTION_usb },
    { "no-acpi", 0, QEMU_OPTION_no_acpi },

    /* android stuff */
    { "noaudio", 0, QEMU_OPTION_noaudio },
    { "mic", HAS_ARG, QEMU_OPTION_mic },
#ifdef CONFIG_TRACE
    { "trace", HAS_ARG, QEMU_OPTION_trace_file },
    { "tracing", HAS_ARG, QEMU_OPTION_tracing },
    { "trace_miss", 0, QEMU_OPTION_trace_miss },
    { "trace_addr", 0, QEMU_OPTION_trace_addr },
    { "dcache_load_miss", HAS_ARG, QEMU_OPTION_dcache_load_miss },
    { "dcache_store_miss", HAS_ARG, QEMU_OPTION_dcache_store_miss },
#endif
#ifdef CONFIG_NAND
    { "nand", HAS_ARG, QEMU_OPTION_nand },
#endif
    { "clock", HAS_ARG, QEMU_OPTION_clock },
    { NULL },
};

#if defined (TARGET_I386) && defined(USE_CODE_COPY)

/* this stack is only used during signal handling */
#define SIGNAL_STACK_SIZE 32768

static uint8_t *signal_stack;

#endif

/* password input */

static BlockDriverState *get_bdrv(int index)
{
    BlockDriverState *bs;

    if (index < 4) {
        bs = bs_table[index];
    } else if (index < 6) {
        bs = fd_table[index - 4];
    } else {
        bs = NULL;
    }
    return bs;
}

static void read_passwords(void)
{
    BlockDriverState *bs;
    int i, j;
    char password[256];

    for(i = 0; i < 6; i++) {
        bs = get_bdrv(i);
        if (bs && bdrv_is_encrypted(bs)) {
            term_printf("%s is encrypted.\n", bdrv_get_device_name(bs));
            for(j = 0; j < 3; j++) {
                monitor_readline("Password: ",
                                 1, password, sizeof(password));
                if (bdrv_set_key(bs, password) == 0)
                    break;
                term_printf("invalid password\n");
            }
        }
    }
}

/* XXX: currently we cannot use simultaneously different CPUs */
void register_machines(void)
{
#if defined(TARGET_I386)
    qemu_register_machine(&pc_machine);
    qemu_register_machine(&isapc_machine);
#elif defined(TARGET_PPC)
    qemu_register_machine(&heathrow_machine);
    qemu_register_machine(&core99_machine);
    qemu_register_machine(&prep_machine);
#elif defined(TARGET_MIPS)
    qemu_register_machine(&mips_machine);
#elif defined(TARGET_SPARC)
#ifdef TARGET_SPARC64
    qemu_register_machine(&sun4u_machine);
#else
    qemu_register_machine(&sun4m_machine);
#endif
#elif defined(TARGET_ARM)
    qemu_register_machine(&android_arm_machine);
#if 0
    qemu_register_machine(&integratorcp926_machine);
    qemu_register_machine(&integratorcp1026_machine);
    qemu_register_machine(&versatilepb_machine);
    qemu_register_machine(&versatileab_machine);
#endif
#elif defined(TARGET_SH4)
    qemu_register_machine(&shix_machine);
#else
#error unsupported CPU
#endif
}

#ifdef HAS_AUDIO
struct soundhw soundhw[] = {
#if 0  /* ANDROID */
#ifdef TARGET_I386
    {
        "pcspk",
        "PC speaker",
        0,
        1,
        { .init_isa = pcspk_audio_init }
    },
#endif
    {
        "sb16",
        "Creative Sound Blaster 16",
        0,
        1,
        { .init_isa = SB16_init }
    },

#ifdef CONFIG_ADLIB
    {
        "adlib",
#ifdef HAS_YMF262
        "Yamaha YMF262 (OPL3)",
#else
        "Yamaha YM3812 (OPL2)",
#endif
        0,
        1,
        { .init_isa = Adlib_init }
    },
#endif

#ifdef CONFIG_GUS
    {
        "gus",
        "Gravis Ultrasound GF1",
        0,
        1,
        { .init_isa = GUS_init }
    },
#endif

    {
        "es1370",
        "ENSONIQ AudioPCI ES1370",
        0,
        0,
        { .init_pci = es1370_init }
    },
#endif /* ANDROID */
    { NULL, NULL, 0, 0, { NULL } }
};

static void select_soundhw (const char *optarg)
{
    struct soundhw *c;

    if (*optarg == '?') {
    show_valid_cards:

        printf ("Valid sound card names (comma separated):\n");
        for (c = soundhw; c->name; ++c) {
            printf ("%-11s %s\n", c->name, c->descr);
        }
        printf ("\n-soundhw all will enable all of the above\n");
        exit (*optarg != '?');
    }
    else {
        size_t l;
        const char *p;
        char *e;
        int bad_card = 0;

        if (!strcmp (optarg, "all")) {
            for (c = soundhw; c->name; ++c) {
                c->enabled = 1;
            }
            return;
        }

        p = optarg;
        while (*p) {
            e = strchr (p, ',');
            l = !e ? strlen (p) : (size_t) (e - p);

            for (c = soundhw; c->name; ++c) {
                if (!strncmp (c->name, p, l)) {
                    c->enabled = 1;
                    break;
                }
            }

            if (!c->name) {
                if (l > 80) {
                    fprintf (stderr,
                             "Unknown sound card name (too big to show)\n");
                }
                else {
                    fprintf (stderr, "Unknown sound card name `%.*s'\n",
                             (int) l, p);
                }
                bad_card = 1;
            }
            p += l + (e != NULL);
        }

        if (bad_card)
            goto show_valid_cards;
    }
}
#endif

#ifdef _WIN32
static BOOL WINAPI qemu_ctrl_handler(DWORD type)
{
	//qemu_del_wait_object(host_alarm,NULL,NULL);
    exit(STATUS_CONTROL_C_EXIT);
    return TRUE;
}
#endif

#define MAX_NET_CLIENTS 32

int main(int argc, char **argv)
{
#ifdef CONFIG_GDBSTUB
    int use_gdbstub, gdbstub_port;
#endif
    int i, cdrom_index;
    int snapshot, linux_boot;
    const char *initrd_filename;
    const char *hd_filename[MAX_DISKS], *fd_filename[MAX_FD];
    const char *kernel_filename, *kernel_cmdline;
    DisplayState *ds = &display_state;
    int cyls, heads, secs, translation;
    int start_emulation = 1;
    char net_clients[MAX_NET_CLIENTS][256];
    int nb_net_clients;
    int optind;
    const char *r, *optarg;
    CharDriverState *monitor_hd;
    char monitor_device[128];
    char serial_devices[MAX_SERIAL_PORTS][128];
    int serial_device_index;
    char parallel_devices[MAX_PARALLEL_PORTS][128];
    int parallel_device_index;
    const char *loadvm = NULL;
    QEMUMachine *machine;
    char usb_devices[MAX_USB_CMDLINE][128];
    int usb_devices_index;

    LIST_INIT (&vm_change_state_head);
#ifndef _WIN32
    {
        struct sigaction act;
        sigfillset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL);
    }
#else
    SetConsoleCtrlHandler(qemu_ctrl_handler, TRUE);
    /* Note: cpu_interrupt() is currently not SMP safe, so we force
       QEMU to run on a single CPU */
    {
        HANDLE h;
        DWORD mask, smask;
        int i;
        h = GetCurrentProcess();
        if (GetProcessAffinityMask(h, &mask, &smask)) {
            for(i = 0; i < 32; i++) {
                if (mask & (1 << i))
                    break;
            }
            if (i != 32) {
                mask = 1 << i;
                SetProcessAffinityMask(h, mask);
            }
        }
    }
#endif

    register_machines();
    machine = first_machine;
    initrd_filename = NULL;
    for(i = 0; i < MAX_FD; i++)
        fd_filename[i] = NULL;
    for(i = 0; i < MAX_DISKS; i++)
        hd_filename[i] = NULL;
    ram_size = DEFAULT_RAM_SIZE * 1024 * 1024;
    vga_ram_size = VGA_RAM_SIZE;
    bios_size = BIOS_SIZE;
#ifdef CONFIG_GDBSTUB
    use_gdbstub = 0;
    gdbstub_port = DEFAULT_GDBSTUB_PORT;
#endif
    snapshot = 0;
    nographic = 0;
    kernel_filename = NULL;
    kernel_cmdline = "";
#ifdef TARGET_PPC
    cdrom_index = 1;
#else
    cdrom_index = 2;
#endif
    cyls = heads = secs = 0;
    translation = BIOS_ATA_TRANSLATION_AUTO;
    pstrcpy(monitor_device, sizeof(monitor_device), "vc");

    pstrcpy(serial_devices[0], sizeof(serial_devices[0]), "vc");
    for(i = 1; i < MAX_SERIAL_PORTS; i++)
        serial_devices[i][0] = '\0';
    serial_device_index = 0;

    pstrcpy(parallel_devices[0], sizeof(parallel_devices[0]), "vc");
    for(i = 1; i < MAX_PARALLEL_PORTS; i++)
        parallel_devices[i][0] = '\0';
    parallel_device_index = 0;

    usb_devices_index = 0;

    nb_net_clients = 0;

    nb_nics = 0;
    /* default mac address of the first network interface */

    android_audio_enabled = 1;

    optind = 1;
    for(;;) {
        if (optind >= argc)
            break;
        r = argv[optind];
        if (r[0] != '-') {
            hd_filename[0] = argv[optind++];
        } else {
            const QEMUOption *popt;

            optind++;
            popt = qemu_options;
            for(;;) {
                if (!popt->name) {
                    fprintf(stderr, "%s: invalid option -- '%s'\n",
                            argv[0], r);
                    exit(1);
                }
                if (!strcmp(popt->name, r + 1))
                    break;
                popt++;
            }
            if (popt->flags & HAS_ARG) {
                if (optind >= argc) {
                    fprintf(stderr, "%s: option '%s' requires an argument\n",
                            argv[0], r);
                    exit(1);
                }
                optarg = argv[optind++];
            } else {
                optarg = NULL;
            }

            switch(popt->index) {
            case QEMU_OPTION_M:
                machine = find_machine(optarg);
                if (!machine) {
                    QEMUMachine *m;
                    printf("Supported machines are:\n");
                    for(m = first_machine; m != NULL; m = m->next) {
                        printf("%-10s %s%s\n",
                               m->name, m->desc,
                               m == first_machine ? " (default)" : "");
                    }
                    exit(1);
                }
                break;
            case QEMU_OPTION_initrd:
                initrd_filename = optarg;
                break;
            case QEMU_OPTION_hda:
            case QEMU_OPTION_hdb:
            case QEMU_OPTION_hdc:
            case QEMU_OPTION_hdd:
                {
                    int hd_index;
                    hd_index = popt->index - QEMU_OPTION_hda;
                    hd_filename[hd_index] = optarg;
                    if (hd_index == cdrom_index)
                        cdrom_index = -1;
                }
                break;
            case QEMU_OPTION_snapshot:
                snapshot = 1;
                break;
            case QEMU_OPTION_hdachs:
                {
                    const char *p;
                    p = optarg;
                    cyls = strtol(p, (char **)&p, 0);
                    if (cyls < 1 || cyls > 16383)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    heads = strtol(p, (char **)&p, 0);
                    if (heads < 1 || heads > 16)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    secs = strtol(p, (char **)&p, 0);
                    if (secs < 1 || secs > 63)
                        goto chs_fail;
                    if (*p == ',') {
                        p++;
                        if (!strcmp(p, "none"))
                            translation = BIOS_ATA_TRANSLATION_NONE;
                        else if (!strcmp(p, "lba"))
                            translation = BIOS_ATA_TRANSLATION_LBA;
                        else if (!strcmp(p, "auto"))
                            translation = BIOS_ATA_TRANSLATION_AUTO;
                        else
                            goto chs_fail;
                    } else if (*p != '\0') {
                    chs_fail:
                        fprintf(stderr, "qemu: invalid physical CHS format\n");
                        exit(1);
                    }
                }
                break;
            case QEMU_OPTION_nographic:
                pstrcpy(monitor_device, sizeof(monitor_device), "stdio");
                pstrcpy(serial_devices[0], sizeof(serial_devices[0]), "stdio");
                nographic = 1;
                break;
            case QEMU_OPTION_kernel:
                kernel_filename = optarg;
                break;
            case QEMU_OPTION_append:
                kernel_cmdline = optarg;
                break;
            case QEMU_OPTION_cdrom:
                if (cdrom_index >= 0) {
                    hd_filename[cdrom_index] = optarg;
                }
                break;
            case QEMU_OPTION_boot:
                boot_device = optarg[0];
                if (boot_device != 'a' &&
#ifdef TARGET_SPARC
		    // Network boot
		    boot_device != 'n' &&
#endif
                    boot_device != 'c' && boot_device != 'd') {
                    fprintf(stderr, "qemu: invalid boot device '%c'\n", boot_device);
                    exit(1);
                }
                break;
            case QEMU_OPTION_fda:
                fd_filename[0] = optarg;
                break;
            case QEMU_OPTION_fdb:
                fd_filename[1] = optarg;
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_no_fd_bootchk:
                fd_bootchk = 0;
                break;
#endif
            case QEMU_OPTION_no_code_copy:
                code_copy_enabled = 0;
                break;
            case QEMU_OPTION_net:
                if (nb_net_clients >= MAX_NET_CLIENTS) {
                    fprintf(stderr, "qemu: too many network clients\n");
                    exit(1);
                }
                pstrcpy(net_clients[nb_net_clients],
                        sizeof(net_clients[0]),
                        optarg);
                nb_net_clients++;
                break;
#ifdef CONFIG_SLIRP
            case QEMU_OPTION_tftp:
		tftp_prefix = optarg;
                break;
            case QEMU_OPTION_bootp:
                bootp_filename = optarg;
                break;
#if 0  /* ANDROID disabled */
            case QEMU_OPTION_smb:
		net_slirp_smb(optarg);
                break;
#endif
            case QEMU_OPTION_redir:
                if ( net_slirp_redir(optarg) < 0 ) {
                    fprintf(stderr, "qemu: could not set up redirection '%s'\n", optarg);
                }
                break;
#endif
#ifdef HAS_AUDIO
            case QEMU_OPTION_audio_help:
                AUD_help ();
                exit (0);
                break;
            case QEMU_OPTION_soundhw:
                select_soundhw (optarg);
                break;
#endif
            case QEMU_OPTION_h:
                fprintf(stderr, "qemu: -h option used\n");
                help();
                break;
            case QEMU_OPTION_m:
                ram_size = atoi(optarg) * 1024 * 1024;
                if (ram_size <= 0) {
                    fprintf(stderr, "qemu: invalid RAM size '%s'\n", optarg);
                    help();
                }
                if (ram_size > PHYS_RAM_MAX_SIZE) {
                    fprintf(stderr, "qemu: at most %d MB RAM can be simulated\n",
                            PHYS_RAM_MAX_SIZE / (1024 * 1024));
                    exit(1);
                }
                break;
            case QEMU_OPTION_d:
                {
                    int mask;
                    CPULogItem *item;

                    mask = cpu_str_to_log_mask(optarg);
                    if (!mask) {
                        printf("Log items (comma separated):\n");
                    for(item = cpu_log_items; item->mask != 0; item++) {
                        printf("%-10s %s\n", item->name, item->help);
                    }
                    exit(1);
                    }
                    cpu_set_log(mask);
                }
                break;
#ifdef CONFIG_GDBSTUB
            case QEMU_OPTION_s:
                use_gdbstub = 1;
                break;
            case QEMU_OPTION_p:
                gdbstub_port = atoi(optarg);
                break;
#endif
            case QEMU_OPTION_L:
                bios_dir = optarg;
                break;
            case QEMU_OPTION_S:
#if 1  /* ANDROID */
                fprintf(stderr, "Sorry, stopped launch is not supported in the Android emulator\n" );
                exit(1);
#else
                start_emulation = 0;
                break;
#endif
	    case QEMU_OPTION_k:
		keyboard_layout = optarg;
		break;
            case QEMU_OPTION_localtime:
                rtc_utc = 0;
                break;
            case QEMU_OPTION_g:
                {
                    const char *p;
                    int w, h, depth;
                    p = optarg;
                    w = strtol(p, (char **)&p, 10);
                    if (w <= 0) {
                    graphic_error:
                        fprintf(stderr, "qemu: invalid resolution or depth\n");
                        exit(1);
                    }
                    if (*p != 'x')
                        goto graphic_error;
                    p++;
                    h = strtol(p, (char **)&p, 10);
                    if (h <= 0)
                        goto graphic_error;
                    if (*p == 'x') {
                        p++;
                        depth = strtol(p, (char **)&p, 10);
                        if (depth != 8 && depth != 15 && depth != 16 &&
                            depth != 24 && depth != 32)
                            goto graphic_error;
                    } else if (*p == '\0') {
                        depth = graphic_depth;
                    } else {
                        goto graphic_error;
                    }

                    graphic_width = w;
                    graphic_height = h;
                    graphic_depth = depth;
                }
                break;
            case QEMU_OPTION_monitor:
                pstrcpy(monitor_device, sizeof(monitor_device), optarg);
                break;
            case QEMU_OPTION_serial:
                if (serial_device_index >= MAX_SERIAL_PORTS) {
                    fprintf(stderr, "qemu: too many serial ports\n");
                    exit(1);
                }
                pstrcpy(serial_devices[serial_device_index],
                        sizeof(serial_devices[0]), optarg);
                serial_device_index++;
                break;
            case QEMU_OPTION_parallel:
                if (parallel_device_index >= MAX_PARALLEL_PORTS) {
                    fprintf(stderr, "qemu: too many parallel ports\n");
                    exit(1);
                }
                pstrcpy(parallel_devices[parallel_device_index],
                        sizeof(parallel_devices[0]), optarg);
                parallel_device_index++;
                break;
	    case QEMU_OPTION_loadvm:
		loadvm = optarg;
		break;
            case QEMU_OPTION_full_screen:
                full_screen = 1;
                break;
            case QEMU_OPTION_pidfile:
                create_pidfile(optarg);
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_win2k_hack:
                win2k_install_hack = 1;
                break;
#endif
#ifdef USE_KQEMU
            case QEMU_OPTION_no_kqemu:
                kqemu_allowed = 0;
                break;
            case QEMU_OPTION_kernel_kqemu:
                kqemu_allowed = 2;
                break;
#endif
            case QEMU_OPTION_usb:
                usb_enabled = 1;
                break;
            case QEMU_OPTION_usbdevice:
                usb_enabled = 1;
                if (usb_devices_index >= MAX_USB_CMDLINE) {
                    fprintf(stderr, "Too many USB devices\n");
                    exit(1);
                }
                pstrcpy(usb_devices[usb_devices_index],
                        sizeof(usb_devices[usb_devices_index]),
                        optarg);
                usb_devices_index++;
                break;
            case QEMU_OPTION_smp:
                smp_cpus = atoi(optarg);
                if (smp_cpus < 1 || smp_cpus > MAX_CPUS) {
                    fprintf(stderr, "Invalid number of CPUs\n");
                    exit(1);
                }
                break;
	    case QEMU_OPTION_vnc:
		vnc_display = atoi(optarg);
		if (vnc_display < 0) {
		    fprintf(stderr, "Invalid VNC display\n");
		    exit(1);
		}
		break;
            case QEMU_OPTION_no_acpi:
                acpi_enabled = 0;
                break;
            case QEMU_OPTION_clock:
                qemu_configure_alarms(optarg);
                break;
            case QEMU_OPTION_noaudio:
                android_audio_enabled = 0;
                break;
            case QEMU_OPTION_mic:
                audio_input_source = (char*)optarg;
                break;
#ifdef CONFIG_TRACE
            case QEMU_OPTION_trace_file:
                trace_filename = optarg;
                tracing = 1;
                break;
            case QEMU_OPTION_trace_miss:
                trace_cache_miss = 1;
                break;
            case QEMU_OPTION_trace_addr:
                trace_all_addr = 1;
                break;
            case QEMU_OPTION_tracing:
                if (strcmp(optarg, "off") == 0)
                    tracing = 0;
                else if (strcmp(optarg, "on") == 0 && trace_filename)
                    tracing = 1;
                else {
                    fprintf(stderr, "Unexpected option to -tracing ('%s')\n",
                            optarg);
                    exit(1);
                }
                break;
            case QEMU_OPTION_dcache_load_miss:
                dcache_load_miss_penalty = atoi(optarg);
                break;
            case QEMU_OPTION_dcache_store_miss:
                dcache_store_miss_penalty = atoi(optarg);
                break;
#endif
#ifdef CONFIG_NAND
            case QEMU_OPTION_nand:
                nand_add_dev(optarg);
                break;
#endif
            }
        }
    }

#ifdef USE_KQEMU
    if (smp_cpus > 1)
        kqemu_allowed = 0;
#endif
    linux_boot = (kernel_filename != NULL);

    if (!linux_boot &&
        hd_filename[0] == '\0' &&
        (cdrom_index >= 0 && hd_filename[cdrom_index] == '\0') &&
        fd_filename[0] == '\0') {
        fprintf(stderr, "qemu: no kernel and no disk image specified\n");
        help();
    }
    /* boot to cd by default if no hard disk */
    if (hd_filename[0] == '\0' && boot_device == 'c') {
        if (fd_filename[0] != '\0')
            boot_device = 'a';
        else
            boot_device = 'd';
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    init_timers();
    qemu_init_alarm_timer( cpu_signal_alarm );

#ifndef _WIN32
    /* setup sigint handler */
    {
        struct sigaction user_act;

        sigfillset(&user_act.sa_mask);
        user_act.sa_flags = 0;
#if defined (TARGET_I386) && defined(USE_CODE_COPY)
        user_act.sa_flags |= SA_ONSTACK;
#endif
        user_act.sa_handler = sigint_handler;
        sigaction(SIGINT, &user_act, NULL);
    }
#endif

    socket_init();

    /* init network clients */
    if (nb_net_clients == 0) {
        /* if no clients, we use a default config */
        pstrcpy(net_clients[0], sizeof(net_clients[0]),
                "nic");
        pstrcpy(net_clients[1], sizeof(net_clients[0]),
                "user");
        nb_net_clients = 2;
    }

    for(i = 0;i < nb_net_clients; i++) {
        if (net_client_init(net_clients[i]) < 0)
            exit(1);
    }

    /* init the memory */
    phys_ram_size = ram_size + vga_ram_size + bios_size;

    phys_ram_base = qemu_vmalloc(phys_ram_size);
    if (!phys_ram_base) {
        fprintf(stderr, "Could not allocate physical memory\n");
        exit(1);
    }

    /* we always create the cdrom drive, even if no disk is there */
    bdrv_init();
    if (cdrom_index >= 0) {
        bs_table[cdrom_index] = bdrv_new("cdrom");
        bdrv_set_type_hint(bs_table[cdrom_index], BDRV_TYPE_CDROM);
    }

    /* open the virtual block devices */
    for(i = 0; i < MAX_DISKS; i++) {
        if (hd_filename[i]) {
            if (!bs_table[i]) {
                char buf[64];
                snprintf(buf, sizeof(buf), "hd%c", i + 'a');
                bs_table[i] = bdrv_new(buf);
            }
            if (bdrv_open(bs_table[i], hd_filename[i], snapshot) < 0) {
                fprintf(stderr, "qemu: could not open hard disk image '%s'\n",
                        hd_filename[i]);
                exit(1);
            }
            if (i == 0 && cyls != 0) {
                bdrv_set_geometry_hint(bs_table[i], cyls, heads, secs);
                bdrv_set_translation_hint(bs_table[i], translation);
            }
        }
    }

    /* we always create at least one floppy disk */
    fd_table[0] = bdrv_new("fda");
    bdrv_set_type_hint(fd_table[0], BDRV_TYPE_FLOPPY);

    for(i = 0; i < MAX_FD; i++) {
        if (fd_filename[i]) {
            if (!fd_table[i]) {
                char buf[64];
                snprintf(buf, sizeof(buf), "fd%c", i + 'a');
                fd_table[i] = bdrv_new(buf);
                bdrv_set_type_hint(fd_table[i], BDRV_TYPE_FLOPPY);
            }
            if (fd_filename[i] != '\0') {
                if (bdrv_open(fd_table[i], fd_filename[i], snapshot) < 0) {
                    fprintf(stderr, "qemu: could not open floppy disk image '%s'\n",
                            fd_filename[i]);
                    exit(1);
                }
            }
        }
    }

    register_savevm("timer", 0, 1, timer_save, timer_load, NULL);
    register_savevm("ram", 0, 1, ram_save, ram_load, NULL);

    init_ioports();

    /* terminal init */
    if (nographic) {
        dumb_display_init(ds);
    } else if (vnc_display != -1) {
	vnc_display_init(ds, vnc_display);
    } else {
#if defined(CONFIG_SDL)
        sdl_display_init(ds, full_screen);
#elif defined(CONFIG_COCOA)
        cocoa_display_init(ds, full_screen);
#else
        dumb_display_init(ds);
#endif
    }

    monitor_hd = qemu_chr_open(monitor_device);
    if (!monitor_hd) {
        fprintf(stderr, "qemu: could not open monitor device '%s'\n", monitor_device);
        exit(1);
    }
    monitor_init(monitor_hd, !nographic);

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        if (serial_devices[i][0] != '\0') {
            serial_hds[i] = qemu_chr_open(serial_devices[i]);
            if (!serial_hds[i]) {
                fprintf(stderr, "qemu: could not open serial device '%s'\n",
                        serial_devices[i]);
                exit(1);
            }
            if (!strcmp(serial_devices[i], "vc"))
                qemu_chr_printf(serial_hds[i], "serial%d console\r\n", i);
        }
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        if (parallel_devices[i][0] != '\0') {
            parallel_hds[i] = qemu_chr_open(parallel_devices[i]);
            if (!parallel_hds[i]) {
                fprintf(stderr, "qemu: could not open parallel device '%s'\n",
                        parallel_devices[i]);
                exit(1);
            }
            if (!strcmp(parallel_devices[i], "vc"))
                qemu_chr_printf(parallel_hds[i], "parallel%d console\r\n", i);
        }
    }

#ifdef CONFIG_TRACE
    if (trace_filename) {
        trace_init(trace_filename);
        qemu_trace_enable();
        qemu_trace_enable_gencode();
        qemu_trace_enable_dyngen();
        dcache_init(dcache_size, dcache_ways, dcache_line_size,
                    dcache_replace_policy, dcache_load_miss_penalty,
                    dcache_store_miss_penalty);
        fprintf(stderr, "-- When done tracing, exit the emulator. --\n");
    }
#endif

    machine->init(ram_size, vga_ram_size, boot_device,
                  ds, fd_filename, snapshot,
                  kernel_filename, kernel_cmdline, initrd_filename);

    /* init USB devices */
    if (usb_enabled) {
        for(i = 0; i < usb_devices_index; i++) {
            if (usb_device_add(usb_devices[i]) < 0) {
                fprintf(stderr, "Warning: could not add USB device %s\n",
                        usb_devices[i]);
            }
        }
    }

    gui_timer = qemu_new_timer(rt_clock, gui_update, NULL);
    qemu_mod_timer(gui_timer, qemu_get_clock(rt_clock));

#ifdef CONFIG_GDBSTUB
    if (use_gdbstub) {
        if (gdbserver_start(gdbstub_port) < 0) {
            fprintf(stderr, "Could not open gdbserver socket on port %d\n",
                    gdbstub_port);
            exit(1);
        } else {
            printf("Waiting gdb connection on port %d\n", gdbstub_port);
        }
    } else
#endif
    if (loadvm)
        qemu_loadvm(loadvm);

    /* call android-specific setup function */
    android_emulation_setup();
    qemu_start_alarm_timer();

    {
        /* XXX: simplify init */
        read_passwords();
        if (start_emulation) {
            vm_start();
        }
    }

    main_loop();
    qemu_stop_alarm_timer();

    android_emulation_teardown();
    return 0;
}
