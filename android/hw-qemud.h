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
#ifndef _android_qemud_h
#define _android_qemud_h

#include "qemu-common.h"

/* Support for the qemud-based 'services' in the emulator.
 * Please read docs/ANDROID-QEMUD.TXT to understand what this is about.
 */

/* initialize the qemud support code in the emulator
 */

extern void  android_qemud_init( void );

/* return the character driver state object that needs to be connected to the
 * emulated serial port where all multiplexed channels go through.
 */
extern CharDriverState*  android_qemud_get_cs( void );

/* returns in '*pcs' a CharDriverState object that will be connected to
 * a single client in the emulated system for a given named service.
 *
 * this is only used to connect GPS and GSM service clients to the
 * implementation that requires a CharDriverState object for legacy
 * reasons.
 *
 * returns 0 on success, or -1 in case of error
 */
extern int  android_qemud_get_channel( const char*  name, CharDriverState* *pcs );

/* set an explicit CharDriverState object for a given qemud communication channel. this
 * is used to attach the channel to an external char driver device (e.g. one
 * created with "-serial <device>") directly.
 *
 * returns 0 on success, -1 on error
 */
extern int  android_qemud_set_channel( const char*  name, CharDriverState*  peer_cs );

/* list of known qemud channel names */
#define  ANDROID_QEMUD_GSM      "gsm"
#define  ANDROID_QEMUD_GPS      "gps"
#define  ANDROID_QEMUD_CONTROL  "control"
#define  ANDROID_QEMUD_SENSORS  "sensors"

/* A QemudService service is used to connect one or more clients to
 * a given emulator facility. Only one client can be connected at any
 * given time, but the connection can be closed periodically.
 */

typedef struct QemudClient   QemudClient;
typedef struct QemudService  QemudService;


typedef void (*QemudClientClose)( void*  opaque );
typedef void (*QemudClientRecv) ( void*  opaque, uint8_t*  msg, int  msglen );

extern QemudClient*  qemud_client_new( QemudService*     service,
                                       int               channel_id,
                                       void*             clie_opaque,
                                       QemudClientRecv   clie_recv,
                                       QemudClientClose  clie_close );

extern void           qemud_client_set_framing( QemudClient*  client, int  enabled );

extern void   qemud_client_send ( QemudClient*  client, const uint8_t*  msg, int  msglen );
extern void   qemud_client_close( QemudClient*  client );


typedef QemudClient*  (*QemudServiceConnect)( void*   opaque, QemudService*  service, int  channel );

extern QemudService*  qemud_service_register( const char*          serviceName,
                                              int                  max_clients,
                                              void*                serv_opaque,
                                              QemudServiceConnect  serv_connect );

extern void           qemud_service_broadcast( QemudService*   sv,
                                               const uint8_t*  msg,
                                               int             msglen );

#endif /* _android_qemud_h */
