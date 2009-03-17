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
#ifndef _android_charmap_h
#define _android_charmap_h

#include "linux_keycodes.h"

/* Keep it consistent with linux/input.h */
typedef enum {
    kKeyCodeSoftLeft                = KEY_SOFT1,
    kKeyCodeSoftRight               = KEY_SOFT2,
    kKeyCodeHome                    = KEY_HOME,
    kKeyCodeBack                    = KEY_BACK,
    kKeyCodeCall                    = KEY_SEND,
    kKeyCodeEndCall                 = KEY_END,
    kKeyCode0                       = KEY_0,
    kKeyCode1                       = KEY_1,
    kKeyCode2                       = KEY_2,
    kKeyCode3                       = KEY_3,
    kKeyCode4                       = KEY_4,
    kKeyCode5                       = KEY_5,
    kKeyCode6                       = KEY_6,
    kKeyCode7                       = KEY_7,
    kKeyCode8                       = KEY_8,
    kKeyCode9                       = KEY_9,
    kKeyCodeStar                    = KEY_STAR,
    kKeyCodePound                   = KEY_SHARP,
    kKeyCodeDpadUp                  = KEY_UP,
    kKeyCodeDpadDown                = KEY_DOWN,
    kKeyCodeDpadLeft                = KEY_LEFT,
    kKeyCodeDpadRight               = KEY_RIGHT,
    kKeyCodeDpadCenter              = KEY_CENTER,
    kKeyCodeVolumeUp                = KEY_VOLUMEUP,
    kKeyCodeVolumeDown              = KEY_VOLUMEDOWN,
    kKeyCodePower                   = KEY_POWER,
    kKeyCodeCamera                  = KEY_CAMERA,
    kKeyCodeClear                   = KEY_CLEAR,
    kKeyCodeA                       = KEY_A,
    kKeyCodeB                       = KEY_B,
    kKeyCodeC                       = KEY_C,
    kKeyCodeD                       = KEY_D,
    kKeyCodeE                       = KEY_E,
    kKeyCodeF                       = KEY_F,
    kKeyCodeG                       = KEY_G,
    kKeyCodeH                       = KEY_H,
    kKeyCodeI                       = KEY_I,
    kKeyCodeJ                       = KEY_J,
    kKeyCodeK                       = KEY_K,
    kKeyCodeL                       = KEY_L,
    kKeyCodeM                       = KEY_M,
    kKeyCodeN                       = KEY_N,
    kKeyCodeO                       = KEY_O,
    kKeyCodeP                       = KEY_P,
    kKeyCodeQ                       = KEY_Q,
    kKeyCodeR                       = KEY_R,
    kKeyCodeS                       = KEY_S,
    kKeyCodeT                       = KEY_T,
    kKeyCodeU                       = KEY_U,
    kKeyCodeV                       = KEY_V,
    kKeyCodeW                       = KEY_W,
    kKeyCodeX                       = KEY_X,
    kKeyCodeY                       = KEY_Y,
    kKeyCodeZ                       = KEY_Z,

    kKeyCodeComma                   = KEY_COMMA,
    kKeyCodePeriod                  = KEY_DOT,
    kKeyCodeAltLeft                 = KEY_LEFTALT,
    kKeyCodeAltRight                = KEY_RIGHTALT,
    kKeyCodeCapLeft                 = KEY_LEFTSHIFT,
    kKeyCodeCapRight                = KEY_RIGHTSHIFT,
    kKeyCodeTab                     = KEY_TAB,
    kKeyCodeSpace                   = KEY_SPACE,
    kKeyCodeSym                     = KEY_COMPOSE,
    kKeyCodeExplorer                = KEY_WWW,
    kKeyCodeEnvelope                = KEY_MAIL,
    kKeyCodeNewline                 = KEY_ENTER,
    kKeyCodeDel                     = KEY_BACKSPACE,
    kKeyCodeGrave                   = 399,
    kKeyCodeMinus                   = KEY_MINUS,
    kKeyCodeEquals                  = KEY_EQUAL,
    kKeyCodeLeftBracket             = KEY_LEFTBRACE,
    kKeyCodeRightBracket            = KEY_RIGHTBRACE,
    kKeyCodeBackslash               = KEY_BACKSLASH,
    kKeyCodeSemicolon               = KEY_SEMICOLON,
    kKeyCodeApostrophe              = KEY_APOSTROPHE,
    kKeyCodeSlash                   = KEY_SLASH,
    kKeyCodeAt                      = KEY_EMAIL,
    kKeyCodeNum                     = KEY_NUM,
    kKeyCodeHeadsetHook             = KEY_HEADSETHOOK,
    kKeyCodeFocus                   = KEY_FOCUS,
    kKeyCodePlus                    = KEY_PLUS,
    kKeyCodeMenu                    = KEY_MENU,
    kKeyCodeNotification            = KEY_NOTIFICATION,
    kKeyCodeSearch                  = KEY_SEARCH,

} AndroidKeyCode;


/* this defines a structure used to describe an Android keyboard charmap */
typedef struct AKeyEntry {
    unsigned short  code;
    unsigned short  base;
    unsigned short  caps;
    unsigned short  fn;
    unsigned short  caps_fn;
    unsigned short  number;
} AKeyEntry;

typedef struct {
    const AKeyEntry*  entries;
    int               num_entries;
    char              name[ 32 ];
} AKeyCharmap;

extern const int           android_charmap_count;
extern const AKeyCharmap*  android_charmaps[];

#endif /* _android_charmap_h */
