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

/* this file implements the support of the new 'hardware control'
 * qemud communication channel, which is used by libs/hardware on
 * the system image to communicate with the emulator program for
 * emulating the following:
 *
 *   - power management
 *   - led(s) brightness
 *   - vibrator
 *   - flashlight
 */
#include "android/hw-control.h"
#include "cbuffer.h"
#include "android/qemud.h"
#include "android/utils/misc.h"
#include "android/utils/debug.h"
#include "qemu-char.h"
#include <stdio.h>
#include <string.h>

#define  D(...)  VERBOSE_PRINT(hw_control,__VA_ARGS__)

/* define T_ACTIVE to 1 to debug transport communications */
#define  T_ACTIVE  0

#if T_ACTIVE
#define  T(...)  VERBOSE_PRINT(hw_control,__VA_ARGS__)
#else
#define  T(...)   ((void)0)
#endif

static void*                  hw_control_client;
static AndroidHwControlFuncs  hw_control_funcs;

#define  BUFFER_SIZE  512

typedef struct {
    CharDriverState*  cs;
    int               overflow;
    int               wanted;
    CBuffer           input[1];
    char              input_0[ BUFFER_SIZE ];
    /* note: 1 more byte to zero-terminate the query */
    char              query[ BUFFER_SIZE+1 ];
} HwControl;

/* forward */
static void  hw_control_do_query( HwControl*  h,
                                  uint8_t*    query,
                                  int         querylen );

static void
hw_control_init( HwControl*  h, CharDriverState*  cs )
{
    h->cs       = cs;
    h->overflow = 0;
    h->wanted   = 0;
    cbuffer_reset( h->input,  h->input_0,  sizeof h->input_0 );
}

static int
hw_control_can_read( void*  _hw )
{
    HwControl*  h = _hw;
    return cbuffer_write_avail( h->input );
}

static void
hw_control_read( void*  _hw, const uint8_t*  data, int  len )
{
    HwControl*  h     = _hw;
    CBuffer*    input = h->input;

    T("%s: %4d '%.*s'", __FUNCTION__, len, len, data);

    cbuffer_write( input, data, len );

    while ( input->count > 0 )
    {
        /* skip over unwanted data, if any */
        while (h->overflow > 0) {
            uint8_t*  dummy;
            int       avail = cbuffer_read_peek( input, &dummy );

            if (avail == 0)
                return;

            if (avail > h->overflow)
                avail = h->overflow;

            cbuffer_read_step( input, avail );
            h->overflow -= avail;
        }

        /* all incoming messages are made of a 4-byte hexchar sequence giving */
        /* the length of the following payload                                */
        if (h->wanted == 0)
        {
            char  header[4];
            int   len;

            if (input->count < 4)
                return;

            cbuffer_read( input, header, 4 );
            len = hex2int( (uint8_t*)header, 4 );
            if (len >= 0) {
                /* if the message is too long, skip it */
                if (len > input->size) {
                    T("%s: skipping oversized message (%d > %d)",
                    __FUNCTION__, len, input->size);
                    h->overflow = len;
                } else {
                    T("%s: waiting for %d bytes", __FUNCTION__, len);
                    h->wanted = len;
                }
            }
        }
        else
        {
            if (input->count < h->wanted)
                break;

            cbuffer_read( input, h->query, h->wanted );
            h->query[h->wanted] = 0;
            hw_control_do_query( h, (uint8_t*)h->query, h->wanted );
            h->wanted = 0;
        }
    }
}


static uint8_t*
if_starts_with( uint8_t*  buf, int buflen, const char*  prefix )
{
    int  prefixlen = strlen(prefix);

    if (buflen < prefixlen || memcmp(buf, prefix, prefixlen))
        return NULL;

    return (uint8_t*)buf + prefixlen;
}


static void
hw_control_do_query( HwControl*  h,
                     uint8_t*    query,
                     int         querylen )
{
    uint8_t*   q;

    D("%s: query %4d '%.*s'", __FUNCTION__, querylen, querylen, query );

    q = if_starts_with( query, querylen, "power:light:brightness:" );
    if (q != NULL) {
        if (hw_control_funcs.light_brightness) {
            char*  qq = strchr((const char*)q, ':');
            int    value;
            if (qq == NULL) {
                D("%s: badly formatted", __FUNCTION__ );
                return;
            }
            *qq++ = 0;
            value = atoi(qq);
            hw_control_funcs.light_brightness( hw_control_client, (char*)q, value );
        }
        return;
    }
}


void
android_hw_control_init( void*  opaque, const AndroidHwControlFuncs*  funcs )
{
    static CharDriverState*   hw_control_cs;
    static HwControl          hwstate[1];

    if (hw_control_cs == NULL) {
        CharDriverState*  cs;
        if ( android_qemud_get_channel( ANDROID_QEMUD_CONTROL, &cs ) < 0 ) {
            derror( "could not create hardware control charpipe" );
            exit(1);
        }

        hw_control_cs = cs;
        hw_control_init( hwstate, cs );
        qemu_chr_add_handlers( cs, hw_control_can_read, hw_control_read, NULL, hwstate );

        D("%s: hw-control char pipe initialized", __FUNCTION__);
    }
    hw_control_client = opaque;
    hw_control_funcs  = funcs[0];
}
