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
#ifndef _ANDROID_PROFILE_H
#define _ANDROID_PROFILE_H

#include "android.h"
#include "android_utils.h"

/* verify that a profile name doesn't contain bad characters, returns 0 on success, or -1 in case of error */
extern int   android_profile_check_name( const char*  profile_name );

/* safely append the path of a given profile to a buffer */
extern char*  android_profile_bufprint_path( char*  p, char*  end, const char*  profile_name );

/* return a copy of a profile path */
extern char*  android_profile_strdup_path( const char*  profile_name );

/* safely append the path of a given profile file to a buffer */
extern char*  android_profile_bufprint_file_path( char*  p, char*  end, const char*  profile_name, const char*  file_name );

/* return a copy of a profile file path */
extern char*  android_profile_strdup_file_path( const char*  profile_name, const char*  file_name );


#endif /* ANDROID_PROFILE_H */
