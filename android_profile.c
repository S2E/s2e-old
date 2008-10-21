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
#include "android_profile.h"
#include <string.h>
#include <unistd.h>

/* this environment variable can be set to change the root profiles directory */
#define   ENV_PROFILE_DIR   "ANDROID_PROFILE_DIR"

/* otherwise, we're going to use <config>/EMULATOR_PROFILE_DIR */
#ifdef _WIN32
#  define  EMULATOR_PROFILE_DIR  "EmulatorProfiles"
#else
#  define  EMULATOR_PROFILE_DIR "profiles"
#endif

static char*  profile_dir;

static char*
profile_dir_find( void )
{
    const char*  env  = getenv( ENV_PROFILE_DIR );
    if (env != NULL) {
        int  len = strlen(env);

        if ( access( env, R_OK ) < 0 ) {
            dprint( "%s variable does not point to a valid directory. ignored\n", ENV_PROFILE_DIR );
        }
        else {
            dprint( "using '%s' as profile root directory", env );
            if (len > 0 && env[len-1] == PATH_SEP[0]) {
                len -= 1;
            }
            profile_dir = malloc( len+1 );
            memcpy(profile_dir, env, len);
            profile_dir[len] = 0;
            return profile_dir;
        }
    }

    {
        char  temp[512];
        char*  p = temp;
        char*  end = p + sizeof(temp);
        p = bufprint_config_file( p, end, EMULATOR_PROFILE_DIR );
        if (p >= end) {
            fprintf( stderr, "emulator configuration directory path too long. aborting" );
            exit(1);
        }
        if (p > temp && p[-1] == PATH_SEP[0])
            p[-1] = 0;

        dprint( "using '%s' as profile root directory", temp );
        profile_dir = strdup( temp );
        return profile_dir;
    }
}

int
android_profile_check_name( const char*  profile_name )
{
    static const char*  goodchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-0123456789.";
    int                 len       = strlen(profile_name);
    int                 slen      = strspn(profile_name, goodchars);

    return (len == slen);
}

char*
android_profile_bufprint_path( char*  p, char*  end, const char*  profile_name )
{
    return android_profile_bufprint_file_path( p, end, profile_name, NULL );
}

char*
android_profile_strdup_path( const char*  profile_name )
{
    return android_profile_strdup_file_path( profile_name, NULL );
}

char*
android_profile_bufprint_file_path( char*  p, char*  end, const char*  profile_name, const char*  file_name )
{
    if (profile_dir == NULL)
        profile_dir = profile_dir_find();

    p = bufprint( p, end, "%s", profile_dir );
    if (file_name != NULL) {
        p = bufprint( p, end, PATH_SEP "%s", file_name );
    }
    return p;
}

char*
android_profile_strdup_file_path( const char*  profile_name, const char*  file_name )
{
    int    len1, len;
    char*  result;

    if (profile_dir == NULL)
        profile_dir = profile_dir_find();

    len1 = strlen(profile_dir);
    len  = len1;
    if (file_name) {
        len += 1 + strlen(file_name);
    }

    result = malloc( len+1 );
    memcpy( result, profile_dir, len1 );
    if (file_name) {
        result[len1] = PATH_SEP[0];
        strcpy( result+len1+1, file_name );
    }
    result[len] = 0;
    return  result;
}

