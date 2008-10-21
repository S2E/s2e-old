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

#include "vl.h"

/* recent versions of the emulated Android system contains a background
 * daemon, named 'qemud', which runs as root and opens /dev/ttyS0
 *
 * its purpose is to multiplex several communication channels between
 * the emulator and the system through a single serial port.
 *
 * each channel will be connected to a qemud-created unix socket on the
 * system, and to either a emulated character device or other facility in
 * the emulator.
 *
 *                                                       +--------> /dev/socket/qemud_gsm
 *   emulated GSM    <-----+                       ______|_
 *                         |        emulated      |        |
 *                         +====> /dev/ttyS0 <===>| qemud  |------> /dev/socket/qemud_gps
 *                         |                      |________|
 *   emulated GPS    <-----+                            |
 *                         |                            +---------> other
 *                         |
 *   other  <--------------+
 *
 *
 *   this is done to overcome specific permission problems, as well as to add various
 *   features that would require special kernel drivers otherwise even though they
 *   only need a simple character channel.
 */

/* initialize the qemud support code in the emulator
 */

extern void  android_qemud_init( void );

/* return the character driver state object that needs to be connected to the
 * emulated serial port where all multiplexed channels go through.
 */
extern CharDriverState*  android_qemud_get_cs( void );

/* return the character driver state corresponding to a named qemud communication
 * channel. this can be used to send/data the channel.
 * returns 0 on success, or -1 in case of error
 */
extern int  android_qemud_get_channel( const char*  name, CharDriverState* *pcs );

/* set the character driver state for a given qemud communication channel. this
 * is used to attach the channel to an external char driver device directly.
 * returns 0 on success, -1 on error
 */
extern int  android_qemud_set_channel( const char*  name, CharDriverState*  peer_cs );

/* list of known qemud channel names */
#define  ANDROID_QEMUD_GSM  "gsm"
#define  ANDROID_QEMUD_GPS  "gps"

/* add new channel names here when you need them */

#endif /* _android_qemud_h */
