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
#include "goldfish_nand_reg.h"
#include "goldfish_nand.h"
#include "android/utils/tempfile.h"
#include "qemu_debug.h"
#include "android/android.h"

#define  DEBUG  1
#if DEBUG
#  define  D(...)    VERBOSE_PRINT(init,__VA_ARGS__)
#  define  D_ACTIVE  VERBOSE_CHECK(init)
#  define  T(...)    VERBOSE_PRINT(nand_limits,__VA_ARGS__)
#  define  T_ACTIVE  VERBOSE_CHECK(nand_limits)
#else
#  define  D(...)    ((void)0)
#  define  D_ACTIVE  0
#  define  T(...)    ((void)0)
#  define  T_ACTIVE  0
#endif

/* lseek uses 64-bit offsets on Darwin. */
/* prefer lseek64 on Linux              */
#ifdef __APPLE__
#  define  llseek  lseek
#elif defined(__linux__)
#  define  llseek  lseek64
#endif

#define  XLOG  xlog

static void
xlog( const char*  format, ... )
{
    va_list  args;
    va_start(args, format);
    fprintf(stderr, "NAND: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

typedef struct {
    char*      devname;
    size_t     devname_len;
    char*      data;
    int        fd;
    uint32_t   flags;
    uint32_t   page_size;
    uint32_t   extra_size;
    uint32_t   erase_size;
    uint64_t   size;
} nand_dev;

nand_threshold    android_nand_write_threshold;
nand_threshold    android_nand_read_threshold;

#ifdef CONFIG_NAND_THRESHOLD

/* update a threshold, return 1 if limit is hit, 0 otherwise */
static void
nand_threshold_update( nand_threshold*  t, uint32_t  len )
{
    if (t->counter < t->limit) {
        uint64_t  avail = t->limit - t->counter;
        if (avail > len)
            avail = len;

        if (t->counter == 0) {
            T("%s: starting threshold counting to %lld", 
              __FUNCTION__, t->limit);
        }
        t->counter += avail;
        if (t->counter >= t->limit) {
            /* threshold reach, send a signal to an external process */
            T( "%s: sending signal %d to pid %d !", 
               __FUNCTION__, t->signal, t->pid );

            kill( t->pid, t->signal );
        }
    }
    return;
}

#define  NAND_UPDATE_READ_THRESHOLD(len)  \
    nand_threshold_update( &android_nand_read_threshold, (uint32_t)(len) )

#define  NAND_UPDATE_WRITE_THRESHOLD(len)  \
    nand_threshold_update( &android_nand_write_threshold, (uint32_t)(len) )

#else /* !NAND_THRESHOLD */

#define  NAND_UPDATE_READ_THRESHOLD(len)  \
    do {} while (0)

#define  NAND_UPDATE_WRITE_THRESHOLD(len)  \
    do {} while (0)

#endif /* !NAND_THRESHOLD */

static nand_dev *nand_devs = NULL;
static uint32_t nand_dev_count = 0;

typedef struct {
    uint32_t base;

    // register state
    uint32_t dev;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t transfer_size;
    uint32_t data;
    uint32_t result;
} nand_dev_state;

/* update this everytime you change the nand_dev_state structure */
#define  NAND_DEV_STATE_SAVE_VERSION  1

#define  QFIELD_STRUCT  nand_dev_state
QFIELD_BEGIN(nand_dev_state_fields)
    QFIELD_INT32(dev),
    QFIELD_INT32(addr_low),
    QFIELD_INT32(addr_high),
    QFIELD_INT32(transfer_size),
    QFIELD_INT32(data),
    QFIELD_INT32(result),
QFIELD_END

static void  nand_dev_state_save(QEMUFile*  f, void*  opaque)
{
    nand_dev_state*  s = opaque;

    qemu_put_struct(f, nand_dev_state_fields, s);
}

static int   nand_dev_state_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    nand_dev_state*  s = opaque;

    if (version_id != NAND_DEV_STATE_SAVE_VERSION)
        return -1;

    return qemu_get_struct(f, nand_dev_state_fields, s);
}


static int  do_read(int  fd, void*  buf, size_t  size)
{
    int  ret;
    do {
        ret = read(fd, buf, size);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

static int  do_write(int  fd, const void*  buf, size_t  size)
{
    int  ret;
    do {
        ret = write(fd, buf, size);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

static uint32_t nand_dev_read_file(nand_dev *dev, uint32_t data, uint64_t addr, uint32_t total_len)
{
    uint32_t len = total_len;
    size_t read_len = dev->erase_size;
    int eof = 0;

    NAND_UPDATE_READ_THRESHOLD(total_len);

    lseek(dev->fd, addr, SEEK_SET);
    while(len > 0) {
        if(read_len < dev->erase_size) {
            memset(dev->data, 0xff, dev->erase_size);
            read_len = dev->erase_size;
            eof = 1;
        }
        if(len < read_len)
            read_len = len;
        if(!eof) {
            read_len = do_read(dev->fd, dev->data, read_len);
        }
        cpu_memory_rw_debug(cpu_single_env, data, dev->data, read_len, 1);
        data += read_len;
        len -= read_len;
    }
    return total_len;
}

static uint32_t nand_dev_write_file(nand_dev *dev, uint32_t data, uint64_t addr, uint32_t total_len)
{
    uint32_t len = total_len;
    size_t write_len = dev->erase_size;
    int ret;

    NAND_UPDATE_WRITE_THRESHOLD(total_len);

    lseek(dev->fd, addr, SEEK_SET);
    while(len > 0) {
        if(len < write_len)
            write_len = len;
        cpu_memory_rw_debug(cpu_single_env, data, dev->data, write_len, 0);
        ret = do_write(dev->fd, dev->data, write_len);
        if(ret < write_len) {
            XLOG("nand_dev_write_file, write failed: %s\n", strerror(errno));
            break;
        }
        data += write_len;
        len -= write_len;
    }
    return total_len - len;
}

static uint32_t nand_dev_erase_file(nand_dev *dev, uint64_t addr, uint32_t total_len)
{
    uint32_t len = total_len;
    size_t write_len = dev->erase_size;
    int ret;

    lseek(dev->fd, addr, SEEK_SET);
    memset(dev->data, 0xff, dev->erase_size);
    while(len > 0) {
        if(len < write_len)
            write_len = len;
        ret = do_write(dev->fd, dev->data, write_len);
        if(ret < write_len) {
            XLOG( "nand_dev_write_file, write failed: %s\n", strerror(errno));
            break;
        }
        len -= write_len;
    }
    return total_len - len;
}

/* this is a huge hack required to make the PowerPC emulator binary usable
 * on Mac OS X. If you define this function as 'static', the emulated kernel
 * will panic when attempting to mount the /data partition.
 *
 * worse, if you do *not* define the function as static on Linux-x86, the
 * emulated kernel will also panic !?
 *
 * I still wonder if this is a compiler bug, or due to some nasty thing the
 * emulator does with CPU registers during execution of the translated code.
 */
#if !(defined __APPLE__ && defined __powerpc__)
static
#endif
uint32_t nand_dev_do_cmd(nand_dev_state *s, uint32_t cmd)
{
    uint32_t size;
    uint64_t addr;
    nand_dev *dev;

    addr = s->addr_low | ((uint64_t)s->addr_high << 32);
    size = s->transfer_size;
    if(s->dev >= nand_dev_count)
        return 0;
    dev = nand_devs + s->dev;

    switch(cmd) {
    case NAND_CMD_GET_DEV_NAME:
        if(size > dev->devname_len)
            size = dev->devname_len;
        cpu_memory_rw_debug(cpu_single_env, s->data, dev->devname, size, 1);
        return size;
    case NAND_CMD_READ:
        if(addr >= dev->size)
            return 0;
        if(size + addr > dev->size)
            size = dev->size - addr;
        if(dev->fd >= 0)
            return nand_dev_read_file(dev, s->data, addr, size);
        cpu_memory_rw_debug(cpu_single_env,s->data, &dev->data[addr], size, 1);
        return size;
    case NAND_CMD_WRITE:
        if(dev->flags & NAND_DEV_FLAG_READ_ONLY)
            return 0;
        if(addr >= dev->size)
            return 0;
        if(size + addr > dev->size)
            size = dev->size - addr;
        if(dev->fd >= 0)
            return nand_dev_write_file(dev, s->data, addr, size);
        cpu_memory_rw_debug(cpu_single_env,s->data, &dev->data[addr], size, 0);
        return size;
    case NAND_CMD_ERASE:
        if(dev->flags & NAND_DEV_FLAG_READ_ONLY)
            return 0;
        if(addr >= dev->size)
            return 0;
        if(size + addr > dev->size)
            size = dev->size - addr;
        if(dev->fd >= 0)
            return nand_dev_erase_file(dev, addr, size);
        memset(&dev->data[addr], 0xff, size);
        return size;
    case NAND_CMD_BLOCK_BAD_GET: // no bad block support
        return 0;
    case NAND_CMD_BLOCK_BAD_SET:
        if(dev->flags & NAND_DEV_FLAG_READ_ONLY)
            return 0;
        return 0;
    default:
        cpu_abort(cpu_single_env, "nand_dev_do_cmd: Bad command %x\n", cmd);
        return 0;
    }
}

/* I/O write */
static void nand_dev_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    nand_dev_state *s = (nand_dev_state *)opaque;

    switch (offset) {
    case NAND_DEV:
        s->dev = value;
        if(s->dev >= nand_dev_count) {
            cpu_abort(cpu_single_env, "nand_dev_write: Bad dev %x\n", value);
        }
        break;
    case NAND_ADDR_HIGH:
        s->addr_high = value;
        break;
    case NAND_ADDR_LOW:
        s->addr_low = value;
        break;
    case NAND_TRANSFER_SIZE:
        s->transfer_size = value;
        break;
    case NAND_DATA:
        s->data = value;
        break;
    case NAND_COMMAND:
        s->result = nand_dev_do_cmd(s, value);
        break;
    default:
        cpu_abort(cpu_single_env, "nand_dev_write: Bad offset %x\n", offset);
        break;
    }
}

/* I/O read */
static uint32_t nand_dev_read(void *opaque, target_phys_addr_t offset)
{
    nand_dev_state *s = (nand_dev_state *)opaque;
    nand_dev *dev;

    switch (offset) {
    case NAND_VERSION:
        return NAND_VERSION_CURRENT;
    case NAND_NUM_DEV:
        return nand_dev_count;
    case NAND_RESULT:
        return s->result;
    }

    if(s->dev >= nand_dev_count)
        return 0;

    dev = nand_devs + s->dev;

    switch (offset) {
    case NAND_DEV_FLAGS:
        return dev->flags;

    case NAND_DEV_NAME_LEN:
        return dev->devname_len;

    case NAND_DEV_PAGE_SIZE:
        return dev->page_size;

    case NAND_DEV_EXTRA_SIZE:
        return dev->extra_size;

    case NAND_DEV_ERASE_SIZE:
        return dev->erase_size;

    case NAND_DEV_SIZE_LOW:
        return (uint32_t)dev->size;

    case NAND_DEV_SIZE_HIGH:
        return (uint32_t)(dev->size >> 32);

    default:
        cpu_abort(cpu_single_env, "nand_dev_read: Bad offset %x\n", offset);
        return 0;
    }
}

static CPUReadMemoryFunc *nand_dev_readfn[] = {
   nand_dev_read,
   nand_dev_read,
   nand_dev_read
};

static CPUWriteMemoryFunc *nand_dev_writefn[] = {
   nand_dev_write,
   nand_dev_write,
   nand_dev_write
};

/* initialize the QFB device */
void nand_dev_init(uint32_t base)
{
    int iomemtype;
    static int  instance_id = 0;
    nand_dev_state *s;

    s = (nand_dev_state *)qemu_mallocz(sizeof(nand_dev_state));
    iomemtype = cpu_register_io_memory(nand_dev_readfn, nand_dev_writefn, s);
    cpu_register_physical_memory(base, 0x00000fff, iomemtype);
    s->base = base;

    register_savevm( "nand_dev", instance_id++, NAND_DEV_STATE_SAVE_VERSION,
                      nand_dev_state_save, nand_dev_state_load, s);
}

static int arg_match(const char *a, const char *b, size_t b_len)
{
    while(*a && b_len--) {
        if(*a++ != *b++)
            return 0;
    }
    return b_len == 0;
}

void nand_add_dev(const char *arg)
{
    uint64_t dev_size = 0;
    const char *next_arg;
    const char *value;
    size_t arg_len, value_len;
    nand_dev *new_devs, *dev;
    char *devname = NULL;
    size_t devname_len = 0;
    char *initfilename = NULL;
    char *rwfilename = NULL;
    int initfd = -1;
    int rwfd = -1;
    int read_only = 0;
    int pad;
    ssize_t read_size;
    uint32_t page_size = 2048;
    uint32_t extra_size = 64;
    uint32_t erase_pages = 64;

    while(arg) {
        next_arg = strchr(arg, ',');
        value = strchr(arg, '=');
        if(next_arg != NULL) {
            arg_len = next_arg - arg;
            next_arg++;
            if(value >= next_arg)
                value = NULL;
        }
        else
            arg_len = strlen(arg);
        if(value != NULL) {
            size_t new_arg_len = value - arg;
            value_len = arg_len - new_arg_len - 1;
            arg_len = new_arg_len;
            value++;
        }
        else
            value_len = 0;

        if(devname == NULL) {
            if(value != NULL)
                goto bad_arg_and_value;
            devname_len = arg_len;
            devname = malloc(arg_len);
            if(devname == NULL)
                goto out_of_memory;
            memcpy(devname, arg, arg_len);
        }
        else if(value == NULL) {
            if(arg_match("readonly", arg, arg_len)) {
                read_only = 1;
            }
            else {
                XLOG("bad arg: %.*s\n", arg_len, arg);
                exit(1);
            }
        }
        else {
            if(arg_match("size", arg, arg_len)) {
                char *ep;
                dev_size = strtoull(value, &ep, 0);
                if(ep != value + value_len)
                    goto bad_arg_and_value;
            }
            else if(arg_match("pagesize", arg, arg_len)) {
                char *ep;
                page_size = strtoul(value, &ep, 0);
                if(ep != value + value_len)
                    goto bad_arg_and_value;
            }
            else if(arg_match("extrasize", arg, arg_len)) {
                char *ep;
                extra_size = strtoul(value, &ep, 0);
                if(ep != value + value_len)
                    goto bad_arg_and_value;
            }
            else if(arg_match("erasepages", arg, arg_len)) {
                char *ep;
                erase_pages = strtoul(value, &ep, 0);
                if(ep != value + value_len)
                    goto bad_arg_and_value;
            }
            else if(arg_match("initfile", arg, arg_len)) {
                initfilename = malloc(value_len + 1);
                if(initfilename == NULL)
                    goto out_of_memory;
                memcpy(initfilename, value, value_len);
                initfilename[value_len] = '\0';
            }
            else if(arg_match("file", arg, arg_len)) {
                rwfilename = malloc(value_len + 1);
                if(rwfilename == NULL)
                    goto out_of_memory;
                memcpy(rwfilename, value, value_len);
                rwfilename[value_len] = '\0';
            }
            else {
                goto bad_arg_and_value;
            }
        }

        arg = next_arg;
    }

    if (rwfilename == NULL) {
        /* we create a temporary file to store everything */
        TempFile*    tmp = tempfile_create();

        if (tmp == NULL) {
            XLOG("could not create temp file for %.*s NAND disk image: %s",
                  devname_len, devname, strerror(errno));
            exit(1);
        }
        rwfilename = (char*) tempfile_path(tmp);
        if (VERBOSE_CHECK(init))
            dprint( "mapping '%.*s' NAND image to %s", devname_len, devname, rwfilename);
    }

    if(rwfilename) {
        rwfd = open(rwfilename, O_BINARY | (read_only ? O_RDONLY : O_RDWR));
        if(rwfd < 0 && read_only) {
            XLOG("could not open file %s, %s\n", rwfilename, strerror(errno));
            exit(1);
        }
        /* this could be a writable temporary file. use atexit_close_fd to ensure
         * that it is properly cleaned up at exit on Win32
         */
        if (!read_only)
            atexit_close_fd(rwfd);
    }

    if(initfilename) {
        initfd = open(initfilename, O_BINARY | O_RDONLY);
        if(initfd < 0) {
            XLOG("could not open file %s, %s\n", initfilename, strerror(errno));
            exit(1);
        }
        if(dev_size == 0) {
            dev_size = lseek(initfd, 0, SEEK_END);
            lseek(initfd, 0, SEEK_SET);
        }
    }

    new_devs = realloc(nand_devs, sizeof(nand_devs[0]) * (nand_dev_count + 1));
    if(new_devs == NULL)
        goto out_of_memory;
    nand_devs = new_devs;
    dev = &new_devs[nand_dev_count];

    dev->page_size = page_size;
    dev->extra_size = extra_size;
    dev->erase_size = erase_pages * (page_size + extra_size);
    pad = dev_size % dev->erase_size;
    if (pad != 0) {
        dev_size += (dev->erase_size - pad);
        D("rounding devsize up to a full eraseunit, now %llx\n", dev_size);
    }
    dev->devname = devname;
    dev->devname_len = devname_len;
    dev->size = dev_size;
    dev->data = malloc(dev->erase_size);
    if(dev->data == NULL)
        goto out_of_memory;
    dev->flags = read_only ? NAND_DEV_FLAG_READ_ONLY : 0;

    if (initfd >= 0) {
        do {
            read_size = do_read(initfd, dev->data, dev->erase_size);
            if(read_size < 0) {
                XLOG("could not read file %s, %s\n", initfilename, strerror(errno));
                exit(1);
            }
            if(do_write(rwfd, dev->data, read_size) != read_size) {
                XLOG("could not write file %s, %s\n", initfilename, strerror(errno));
                exit(1);
            }
        } while(read_size == dev->erase_size);
        close(initfd);
    }
    dev->fd = rwfd;

    nand_dev_count++;

    return;

out_of_memory:
    XLOG("out of memory\n");
    exit(1);

bad_arg_and_value:
    XLOG("bad arg: %.*s=%.*s\n", arg_len, arg, value_len, value);
    exit(1);
}

