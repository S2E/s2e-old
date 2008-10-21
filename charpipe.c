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
#include "cbuffer.h"
#include "android_debug.h"

#define  xxDEBUG

#ifdef DEBUG
#  include <stdio.h>
#  define  D(...)   ( fprintf( stderr, __VA_ARGS__ ), fprintf(stderr, "\n") )
#else
#  define  D(...)   ((void)0)
#endif

/* we want to implement a bi-directionnal communication channel
 * between two QEMU character drivers that merge well into the
 * QEMU event loop.
 *
 * each half of the channel has its own object and buffer, and
 * we implement communication through charpipe_poll() which
 * must be called by the main event loop after its call to select()
 *
 */

#define  BIP_BUFFER_SIZE  512

typedef struct BipBuffer {
    struct BipBuffer*  next;
    CBuffer            cb[1];
    char               buff[ BIP_BUFFER_SIZE ];
} BipBuffer;

static BipBuffer*  _free_bip_buffers;

static BipBuffer*
bip_buffer_alloc( void )
{
    BipBuffer*  bip = _free_bip_buffers;
    if (bip != NULL) {
        _free_bip_buffers = bip->next;
    } else {
        bip = malloc( sizeof(*bip) );
        if (bip == NULL) {
            derror( "%s: not enough memory", __FUNCTION__ );
            exit(1);
        }
    }
    bip->next = NULL;
    cbuffer_reset( bip->cb, bip->buff, sizeof(bip->buff) );
    return bip;
}

static void
bip_buffer_free( BipBuffer*  bip )
{
    bip->next         = _free_bip_buffers;
    _free_bip_buffers = bip;
}

/* this models each half of the charpipe */
typedef struct CharPipeHalf {
    CharDriverState       cs[1];
    BipBuffer*            bip_first;
    BipBuffer*            bip_last;
    struct CharPipeHalf*  peer;         /* NULL if closed */
    IOCanRWHandler*       fd_can_read;
    IOReadHandler*        fd_read;
    void*                 fd_opaque;
} CharPipeHalf;



static void
charpipehalf_close( CharDriverState*  cs )
{
    CharPipeHalf*  ph = cs->opaque;

    while (ph->bip_first) {
        BipBuffer*  bip = ph->bip_first;
        ph->bip_first = bip->next;
        bip_buffer_free(bip);
    }
    ph->bip_last    = NULL;

    ph->peer        = NULL;
    ph->fd_can_read = NULL;
    ph->fd_read     = NULL;
    ph->fd_opaque   = NULL;
}


static void
charpipehalf_add_read_handler( CharDriverState*  cs,
                               IOCanRWHandler*   fd_can_read,
                               IOReadHandler*    fd_read,
                               void*             fd_opaque )
{
    CharPipeHalf*  ph = cs->opaque;

    ph->fd_can_read = fd_can_read;
    ph->fd_read     = fd_read;
    ph->fd_opaque   = fd_opaque;
}


static int
charpipehalf_write( CharDriverState*  cs, const uint8_t*  buf, int  len )
{
    CharPipeHalf*  ph   = cs->opaque;
    CharPipeHalf*  peer = ph->peer;
    BipBuffer*     bip  = ph->bip_last;
    int            ret  = 0;

    D("%s: writing %d bytes to %p: '%s'", __FUNCTION__,
      len, ph, quote_bytes( buf, len ));

    if (bip == NULL && peer != NULL && peer->fd_read != NULL) {
        /* no buffered data, try to write directly to the peer */
        while (len > 0) {
            int  size;

            if (peer->fd_can_read) {
                size = peer->fd_can_read( peer->fd_opaque );
                if (size == 0)
                    break;

                if (size > len)
                    size = len;
            } else
                size = len;

            peer->fd_read( peer->fd_opaque, buf, size );
            buf += size;
            len -= size;
            ret += size;
        }
    }

    if (len == 0)
        return ret;

    /* buffer the remaining data */
    if (bip == NULL) {
        bip = bip_buffer_alloc();
        ph->bip_first = ph->bip_last = bip;
    }

    while (len > 0) {
        int  len2 = cbuffer_write( bip->cb, buf, len );

        buf += len2;
        ret += len2;
        len -= len2;
        if (len == 0)
            break;

        /* ok, we need another buffer */
        ph->bip_last = bip_buffer_alloc();
        bip->next = ph->bip_last;
        bip       = ph->bip_last;
    }
    return  ret;
}


static void
charpipehalf_poll( CharPipeHalf*  ph )
{
    CharPipeHalf*   peer = ph->peer;
    int             size;

    if (peer == NULL || peer->fd_read == NULL)
        return;

    while (1) {
        BipBuffer*  bip = ph->bip_first;
        uint8_t*    base;
        int         avail;

        if (bip == NULL)
            break;

        size = cbuffer_read_avail(bip->cb);
        if (size == 0) {
            ph->bip_first = bip->next;
            if (ph->bip_first == NULL)
                ph->bip_last = NULL;
            bip_buffer_free(bip);
            continue;
        }

        if (ph->fd_can_read) {
            int  size2 = peer->fd_can_read( peer->fd_opaque );

            if (size2 == 0)
                break;

            if (size > size2)
                size = size2;
        }

        avail = cbuffer_read_peek( bip->cb, &base );
        if (avail > size)
            avail = size;
        D("%s: sending %d bytes from %p: '%s'", __FUNCTION__,
            avail, ph, quote_bytes( base, avail ));

        peer->fd_read( peer->fd_opaque, base, avail );
        cbuffer_read_step( bip->cb, avail );
    }
}


static void
charpipehalf_init( CharPipeHalf*  ph, CharPipeHalf*  peer )
{
    CharDriverState*  cs = ph->cs;

    ph->bip_first   = NULL;
    ph->bip_last    = NULL;
    ph->peer        = peer;
    ph->fd_can_read = NULL;
    ph->fd_read     = NULL;
    ph->fd_opaque   = NULL;

    cs->chr_write            = charpipehalf_write;
    cs->chr_add_read_handler = charpipehalf_add_read_handler;
    cs->chr_ioctl            = NULL;
    cs->chr_send_event       = NULL;
    cs->chr_close            = charpipehalf_close;
    cs->opaque               = ph;
}


typedef struct CharPipeState {
    CharPipeHalf  a[1];
    CharPipeHalf  b[1];
} CharPipeState;



#define   MAX_CHAR_PIPES   8

static CharPipeState  _s_charpipes[ MAX_CHAR_PIPES ];

int
qemu_chr_open_charpipe( CharDriverState*  *pfirst, CharDriverState*  *psecond )
{
    CharPipeState*  cp     = _s_charpipes;
    CharPipeState*  cp_end = cp + MAX_CHAR_PIPES;

    for ( ; cp < cp_end; cp++ ) {
        if ( cp->a->peer == NULL && cp->b->peer == NULL )
            break;
    }

    if (cp == cp_end) {  /* can't allocate one */
        *pfirst  = NULL;
        *psecond = NULL;
        return -1;
    }

    charpipehalf_init( cp->a, cp->b );
    charpipehalf_init( cp->b, cp->a );

    *pfirst  = cp->a->cs;
    *psecond = cp->b->cs;
    return 0;
}

void
charpipe_poll( void )
{
    CharPipeState*  cp     = _s_charpipes;
    CharPipeState*  cp_end = cp + MAX_CHAR_PIPES;

    for ( ; cp < cp_end; cp++ ) {
        CharPipeHalf*  half;

        half = cp->a;
        if (half->peer != NULL)
            charpipehalf_poll(half);

        half = cp->b;
        if (half->peer != NULL)
            charpipehalf_poll(half);
    }
}
