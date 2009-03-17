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
#ifndef _ANDROID_HW_EVENTS_H
#define _ANDROID_HW_EVENTS_H

#include "android/utils/system.h"

/* from the Linux kernel */

#define  EVENT_TYPE_LIST  \
  EV_TYPE(SYN,0x00)   \
  EV_TYPE(KEY,0x01)   \
  EV_TYPE(REL,0x02)   \
  EV_TYPE(ABS,0x03)   \
  EV_TYPE(MSC,0x04)   \
  EV_TYPE(SW, 0x05)   \
  EV_TYPE(LED,0x11)   \
  EV_TYPE(SND,0x12)   \
  EV_TYPE(REP,0x14)   \
  EV_TYPE(FF, 0x15)   \
  EV_TYPE(PWR,0x16)   \
  EV_TYPE(FF_STATUS,0x17)  \
  EV_TYPE(MAX,0x1f)

#undef  EV_TYPE
#define EV_TYPE(n,v)    GLUE(EV_,n) = v,
typedef enum {
    EVENT_TYPE_LIST
} EventType;
#undef  EV_TYPE

#define  EVENT_BTN_LIST  \
    BTN_CODE(MISC,0x100)  \
    BTN_CODE(0,0x100)     \
    BTN_CODE(1,0x101)     \
    BTN_CODE(2,0x102)     \
    BTN_CODE(3,0x103)     \
    BTN_CODE(4,0x104)     \
    BTN_CODE(5,0x105)     \
    BTN_CODE(6,0x106)     \
    BTN_CODE(7,0x107)     \
    BTN_CODE(8,0x108)     \
    BTN_CODE(9,0x109)     \
    \
    BTN_CODE(MOUSE,  0x110)  \
    BTN_CODE(LEFT,   0x110)  \
    BTN_CODE(RIGHT,  0x111)  \
    BTN_CODE(MIDDLE, 0x112)  \
    BTN_CODE(SIDE,   0x113)  \
    BTN_CODE(EXTRA,  0x114)  \
    BTN_CODE(FORWARD,0x115)  \
    BTN_CODE(BACK,   0x116)  \
    BTN_CODE(TASK,   0x117)  \
    \
    BTN_CODE(JOYSTICK,0x120)  \
    BTN_CODE(TRIGGER, 0x120)  \
    BTN_CODE(THUMB,   0x121)  \
    BTN_CODE(THUMB2,  0x122)  \
    BTN_CODE(TOP,     0x123)  \
    BTN_CODE(TOP2,    0x124)  \
    BTN_CODE(PINKIE,  0x125)  \
    BTN_CODE(BASE,    0x126)  \
    BTN_CODE(BASE2,   0x127)  \
    BTN_CODE(BASE3,   0x128)  \
    BTN_CODE(BASE4,   0x129)  \
    BTN_CODE(BASE5,   0x12a)  \
    BTN_CODE(BASE6,   0x12b)  \
    BTN_CODE(DEAD,    0x12f)  \
    \
    BTN_CODE(GAMEPAD,  0x130)  \
    BTN_CODE(A,        0x130)  \
    BTN_CODE(B,        0x131)  \
    BTN_CODE(C,        0x132)  \
    BTN_CODE(X,        0x133)  \
    BTN_CODE(Y,        0x134)  \
    BTN_CODE(Z,        0x135)  \
    BTN_CODE(TL,       0x136)  \
    BTN_CODE(TR,       0x137)  \
    BTN_CODE(TL2,      0x138)  \
    BTN_CODE(TR2,      0x139)  \
    BTN_CODE(SELECT,   0x13a)  \
    BTN_CODE(START,    0x13b)  \
    BTN_CODE(MODE,     0x13c)  \
    BTN_CODE(THUMBL,   0x13d)  \
    BTN_CODE(THUMBR,   0x13e)  \
    \
    BTN_CODE(DIGI,            0x140)  \
    BTN_CODE(TOOL_PEN,        0x140)  \
    BTN_CODE(TOOL_RUBBER,     0x141)  \
    BTN_CODE(TOOL_BRUSH,      0x142)  \
    BTN_CODE(TOOL_PENCIL,     0x143)  \
    BTN_CODE(TOOL_AIRBRUSH,   0x144)  \
    BTN_CODE(TOOL_FINGER,     0x145)  \
    BTN_CODE(TOOL_MOUSE,      0x146)  \
    BTN_CODE(TOOL_LENS,       0x147)  \
    BTN_CODE(TOUCH,           0x14a)  \
    BTN_CODE(STYLUS,          0x14b)  \
    BTN_CODE(STYLUS2,         0x14c)  \
    BTN_CODE(TOOL_DOUBLETAP,  0x14d)  \
    BTN_CODE(TOOL_TRIPLETAP,  0x14e)  \
    \
    BTN_CODE(WHEEL,  0x150)      \
    BTN_CODE(GEAR_DOWN,  0x150)  \
    BTN_CODE(GEAR_UP,    0x150)

#undef  BTN_CODE
#define BTN_CODE(n,v)   GLUE(BTN_,n) = v,
typedef enum {
    EVENT_BTN_LIST
} EventBtnCode;
#undef  BTN_CODE

#define  EVENT_REL_LIST \
    REL_CODE(X,  0x00)  \
    REL_CODE(Y,  0x01)

#define  REL_CODE(n,v)  GLUE(REL_,n) = v,
typedef enum {
    EVENT_REL_LIST
} EventRelCode;
#undef  REL_CODE

#define  EVENT_ABS_LIST  \
    ABS_CODE(X,        0x00)  \
    ABS_CODE(Y,        0x01)  \
    ABS_CODE(Z,        0x02)  \
    ABS_CODE(RX,       0x03)  \
    ABS_CODE(RY,       0x04)  \
    ABS_CODE(RZ,       0x05)  \
    ABS_CODE(THROTTLE, 0x06)  \
    ABS_CODE(RUDDER,   0x07)  \
    ABS_CODE(WHEEL,    0x08)  \
    ABS_CODE(GAS,      0x09)  \
    ABS_CODE(BRAKE,    0x0a)  \
    ABS_CODE(HAT0X,    0x10)  \
    ABS_CODE(HAT0Y,    0x11)  \
    ABS_CODE(HAT1X,    0x12)  \
    ABS_CODE(HAT1Y,    0x13)  \
    ABS_CODE(HAT2X,    0x14)  \
    ABS_CODE(HAT2Y,    0x15)  \
    ABS_CODE(HAT3X,    0x16)  \
    ABS_CODE(HAT3Y,    0x17)  \
    ABS_CODE(PRESSURE, 0x18)  \
    ABS_CODE(DISTANCE, 0x19)  \
    ABS_CODE(TILT_X,   0x1a)  \
    ABS_CODE(TILT_Y,   0x1b)  \
    ABS_CODE(TOOL_WIDTH, 0x1c)  \
    ABS_CODE(VOLUME,     0x20)  \
    ABS_CODE(MISC,       0x28)  \
    ABS_CODE(MAX,        0x3f)

#define  ABS_CODE(n,v)  GLUE(ABS_,n) = v,

typedef enum {
    EVENT_ABS_LIST
} EventAbsCode;
#undef  ABS_CODE

/* convert an event string specification like <type>:<code>:<value>
 * into three integers. returns 0 on success, or -1 in case of error
 */
extern int   android_event_from_str( const char*  name,
                                     int         *ptype,
                                     int         *pcode,
                                     int         *pvalue );

/* returns the list of valid event type string aliases */
extern int    android_event_get_type_count( void );
extern char*  android_event_bufprint_type_str( char*  buff, char*  end, int  type_index );

/* returns the list of valid event code string aliases for a given event type */
extern int    android_event_get_code_count( int  type );
extern char*  android_event_bufprint_code_str( char*  buff, char*  end, int  type, int  code_index );

#endif /* _ANDROID_HW_EVENTS_H */
