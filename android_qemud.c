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
#include "android_qemud.h"
#include "android_debug.h"
#include "charpipe.h"
#include "cbuffer.h"
#include "android_utils.h"

#define  D(...)    VERBOSE_PRINT(qemud,__VA_ARGS__)
#define  D_ACTIVE  VERBOSE_CHECK(qemud)

/* the T(...) macro is used to dump traffic */
#define  T_ACTIVE   0

#if T_ACTIVE
#define  T(...)    VERBOSE_PRINT(qemud,__VA_ARGS__)
#else
#define  T(...)    ((void)0)
#endif

#define  MAX_PAYLOAD   4000
#define  MAX_CHANNELS  8

#define  CHANNEL_CONTROL_INDEX 0

/** utilities
 **/
static int
hexdigit( int  c )
{
    unsigned  d;

    d = (unsigned)(c - '0');
    if (d < 10) return d;

    d = (unsigned)(c - 'a');
    if (d < 6) return d+10;

    d = (unsigned)(c - 'A');
    if (d < 6) return d+10;

    return -1;
}

static int
hex2int( const uint8_t*  hex, int  len )
{
    int  result = 0;
    while (len > 0) {
        int  c = hexdigit(*hex++);
        if (c < 0)
            return -1;

        result = (result << 4) | c;
        len --;
    }
    return result;
}

static void
int2hex( uint8_t*  hex, int  len, int  val )
{
    static const uint8_t  hexchars[16] = "0123456789abcdef";
    while ( --len >= 0 )
        *hex++ = hexchars[(val >> (len*4)) & 15];
}

/** packets
 **/
#define  HEADER_SIZE  6

typedef struct Packet {
    struct Packet*  next;
    int             len;
    uint8_t         header[HEADER_SIZE];
    uint8_t         data[MAX_PAYLOAD];
} Packet;

static Packet*  _free_packets;

static void
packet_free( Packet*  p )
{
    p->next       = _free_packets;
    _free_packets = p;
}

static Packet*
packet_alloc( void )
{
    Packet*  p = _free_packets;
    if (p != NULL) {
        _free_packets = p->next;
    } else {
        p = malloc(sizeof(*p));
        if (p == NULL) {
            derror("%s: not enough memory", __FUNCTION__);
            exit(1);
        }
    }
    p->next = NULL;
    p->len  = 0;
    return p;
}

/** channels
 **/
typedef void (*EnqueueFunc)( void*  user, Packet*  p );

typedef struct {
    const char*      name;
    int              index;
    CharDriverState* cs;
    EnqueueFunc      enq_func;
    void*            enq_user;
} Channel;


static int
channel_can_read( void*  opaque )
{
    Channel*  c = opaque;

    return c->index < 0 ? 0 : MAX_PAYLOAD;
}


/* here, the data comes from the emulated device (e.g. GSM modem) through
 * a charpipe, we simply need to send it through the multiplexer */
static void
channel_read( void* opaque, const uint8_t*  from, int  len )
{
    Channel*  c = opaque;

    if (c->enq_func != NULL) {
        Packet*   p = packet_alloc();

        if (len > MAX_PAYLOAD)
            len = MAX_PAYLOAD;

        memcpy( p->data, from, len );
        p->len = len + HEADER_SIZE;
        int2hex( p->header+0, 4, len );
        int2hex( p->header+4, 2, c->index );

        c->enq_func( c->enq_user, p );
    }
    else
    {
        D("%s: discarding %d bytes for channel '%s'",
          __FUNCTION__, len, c->name);
    }
}

static void
channel_init( Channel*  c, const char*  name, CharDriverState* peer_cs )
{
    c->name     = name;
    c->index    = -1;
    c->enq_func = NULL;
    c->enq_user = NULL;
    c->cs       = peer_cs;
}


static void
channel_set_peer( Channel*  c, int  index, EnqueueFunc  enq_func, void*  enq_user )
{
    c->index = index;
    qemu_chr_add_read_handler( c->cs,
                               channel_can_read,
                               channel_read,
                               c );
    c->enq_func = enq_func;
    c->enq_user = enq_user;
}


static int
channel_write( Channel*c , const uint8_t*  buf, int  len )
{
    return qemu_chr_write( c->cs, buf, len );
}

/** multiplexer
 **/
#define  IN_BUFF_SIZE  (2*MAX_PAYLOAD)

typedef struct {
    CharDriverState*  cs;

    CBuffer  in_cbuffer[1];
    int      in_datalen;
    int      in_channel;

    int      count;
    Channel  channels[MAX_CHANNELS];
    uint8_t  in_buff[ IN_BUFF_SIZE + HEADER_SIZE ];
} Multiplexer;


/* called by channel_read when data comes from an emulated
 * device, and needs to be multiplexed through the serial
 * port
 */
static void
multiplexer_enqueue( Multiplexer*  m, Packet*  p )
{
    T("%s: sending %d bytes: '%s'", __FUNCTION__,
       p->len - HEADER_SIZE, quote_bytes( p->data, p->len - HEADER_SIZE ) );

    qemu_chr_write( m->cs, p->header, HEADER_SIZE );
    qemu_chr_write( m->cs, p->data, p->len - HEADER_SIZE );
    packet_free(p);
}

/* called when we received a channel registration from the
 * qemud daemon
 */
static void
multiplexer_register_channel( Multiplexer*  m,
                              const char*   name,
                              int           index )
{
    Channel*  c = m->channels;
    Channel*  c_end = c + m->count;

    for ( ; c < c_end; c++ ) {
        if ( !strcmp(c->name, name) )
            break;
    }

    if (c >= c_end) {
        D( "%s: unknown channel name '%s'",
            __FUNCTION__, name );
        return;
    }

    if (c->index >= 0) {
        D( "%s: channel '%s' re-assigned index %d",
            __FUNCTION__, name, index );
        c->index = index;
        return;
    }
    channel_set_peer( c, index, (EnqueueFunc) multiplexer_enqueue, m );
    D( "%s: channel '%s' registered as index %d",
       __FUNCTION__, c->name, c->index );
}


/* handle answers from the control channel */
static void
multiplexer_handle_control( Multiplexer*  m, Packet*  p )
{
    int  len = p->len - HEADER_SIZE;

    /* for now, the only supported answer is 'ok:connect:<name>:<XX>' where
     * <XX> is a hexdecimal channel numner */
    D( "%s: received '%s'", __FUNCTION__, quote_bytes( (const void*)p->data, (unsigned)len ) );
    if ( !memcmp( p->data, "ok:connect:", 11 ) ) do {
        char*  name = (char*)p->data + 11;
        char*  q    = strchr( name, ':' );
        int    index;

        if (q == NULL)
            break;

        q[0] = 0;
        if (q + 3 > (char*)p->data + len)
            break;

        index = hex2int( (uint8_t*)q+1, 2 );
        if (index < 0)
            break;

        multiplexer_register_channel( m, name, index );
        goto Exit;
    }
    while(0);

    D( "%s: unsupported message !!", __FUNCTION__ );
Exit:
    packet_free(p);
}


static int
multiplexer_can_read( void*  opaque )
{
    Multiplexer* m = opaque;

    return cbuffer_write_avail( m->in_cbuffer );
}

/* the data comes from the serial port, we need to reconstruct packets then
 * dispatch them to the appropriate channel */
static void
multiplexer_read( void*  opaque, const uint8_t* from, int  len )
{
    Multiplexer*  m  = opaque;
    CBuffer*      cb = m->in_cbuffer;
    int           ret = 0;

    T("%s: received %d bytes from serial: '%s'",
      __FUNCTION__, len, quote_bytes( from, len ));

    ret = cbuffer_write( cb, from, len );
    if (ret == 0)
        return;

    for (;;) {
        int   len = cbuffer_read_avail( cb );

        if (m->in_datalen == 0) {
            uint8_t  header[HEADER_SIZE];

            if (len < HEADER_SIZE)
                break;

            cbuffer_read( cb, header, HEADER_SIZE );
            m->in_datalen = hex2int( header+0, 4 );
            m->in_channel = hex2int( header+4, 2 );
        }
        else
        {
            Packet*  p;

            if (len < m->in_datalen)
                break;

            /* a full packet was received */
            p = packet_alloc();
            cbuffer_read( cb, p->data, m->in_datalen );
            p->len = HEADER_SIZE + m->in_datalen;

            /* find the channel for this packet */
            if (m->in_channel == CHANNEL_CONTROL_INDEX)
                multiplexer_handle_control( m, p );
            else {
                Channel*  c = m->channels;
                Channel*  c_end = c + m->count;

                for ( ; c < c_end; c++ ) {
                    if (c->index == m->in_channel) {
                        channel_write( c, p->data, m->in_datalen );
                        break;
                    }
                }
                packet_free(p);
            }
            m->in_datalen = 0;
        }

    }
    return;
}

static void
multiplexer_query_channel( Multiplexer*  m, const char*  name )
{
    Packet*  p = packet_alloc();
    int      len;

    len = snprintf( (char*)p->data, MAX_PAYLOAD, "connect:%s", name );

    int2hex( p->header+0, 4, len );
    int2hex( p->header+4, 2, CHANNEL_CONTROL_INDEX );
    p->len = HEADER_SIZE + len;

    multiplexer_enqueue( m, p );
}


static Channel*
multiplexer_find_channel( Multiplexer*  m, const char*  name )
{
    int  n;
    for (n = 0; n < m->count; n++)
        if ( !strcmp(m->channels[n].name, name) )
            return m->channels + n;

    return NULL;
}


static Multiplexer       _multiplexer[1];
static CharDriverState*  android_qemud_cs;

extern void
android_qemud_init( void )
{
    Multiplexer*      m = _multiplexer;

    if (android_qemud_cs != NULL)
        return;

    m->count = 0;

    cbuffer_reset( m->in_cbuffer, m->in_buff, sizeof(m->in_buff) );
    m->in_datalen = 0;
    m->in_channel = 0;

    if (qemu_chr_open_charpipe( &android_qemud_cs, &m->cs ) < 0) {
        derror( "%s: can't create charpipe to serial port",
                __FUNCTION__ );
        exit(1);
    }

    qemu_chr_add_read_handler( m->cs, multiplexer_can_read,
                               multiplexer_read, m );
}


CharDriverState*  android_qemud_get_cs( void )
{
    if (android_qemud_cs == NULL)
        android_qemud_init();

    return android_qemud_cs;
}


extern int
android_qemud_get_channel( const char*  name, CharDriverState**  pcs )
{
    Multiplexer*      m = _multiplexer;
    Channel*          c;
    CharDriverState*  peer_cs;
    int               ret;

    if (m->cs == NULL)
        android_qemud_init();

    c = multiplexer_find_channel( m, name );
    if (c) {
        derror( "%s: trying to get already-opened qemud channel '%s'",
                __FUNCTION__, name );
        return -1;
    }

    if (m->count >= MAX_CHANNELS) {
        derror( "%s: too many registered channels (%d)",
                __FUNCTION__, m->count );
        return -1;
    }

    c = m->channels + m->count;

    ret = qemu_chr_open_charpipe( &peer_cs, pcs );
    if (ret == 0) {
        channel_init(c, name, peer_cs);
        m->count += 1;
        multiplexer_query_channel( m, c->name );
    }

    return ret;
}

extern int
android_qemud_set_channel( const char*  name, CharDriverState*  peer_cs )
{
    Multiplexer*  m = _multiplexer;
    Channel*      c;
    int           ret;

    if (m->cs == NULL)
        android_qemud_init();

    c = multiplexer_find_channel(m, name);
    if (c != NULL) {
        derror( "%s: trying to set opened qemud channel '%s'",
                __FUNCTION__, name );
        return -1;
    }

    if (m->count >= MAX_CHANNELS) {
        derror( "%s: too many registered channels (%d)",
                __FUNCTION__, m->count );
        return -1;
    }

    c = m->channels + m->count;
    channel_init(c, name, peer_cs);
    m->count += 1;
    multiplexer_query_channel( m, c->name );

    return ret;
}
