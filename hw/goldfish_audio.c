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
#include "goldfish_device.h"
#include "audio/audio.h"
#include "qemu_debug.h"
#include "android/globals.h"

#define  DEBUG  1

#if DEBUG
#  define  D(...)  VERBOSE_PRINT(audio,__VA_ARGS__)
#else
#  define  D(...)  ((void)0)
#endif

extern void  dprint(const char*  fmt, ...);

/* define USE_QEMU_AUDIO_IN to 1 to use QEMU's audio subsystem to
 * implement the audio input. if 0, this will try to read a .wav file
 * directly...
 */
#define  USE_QEMU_AUDIO_IN  1

enum {
	/* audio status register */
	AUDIO_INT_STATUS	= 0x00,
	/* set this to enable IRQ */
	AUDIO_INT_ENABLE	= 0x04,
	/* set these to specify buffer addresses */
	AUDIO_SET_WRITE_BUFFER_1 = 0x08,
	AUDIO_SET_WRITE_BUFFER_2 = 0x0C,
	/* set number of bytes in buffer to write */
	AUDIO_WRITE_BUFFER_1  = 0x10,
	AUDIO_WRITE_BUFFER_2  = 0x14,

	/* true if audio input is supported */
	AUDIO_READ_SUPPORTED = 0x18,
	/* buffer to use for audio input */
	AUDIO_SET_READ_BUFFER = 0x1C,

	/* driver writes number of bytes to read */
	AUDIO_START_READ  = 0x20,

	/* number of bytes available in read buffer */
	AUDIO_READ_BUFFER_AVAILABLE  = 0x24,

	/* AUDIO_INT_STATUS bits */

	/* this bit set when it is safe to write more bytes to the buffer */
	AUDIO_INT_WRITE_BUFFER_1_EMPTY	= 1U << 0,
	AUDIO_INT_WRITE_BUFFER_2_EMPTY	= 1U << 1,
	AUDIO_INT_READ_BUFFER_FULL      = 1U << 2,
};


struct goldfish_audio_state {
    struct goldfish_device dev;
    // pointers to our two write buffers
    uint32_t buffer_1, buffer_2;
    uint32_t read_buffer;
    // buffer flags
    uint32_t int_status;
    // irq enable mask for int_status
    uint32_t int_enable;

#if USE_QEMU_AUDIO_IN
    uint32_t  read_pos;
    uint32_t  read_size;
#else
    // path to file or device to use for input
    const char* input_source;
    // true if input is a wav file
    int input_is_wav;
    // true if we need to convert stereo -> mono
    int input_is_stereo;
    // file descriptor to use for input
    int input_fd;
#endif

    // number of bytes available in the read buffer
    int read_buffer_available;

    // set to 1 or 2 to indicate which buffer we are writing from, or zero if both buffers are empty
    int current_buffer;

    // current data to write
    uint8* data_1;
    uint32_t data_1_length;
    uint8* data_2;
    uint32_t data_2_length;


    // for QEMU sound output
    QEMUSoundCard card;
    SWVoiceOut *voice;
#if USE_QEMU_AUDIO_IN
    SWVoiceIn*  voicein;
#endif
};

/* update this whenever you change the goldfish_audio_state structure */
#define  AUDIO_STATE_SAVE_VERSION  1

#define  QFIELD_STRUCT   struct goldfish_audio_state
QFIELD_BEGIN(audio_state_fields)
    QFIELD_INT32(buffer_1),
    QFIELD_INT32(buffer_2),
    QFIELD_INT32(read_buffer),
    QFIELD_INT32(int_status),
    QFIELD_INT32(int_enable),
#if USE_QEMU_AUDIO_IN
    QFIELD_INT32(read_pos),
    QFIELD_INT32(read_size),
#endif
    QFIELD_INT32(read_buffer_available),
    QFIELD_INT32(current_buffer),
    QFIELD_INT32(data_1_length),
    QFIELD_INT32(data_2_length),
QFIELD_END

static void  audio_state_save( QEMUFile*  f, void* opaque )
{
    struct goldfish_audio_state*  s = opaque;

    qemu_put_struct(f, audio_state_fields, s);

    /* we can't write data_1 and data_2 directly */
    qemu_put_be32( f, s->data_1 - phys_ram_base );
    qemu_put_be32( f, s->data_2 - phys_ram_base );
}

static int   audio_state_load( QEMUFile*  f, void*  opaque, int  version_id )
{
    struct goldfish_audio_state*  s = opaque;
    int                           ret;

    if (version_id != AUDIO_STATE_SAVE_VERSION)
        return -1;

    ret = qemu_get_struct(f, audio_state_fields, s);
    if (!ret) {
        s->data_1 = qemu_get_be32(f) + phys_ram_base;
        s->data_2 = qemu_get_be32(f) + phys_ram_base;
    }
    return -1;
}

static void enable_audio(struct goldfish_audio_state *s, int enable)
{
    // enable or disable the output voice
    if (s->voice != NULL)
        AUD_set_active_out(s->voice,   (enable & (AUDIO_INT_WRITE_BUFFER_1_EMPTY | AUDIO_INT_WRITE_BUFFER_2_EMPTY)) != 0);

    if (s->voicein)
        AUD_set_active_in (s->voicein, (enable & AUDIO_INT_READ_BUFFER_FULL) != 0);
    // reset buffer information
    s->data_1_length = 0;
    s->data_2_length = 0;
    s->current_buffer = 0;
    s->read_pos = 0;
}

#if USE_QEMU_AUDIO_IN
static void start_read(struct goldfish_audio_state *s, uint32_t count)
{
    //printf( "... goldfish audio start_read, count=%d\n", count );
    s->read_size = count;
    s->read_buffer_available = 0;
    s->read_pos  = 0;
}
#else
static void start_read(struct goldfish_audio_state *s, uint32_t count)
{
    uint8   wav_header[44];
    int result;

    if (!s->input_source) return;

    if (s->input_fd < 0) {
        s->input_fd = open(s->input_source, O_BINARY | O_RDONLY);

        if (s->input_fd < 0) {
            fprintf(stderr, "goldfish_audio could not open %s for audio input\n", s->input_source);
            s->input_source = NULL; // set to to avoid endless retries
            return;
        }

        // skip WAV header if we have a WAV file
        if (s->input_is_wav) {
            if (read(s->input_fd, wav_header, sizeof(wav_header)) != sizeof(wav_header)) {
                fprintf(stderr, "goldfish_audio could not read WAV file header %s\n", s->input_source);
                s->input_fd = -1;
                s->input_source = NULL; // set to to avoid endless retries
                return;
            }

            // is the WAV file stereo?
            s->input_is_stereo = (wav_header[22] == 2);
        } else {
            // assume input from an audio device is stereo
            s->input_is_stereo = 1;
        }
    }

    uint8* buffer = (uint8*)phys_ram_base + s->read_buffer;
    if (s->input_is_stereo) {
        // need to read twice as much data
        count *= 2;
    }

try_again:
    result = read(s->input_fd, buffer, count);
    if (result == 0 && s->input_is_wav) {
        // end of file, so seek back to the beginning
       lseek(s->input_fd, sizeof(wav_header), SEEK_SET);
       goto try_again;
    }

    if (result > 0 && s->input_is_stereo) {
        // we need to convert stereo to mono
        uint8* src  = (uint8*)buffer;
        uint8* dest = src;
        int count = result/2;
        while (count-- > 0) {
            int  sample1 = src[0] | (src[1] << 8);
            int  sample2 = src[2] | (src[3] << 8);
            int  sample  = (sample1 + sample2) >> 1;
            dst[0] = (uint8_t) sample;
            dst[1] = (uint8_t)(sample >> 8);
            src   += 4;
            dst   += 2;
        }

        // we reduced the number of bytes by 2
        result /= 2;
    }

    s->read_buffer_available = (result > 0 ? result : 0);
    s->int_status |= AUDIO_INT_READ_BUFFER_FULL;
    goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
}
#endif

static uint32_t goldfish_audio_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t ret;
    struct goldfish_audio_state *s = opaque;
    offset -= s->dev.base;
    switch(offset) {
        case AUDIO_INT_STATUS:
            // return current buffer status flags
            ret = s->int_status & s->int_enable;
            if(ret) {
                goldfish_device_set_irq(&s->dev, 0, 0);
            }
            return ret;

	case AUDIO_READ_SUPPORTED:
#if USE_QEMU_AUDIO_IN
            D("%s: AUDIO_READ_SUPPORTED returns %d", __FUNCTION__,
              (s->voicein != NULL));
            return (s->voicein != NULL);
#else
            return (s->input_source ? 1 : 0);
#endif

	case AUDIO_READ_BUFFER_AVAILABLE:
            D("%s: AUDIO_READ_BUFFER_AVAILABLE returns %d", __FUNCTION__,
               s->read_buffer_available);
	    return s->read_buffer_available;

        default:
            cpu_abort (cpu_single_env, "goldfish_audio_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_audio_write(void *opaque, target_phys_addr_t offset, uint32_t val)
{
    struct goldfish_audio_state *s = opaque;
    offset -= s->dev.base;

    switch(offset) {
        case AUDIO_INT_ENABLE:
            /* enable buffer empty interrupts */
            D("%s: AUDIO_INT_ENABLE %d", __FUNCTION__, val );
            enable_audio(s, val);
            s->int_enable = val;
            s->int_status = (AUDIO_INT_WRITE_BUFFER_1_EMPTY | AUDIO_INT_WRITE_BUFFER_2_EMPTY);
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            break;
        case AUDIO_SET_WRITE_BUFFER_1:
            /* save pointer to buffer 1 */
            s->buffer_1 = val;
            break;
        case AUDIO_SET_WRITE_BUFFER_2:
            /* save pointer to buffer 2 */
            s->buffer_2 = val;
            break;
        case AUDIO_WRITE_BUFFER_1:
            /* record that data in buffer 1 is ready to write */
            if (s->current_buffer == 0) s->current_buffer = 1;
            s->data_1 = phys_ram_base + s->buffer_1;
            s->data_1_length = val;
            s->int_status &= ~AUDIO_INT_WRITE_BUFFER_1_EMPTY;
            break;
        case AUDIO_WRITE_BUFFER_2:
            /* record that data in buffer 2 is ready to write */
            if (s->current_buffer == 0) s->current_buffer = 2;
            s->data_2 = phys_ram_base + s->buffer_2;
            s->data_2_length = val;
            s->int_status &= ~AUDIO_INT_WRITE_BUFFER_2_EMPTY;
            break;

	case AUDIO_SET_READ_BUFFER:
            /* save pointer to the read buffer */
            s->read_buffer = val;
            D( "%s: AUDIO_SET_READ_BUFFER %p", __FUNCTION__, (void*)val );
            break;

        case AUDIO_START_READ:
            D( "%s: AUDIO_START_READ %d", __FUNCTION__, val );
            start_read(s, val);
            s->int_status &= ~AUDIO_INT_READ_BUFFER_FULL;
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            break;

        default:
            cpu_abort (cpu_single_env, "goldfish_audio_write: Bad offset %x\n", offset);
    }
}

static void goldfish_audio_callback(void *opaque, int free)
{
    struct goldfish_audio_state *s = opaque;
    int new_status = 0;

    /* loop until free is zero or both buffers are empty */
    while (free && s->current_buffer) {

        /* write data in buffer 1 */
        while (free && s->current_buffer == 1) {
            int write = s->data_1_length;
            if (write > free) write = free;

            int written = AUD_write(s->voice, s->data_1, write);
            if (written) {
                D("%s: sent %d bytes to audio output", __FUNCTION__, write);
                s->data_1 += written;
                s->data_1_length -= written;
                free -= written;

                if (s->data_1_length == 0) {
                    new_status |= AUDIO_INT_WRITE_BUFFER_1_EMPTY;
                    s->current_buffer = (s->data_2_length ? 2 : 0);
                }
            } else {
                break;
            }
        }

        /* write data in buffer 2 */
        while (free && s->current_buffer == 2) {
            int write = s->data_2_length;
            if (write > free) write = free;

            int written = AUD_write(s->voice, s->data_2, write);
            if (written) {
                D("%s: sent %d bytes to audio output", __FUNCTION__, write);
                s->data_2 += written;
                s->data_2_length -= written;
                free -= written;

                if (s->data_2_length == 0) {
                    new_status |= AUDIO_INT_WRITE_BUFFER_2_EMPTY;
                    s->current_buffer = (s->data_1_length ? 1 : 0);
                }
            } else {
                break;
            }
        }
    }

    if (new_status && new_status != s->int_status) {
        s->int_status |= new_status;
        goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
    }
}

#if USE_QEMU_AUDIO_IN
static void
goldfish_audio_in_callback(void *opaque, int avail)
{
    struct goldfish_audio_state *s = opaque;
    int new_status = 0;

    if (s->read_pos >= s->read_size)
        return;

    if (0 && s->read_size > 0)
        D("%s: in %d (pos=%d size=%d)", __FUNCTION__,
           avail, s->read_pos, s->read_size );

    while (avail > 0) {
        int     pos     = s->read_pos;
        int     missing = s->read_size - pos;
        uint8*  buffer  = (uint8*)phys_ram_base + s->read_buffer + pos;
        int     read;
        int     avail2 = (avail > missing) ? missing : avail;

        read = AUD_read(s->voicein, buffer, avail2);
        if (read == 0)
            break;

        if (avail2 > 0)
            D("%s: AUD_read(%d) returned %d", __FUNCTION__, avail2, read);

        s->read_buffer_available += read;

        avail -= read;
        pos   += read;
        if (pos == s->read_size) {
            new_status |= AUDIO_INT_READ_BUFFER_FULL;
            D("%s: AUDIO_INT_READ_BUFFER_FULL available=%d", __FUNCTION__, s->read_buffer_available);
        }
        s->read_pos = pos;
    }

    if (new_status && new_status != s->int_status) {
        s->int_status |= new_status;
        goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
    }
}
#endif /* USE_QEMU_AUDIO_IN */

static CPUReadMemoryFunc *goldfish_audio_readfn[] = {
   goldfish_audio_read,
   goldfish_audio_read,
   goldfish_audio_read
};

static CPUWriteMemoryFunc *goldfish_audio_writefn[] = {
   goldfish_audio_write,
   goldfish_audio_write,
   goldfish_audio_write
};

void goldfish_audio_init(uint32_t base, int id, const char* input_source)
{
    struct goldfish_audio_state *s;
    audsettings_t as;

    /* nothing to do if no audio input and output */
    if (!android_hw->hw_audioOutput && !android_hw->hw_audioInput)
        return;

    s = (struct goldfish_audio_state *)qemu_mallocz(sizeof(*s));
    s->dev.name = "goldfish_audio";
    s->dev.id = id;
    s->dev.base = base;
    s->dev.size = 0x1000;
    s->dev.irq_count = 1;

#ifndef USE_QEMU_AUDIO_IN
    s->input_fd = -1;
    if (input_source) {
        s->input_source = input_source;
        char* extension = strrchr(input_source, '.');
        if (extension && strcasecmp(extension, ".wav") == 0) {
            s->input_is_wav = 1;
        }
     }
#endif

    AUD_register_card( &glob_audio_state, "goldfish_audio", &s->card);

    as.freq = 44100;
    as.nchannels = 2;
    as.fmt = AUD_FMT_S16;
    as.endianness = AUDIO_HOST_ENDIANNESS;

    if (android_hw->hw_audioOutput) {
        s->voice = AUD_open_out (
            &s->card,
            s->voice,
            "goldfish_audio",
            s,
            goldfish_audio_callback,
            &as
            );
        if (!s->voice) {
            dprint("warning: opening audio output failed\n");
            return;
        }
    }

#if USE_QEMU_AUDIO_IN
    as.freq       = 8000;
    as.nchannels  = 1;
    as.fmt        = AUD_FMT_S16;
    as.endianness = AUDIO_HOST_ENDIANNESS;

    if (android_hw->hw_audioInput) {
        s->voicein = AUD_open_in (
            &s->card,
            NULL,
            "goldfish_audio_in",
            s,
            goldfish_audio_in_callback,
            &as
            );
        if (!s->voicein) {
            dprint("warning: opening audio input failed\n");
        }
    }
#endif

    goldfish_device_add(&s->dev, goldfish_audio_readfn, goldfish_audio_writefn, s);

    register_savevm( "audio_state", 0, AUDIO_STATE_SAVE_VERSION,
                     audio_state_save, audio_state_load, s );
}

