/* Copyright (C) 2011 The Android Open Source Project
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
#ifndef ANDROID_MAIN_COMMON_H
#define ANDROID_MAIN_COMMON_H

#include <stdint.h>
#include "android/cmdline-option.h"
#include "android/skin/keyset.h"
#include "android/config.h"

/* Common routines used by both android/main.c and android/main-ui.c */

/** Emulator user configuration (e.g. last window position)
 **/

void user_config_init( void );
void user_config_done( void );

void user_config_get_window_pos( int *window_x, int *window_y );

#define  ONE_MB  (1024*1024)

unsigned convertBytesToMB( uint64_t  size );
uint64_t convertMBToBytes( unsigned  megaBytes );

extern SkinKeyset*  android_keyset;
void parse_keyset(const char*  keyset, AndroidOptions*  opts);
void write_default_keyset( void );

extern const char*  skin_network_speed;
extern const char*  skin_network_delay;

/* Find the skin corresponding to our options, and return an AConfig pointer
 * and the base path to load skin data from
 */
void parse_skin_files(const char*      skinDirPath,
                      const char*      skinName,
                      AndroidOptions*  opts,
                      AConfig*        *skinConfig,
                      char*           *skinPath);

/* Returns the amount of pixels used by the default display. */
int64_t  get_screen_pixels(AConfig*  skinConfig);

void init_sdl_ui(AConfig*         skinConfig,
                 const char*      skinPath,
                 AndroidOptions*  opts);

#endif /* ANDROID_MAIN_COMMON_H */
