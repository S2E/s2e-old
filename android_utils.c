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
#include "android_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include <process.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <limits.h>
#include <winbase.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#endif
#include "android.h"
#include "android_debug.h"

#define  D(...)   VERBOSE_PRINT(init,__VA_ARGS__)

/** PATH HANDLING ROUTINES
 **
 **  path_parent() can be used to return the n-level parent of a given directory
 **  this understands . and .. when encountered in the input path
 **/

static __inline__ int
ispathsep(int  c)
{
#ifdef _WIN32
    return (c == '/' || c == '\\');
#else
    return (c == '/');
#endif
}

char*  path_parent( const char*  path, int  levels )
{
    const char*  end = path + strlen(path);
    char*        result;

    while (levels > 0) {
        const char*  base;

        /* trim any trailing path separator */
        while (end > path && ispathsep(end[-1]))
            end--;

        base = end;
        while (base > path && !ispathsep(base[-1]))
            base--;

        if (base <= path) /* we can't go that far */
            return NULL;

        if (end == base+1 && base[0] == '.')
            goto Next;

        if (end == base+2 && base[0] == '.' && base[1] == '.') {
            levels += 1;
            goto Next;
        }

        levels -= 1;

    Next:
        end = base - 1;
    }
    result = malloc( end-path+1 );
    if (result != NULL) {
        memcpy( result, path, end-path );
        result[end-path] = 0;
    }
    return result;
}


/** MISC FILE AND DIRECTORY HANDLING
 **/

int
path_exists( const char*  path )
{
    int  ret;
    CHECKED(ret, access(path, F_OK));
    return (ret == 0) || (errno != ENOENT);
}

/* checks that a path points to a regular file */
int
path_is_regular( const char*  path )
{
    int          ret;
    struct stat  st;

    CHECKED(ret, stat(path, &st));
    if (ret < 0)
        return 0;

    return S_ISREG(st.st_mode);
}


/* checks that a path points to a directory */
int
path_is_dir( const char*  path )
{
    int          ret;
    struct stat  st;

    CHECKED(ret, stat(path, &st));
    if (ret < 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

/* checks that one can read/write a given (regular) file */
int
path_can_read( const char*  path )
{
    int  ret;
    CHECKED(ret, access(path, R_OK));
    return (ret == 0);
}

int
path_can_write( const char*  path )
{
    int  ret;
    CHECKED(ret, access(path, R_OK));
    return (ret == 0);
}

/* try to make a directory. returns 0 on success, -1 on failure
 * (error code in errno) */
int
path_mkdir( const char*  path, int  mode )
{
#ifdef _WIN32
    (void)mode;
    return _mkdir(path);
#else
    int  ret;
    CHECKED(ret, mkdir(path, mode));
    return ret;
#endif
}

static int
path_mkdir_recursive( char*  path, unsigned  len, int  mode )
{
    char      old_c;
    int       ret;
    unsigned  len2;

    /* get rid of trailing separators */
    while (len > 0 && ispathsep(path[len-1]))
        len -= 1;

    if (len == 0) {
        errno = ENOENT;
        return -1;
    }

    /* check that the parent exists, 'len2' is the length of
     * the parent part of the path */
    len2 = len-1;
    while (len2 > 0 && !ispathsep(path[len2-1]))
        len2 -= 1;

    if (len2 > 0) {
        old_c      = path[len2];
        path[len2] = 0;
        ret        = 0;
        if ( !path_exists(path) ) {
            /* the parent doesn't exist, so try to create it */
            ret = path_mkdir_recursive( path, len2, mode );
        }
        path[len2] = old_c;

        if (ret < 0)
            return ret;
    }

    /* at this point, we now the parent exists */
    old_c     = path[len];
    path[len] = 0;
    ret       = path_mkdir( path, mode );
    path[len] = old_c;

    return ret;
}

/* ensure that a given directory exists, create it if not, 
   0 on success, -1 on failure (error code in errno) */
int
path_mkdir_if_needed( const char*  path, int  mode )
{
    int  ret = 0;

    if (!path_exists(path)) {
        ret = path_mkdir(path, mode);

        if (ret < 0 && errno == ENOENT) {
            char      temp[MAX_PATH];
            unsigned  len = (unsigned)strlen(path);

            if (len > sizeof(temp)-1) {
                errno = EINVAL;
                return -1;
            }
            memcpy( temp, path, len );
            temp[len] = 0;

            return path_mkdir_recursive(temp, len, mode);
        }
    }
    return ret;
}

/* return the size of a given file in '*psize'. returns 0 on
 * success, -1 on failure (error code in errno) */
int
path_get_size( const char*  path, uint64_t  *psize )
{
#ifdef _WIN32
    /* avoid _stat64 which is only defined in MSVCRT.DLL, not CRTDLL.DLL */
    /* do not use OpenFile() because it has strange search behaviour that could */
    /* result in getting the size of a different file */
    LARGE_INTEGER  size;
    HANDLE  file = CreateFile( /* lpFilename */        path,
                               /* dwDesiredAccess */   GENERIC_READ,    
                               /* dwSharedMode */     FILE_SHARE_READ|FILE_SHARE_WRITE, 
                               /* lpSecurityAttributes */  NULL, 
                               /* dwCreationDisposition */ OPEN_EXISTING,
                               /* dwFlagsAndAttributes */  0,
                               /* hTemplateFile */      NULL );
    if (file == INVALID_HANDLE_VALUE) {
        /* ok, just to play fair */
        errno = ENOENT;
        return -1;
    }
    if (!GetFileSizeEx(file, &size)) {
        /* maybe we tried to get the size of a pipe or something like that ? */
        *psize = 0;
    }
    else {
        *psize = (uint64_t) size.QuadPart;
    }
    CloseHandle(file);
    return 0;
#else
    int    ret;
    struct stat  st;

    CHECKED(ret, stat(path, &st));
    if (ret == 0) {
        *psize = (uint64_t) st.st_size;
    }
    return ret;
#endif
}


/** USEFUL STRING BUFFER FUNCTIONS
 **/

char*
vbufprint( char*        buffer,
           char*        buffer_end,
           const char*  fmt,
           va_list      args )
{
    int  len = vsnprintf( buffer, buffer_end - buffer, fmt, args );
    if (len < 0 || buffer+len >= buffer_end) {
        if (buffer < buffer_end)
            buffer_end[-1] = 0;
        return buffer_end;
    }
    return buffer + len;
}

char*
bufprint(char*  buffer, char*  end, const char*  fmt, ... )
{
    va_list  args;
    char*    result;

    va_start(args, fmt);
    result = vbufprint(buffer, end, fmt, args);
    va_end(args);
    return  result;
}

/** USEFUL DIRECTORY SUPPORT
 **
 **  bufprint_app_dir() returns the directory where the emulator binary is located
 **
 **  get_android_home() returns a user-specific directory where the emulator will
 **  store its writable data (e.g. config files, profiles, etc...).
 **  on Unix, this is $HOME/.android, on Windows, this is something like
 **  "%USERPROFILE%/Local Settings/AppData/Android" on XP, and something different
 **  on Vista.
 **
 **  both functions return a string that must be freed by the caller
 **/

#ifdef __linux__
char*
bufprint_app_dir(char*  buff, char*  end)
{
    char   path[1024];
    int    len;
    char*  x;

    len = readlink("/proc/self/exe", path, sizeof(path));
    if (len <= 0 || len >= (int)sizeof(path)) goto Fail;
    path[len] = 0;

    x = strrchr(path, '/');
    if (x == 0) goto Fail;
    *x = 0;

    return bufprint(buff, end, "%s", path);
Fail:
    fprintf(stderr,"cannot locate application directory\n");
    exit(1);
    return end;
}

#elif defined(__APPLE__)
/* the following hack is needed in order to build with XCode 3.1
 * don't ask me why, but it seems that there were changes in the
 * GCC compiler that we don't have in our pre-compiled version
 */
#ifndef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ MAC_OS_X_VERSION_10_4
#endif
#import <Carbon/Carbon.h>
#include <unistd.h>

char*
bufprint_app_dir(char*  buff, char*  end)
{
    ProcessSerialNumber psn;
    CFDictionaryRef     dict;
    CFStringRef         value;
    char                s[PATH_MAX];
    char*               x;

    GetCurrentProcess(&psn);
    dict  = ProcessInformationCopyDictionary(&psn, 0xffffffff);
    value = (CFStringRef)CFDictionaryGetValue(dict,
                                             CFSTR("CFBundleExecutable"));
    CFStringGetCString(value, s, PATH_MAX - 1, kCFStringEncodingUTF8);
    x = strrchr(s, '/');
    if (x == 0) goto fail;
    *x = 0;

    return bufprint(buff, end, "%s", s);
fail:
    fprintf(stderr,"cannot locate application directory\n");
    exit(1);
    return end;
}
#elif defined _WIN32
char*
bufprint_app_dir(char*  buff, char*  end)
{
    char   appDir[MAX_PATH];
    char*  sep;

    GetModuleFileName( 0, appDir, sizeof(appDir)-1 );
    sep = strrchr( appDir, '\\' );
    if (sep)
        *sep = 0;

    return bufprint(buff, end, "%s", appDir);
}
#else
char*
bufprint_app_dir(char*  buff, char*  end)
{
    return bufprint(buff, end, ".");
}
#endif

#ifdef _WIN32
#define  _ANDROID_PATH   "Android"
#else
#define  _ANDROID_PATH   ".android"
#endif

char*
bufprint_config_path(char*  buff, char*  end)
{
#ifdef _WIN32
    char  path[MAX_PATH];

    SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,
                     NULL, 0, path);

    return bufprint(buff, end, "%s\\%s\\%s", path, _ANDROID_PATH, ANDROID_SDK_VERSION);
#else
    const char*  home = getenv("HOME");
    if (home == NULL)
        home = "/tmp";
    return bufprint(buff, end, "%s/%s/%s", home, _ANDROID_PATH, ANDROID_SDK_VERSION);
#endif
}

char*
bufprint_config_file(char*  buff, char*  end, const char*  suffix)
{
    char*   p;
    p = bufprint_config_path(buff, end);
    p = bufprint(p, end, PATH_SEP "%s", suffix);
    return p;
}

char*
bufprint_temp_dir(char*  buff, char*  end)
{
#ifdef _WIN32
    char   path[MAX_PATH];
    DWORD  retval;

    retval = GetTempPath( sizeof(path), path );
    if (retval > sizeof(path) || retval == 0) {
        D( "can't locate TEMP directory" );
        pstrcpy(path, sizeof(path), "C:\\Temp");
    }
    strncat( path, "\\AndroidEmulator", sizeof(path)-1 );
    _mkdir(path);

    return  bufprint(buff, end, "%s", path);
#else
    const char*  tmppath = "/tmp/android";
    mkdir(tmppath, 0744);
    return  bufprint(buff, end, "%s", tmppath );
#endif
}

char*
bufprint_temp_file(char*  buff, char*  end, const char*  suffix)
{
    char*  p;
    p = bufprint_temp_dir(buff, end);
    p = bufprint(p, end, PATH_SEP "%s", suffix);
    return p;
}



/** FILE LOCKS SUPPORT
 **
 ** a FileLock is useful to prevent several emulator instances from using the same
 ** writable file (e.g. the userdata.img disk images).
 **
 ** create a FileLock object with filelock_create(), ithis function should return NULL
 ** only if thee file doesn't exist, or if you don't have enough memory.
 *
 *  then call filelock_lock() to try to acquire a lock for the corresponding file.
 ** returns 0 on success, or -1 in case of error, which means that another program
 ** is using the file or that the directory containing the file is read-only.
 **
 ** all file locks are automatically released and destroyed when the program exits.
 ** the filelock_lock() function can also detect stale file locks that can linger
 ** when the emulator crashes unexpectedly, and will happily clean them for you
 **
 **  here's how it works, three files are used:
 **     file  - the data file accessed by the emulator
 **     lock  - a lock file  (file + '.lock')
 **     temp  - a temporary file make unique with mkstemp
 **
 **  when locking:
 **      create 'temp' and store our pid in it
 **      attemp to link 'lock' to 'temp'
 **         if the link succeeds, we obtain the lock
 **      unlink 'temp'
 **
 **  when unlocking:
 **      unlink 'lock'
 **
 **
 **  on Windows, 'lock' is a directory name. locking is equivalent to
 **  creating it...
 **
 **/

struct FileLock
{
  const char*  file;
  const char*  lock;
  char*        temp;
  int          locked;
  FileLock*    next;
};

#define  LOCK_NAME   ".lock"
#define  TEMP_NAME   ".tmp-XXXXXX"

#ifdef _WIN32
#define  PIDFILE_NAME  "pid"
#endif

/* returns 0 on success, -1 on failure */
int
filelock_lock( FileLock*  lock )
{
    int    ret;
#ifdef _WIN32
    int  pidfile_fd = -1;

    ret = _mkdir( lock->lock );
    if (ret < 0) {
        if (errno == ENOENT) {
            D( "could not access directory '%s', check path elements", lock->lock );
            return -1;
        } else if (errno != EEXIST) {
            D( "_mkdir(%s): %s", lock->lock, strerror(errno) );
            return -1;
        }

        /* if we get here, it's because the .lock directory already exists */
        /* check to see if there is a pid file in it                       */
        D("directory '%s' already exist, waiting a bit to ensure that no other emulator instance is starting", lock->lock );
        {
            int  _sleep = 200;
            int  tries;

            for ( tries = 4; tries > 0; tries-- )
            {
                pidfile_fd = open( lock->temp, O_RDONLY );

                if (pidfile_fd >= 0)
                    break;

                Sleep( _sleep );
                _sleep *= 2;
            }
        }

        if (pidfile_fd < 0) {
            D( "no pid file in '%s', assuming stale directory", lock->lock );
        }
        else
        {
            /* read the pidfile, and check wether the corresponding process is still running */
            char            buf[16];
            int             len, lockpid;
            HANDLE          processSnapshot;
            PROCESSENTRY32  pe32;
            int             is_locked = 0;

            len = read( pidfile_fd, buf, sizeof(buf)-1 );
            if (len < 0) {
                D( "could not read pid file '%s'", lock->temp );
                close( pidfile_fd );
                return -1;
            }
            buf[len] = 0;
            lockpid  = atoi(buf);

            /* PID 0 is the IDLE process, and 0 is returned in case of invalid input */
            if (lockpid == 0)
                lockpid = -1;

            close( pidfile_fd );

            pe32.dwSize     = sizeof( PROCESSENTRY32 );
            processSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

            if ( processSnapshot == INVALID_HANDLE_VALUE ) {
                D( "could not retrieve the list of currently active processes\n" );
                is_locked = 1;
            }
            else if ( !Process32First( processSnapshot, &pe32 ) )
            {
                D( "could not retrieve first process id\n" );
                CloseHandle( processSnapshot );
                is_locked = 1;
            }
            else
            {
                do {
                    if (pe32.th32ProcessID == lockpid) {
                        is_locked = 1;
                        break;
                    }
                } while (Process32Next( processSnapshot, &pe32 ) );

                CloseHandle( processSnapshot );
            }

            if (is_locked) {
                D( "the file '%s' is locked by process ID %d\n", lock->file, lockpid );
                return -1;
            }
        }
    }

    /* write our PID into the pid file */
    pidfile_fd = open( lock->temp, O_WRONLY | O_CREAT | O_TRUNC );
    if (pidfile_fd < 0) {
        if (errno == EACCES) {
            if ( unlink_file( lock->temp ) < 0 ) {
                D( "could not remove '%s': %s\n", lock->temp, strerror(errno) );
                return -1;
            }
            pidfile_fd = open( lock->temp, O_WRONLY | O_CREAT | O_TRUNC );
        }
        if (pidfile_fd < 0) {
            D( "could not create '%s': %s\n", lock->temp, strerror(errno) );
            return -1;
        }
    }

    {
        char  buf[16];
        sprintf( buf, "%ld", GetCurrentProcessId() );
        ret = write( pidfile_fd, buf, strlen(buf) );
        close(pidfile_fd);
        if (ret < 0) {
            D( "could not write PID to '%s'\n", lock->temp );
            return -1;
        }
    }

    lock->locked = 1;
    return 0;
#else
    int    temp_fd = -1;
    int    lock_fd = -1;
    int    rc, tries, _sleep;
    FILE*  f = NULL;
    char   pid[8];
    struct stat  st_temp;

    strcpy( lock->temp, lock->file );
    strcat( lock->temp, TEMP_NAME );
    temp_fd = mkstemp( lock->temp );

    if (temp_fd < 0) {
        D("cannot create locking temp file '%s'", lock->temp );
        goto Fail;
    }

    sprintf( pid, "%d", getpid() );
    ret = write( temp_fd, pid, strlen(pid)+1 );
    if (ret < 0) {
        D( "cannot write to locking temp file '%s'", lock->temp);
        goto Fail;
    }
    close( temp_fd );
    temp_fd = -1;

    CHECKED(rc, lstat( lock->temp, &st_temp ));
    if (rc < 0) {
        D( "can't properly stat our locking temp file '%s'", lock->temp );
        goto Fail;
    }

    /* now attempt to link the temp file to the lock file */
    _sleep = 0;
    for ( tries = 4; tries > 0; tries-- )
    {
        struct stat  st_lock;
        int          rc;

        if (_sleep > 0) {
            if (_sleep > 2000000) {
                D( "cannot acquire lock file '%s'", lock->lock );
                goto Fail;
            }
            usleep( _sleep );
        }
        _sleep += 200000;

        /* the return value of link() is buggy on NFS */
        CHECKED(rc, link( lock->temp, lock->lock ));

        CHECKED(rc, lstat( lock->lock, &st_lock ));
        if (rc == 0 &&
            st_temp.st_rdev == st_lock.st_rdev &&
            st_temp.st_ino  == st_lock.st_ino  )
        {
            /* SUCCESS */
            lock->locked = 1;
            CHECKED(rc, unlink( lock->temp ));
            return 0;
        }

        /* if we get there, it means that the link() call failed */
        /* check the lockfile to see if it is stale              */
        if (rc == 0) {
            char    buf[16];
            time_t  now;
            int     lockpid = 0;
            int     lockfd;
            int     stale = 2;  /* means don't know */
            struct stat  st;

            CHECKED(rc, time( &now));
            st.st_mtime = now - 120;

            CHECKED(lockfd, open( lock->lock,O_RDONLY ));
            if ( lockfd >= 0 ) {
                int  len;

                CHECKED(len, read( lockfd, buf, sizeof(buf)-1 ));
                buf[len] = 0;
                lockpid = atoi(buf);

                CHECKED(rc, fstat( lockfd, &st ));
                if (rc == 0)
                  now = st.st_atime;

                CHECKED(rc, close(lockfd));
            }
            /* if there is a PID, check that it is still alive */
            if (lockpid > 0) {
                CHECKED(rc, kill( lockpid, 0 ));
                if (rc == 0 || errno == EPERM) {
                    stale = 0;
                } else if (rc < 0 && errno == ESRCH) {
                    stale = 1;
                }
            }
            if (stale == 2) {
                /* no pid, stale if the file is older than 1 minute */
                stale = (now >= st.st_mtime + 60);
            }

            if (stale) {
                D( "removing stale lockfile '%s'", lock->lock );
                CHECKED(rc, unlink( lock->lock ));
                _sleep = 0;
                tries++;
            }
        }
    }
    D("file '%s' is already in use by another process", lock->file );

Fail:
    if (f)
        fclose(f);

    if (temp_fd >= 0) {
        close(temp_fd);
    }

    if (lock_fd >= 0) {
        close(lock_fd);
    }

    unlink( lock->lock );
    unlink( lock->temp );
    return -1;
#endif
}

void
filelock_unlock( FileLock*  lock )
{
#ifdef _WIN32
    unlink_file( (char*)lock->temp );
    rmdir( (char*)lock->lock );
    lock->locked = 0;
#else
    unlink( (char*)lock->lock );
    lock->locked = 0;
#endif
}


/* used to cleanup all locks at emulator exit */
static FileLock*   _all_filelocks;

static void
filelock_atexit( void )
{
  FileLock*  lock;

  for (lock = _all_filelocks; lock != NULL; lock = lock->next)
  {
    if (lock->locked)
        filelock_unlock( lock );
  }
}

/* create a file lock */
FileLock*
filelock_create( const char*  file )
{
    int    file_len = strlen(file);
    int    lock_len = file_len + sizeof(LOCK_NAME);
#ifdef _WIN32
    int    temp_len = lock_len + 1 + sizeof(PIDFILE_NAME);
#else
    int    temp_len = file_len + sizeof(TEMP_NAME);
#endif
    int    total_len = sizeof(FileLock) + file_len + lock_len + temp_len + 3;

    FileLock*  lock = malloc(total_len);
    if (lock == NULL)
      goto Exit;

    lock->file = (const char*)(lock + 1);
    memcpy( (char*)lock->file, file, file_len+1 );

    lock->lock = lock->file + file_len + 1;
    memcpy( (char*)lock->lock, file, file_len+1 );
    strcat( (char*)lock->lock, LOCK_NAME );

    lock->temp    = (char*)lock->lock + lock_len + 1;
#ifdef _WIN32
    sprintf( (char*)lock->temp, "%s\\" PIDFILE_NAME, lock->lock );
#else
    lock->temp[0] = 0;
#endif
    lock->locked = 0;

    lock->next     = _all_filelocks;
    _all_filelocks = lock;

    if (lock->next == NULL)
        atexit( filelock_atexit );
Exit:
    return lock;
}

/** TEMP FILE SUPPORT
 **
 ** simple interface to create an empty temporary file on the system.
 **
 ** create the file with tempfile_create(), which returns a reference to a TempFile
 ** object, or NULL if your system is so weird it doesn't have a temporary directory.
 **
 ** you can then call tempfile_path() to retrieve the TempFile's real path to open
 ** it. the returned path is owned by the TempFile object and should not be freed.
 **
 ** all temporary files are destroyed when the program quits, unless you explicitely
 ** close them before that with tempfile_close()
 **/

struct TempFile
{
    const char*  name;
    TempFile*    next;
};

static void       tempfile_atexit();
static TempFile*  _all_tempfiles;

TempFile*
tempfile_create( void )
{
    TempFile*    tempfile;
    const char*  tempname = NULL;

#ifdef _WIN32
    char  temp_namebuff[MAX_PATH];
    char  temp_dir[MAX_PATH];
    char  *p = temp_dir, *end = p + sizeof(temp_dir);
    UINT  retval;

    p = bufprint_temp_dir( p, end );
    if (p >= end) {
        D( "TEMP directory path is too long" );
        return NULL;
    }

    retval = GetTempFileName(temp_dir, "TMP", 0, temp_namebuff);
    if (retval == 0) {
        D( "can't create temporary file in '%s'", temp_dir );
        return NULL;
    }

    tempname = temp_namebuff;
#else
#define  TEMPLATE  "/tmp/.android-emulator-XXXXXX"
    int   tempfd = -1;
    char  template[512];
    char  *p = template, *end = p + sizeof(template);

    p = bufprint_temp_file( p, end, "emulator-XXXXXX" );
    if (p >= end) {
        D( "Xcannot create temporary file in /tmp/android !!" );
        return NULL;
    }

    D( "template: %s", template );
    tempfd = mkstemp( template );
    if (tempfd < 0) {
        D("cannot create temporary file in /tmp/android !!");
        return NULL;
    }
    close(tempfd);
    tempname = template;
#endif
    tempfile = malloc( sizeof(*tempfile) + strlen(tempname) + 1 );
    tempfile->name = (char*)(tempfile + 1);
    strcpy( (char*)tempfile->name, tempname );

    tempfile->next = _all_tempfiles;
    _all_tempfiles = tempfile;

    if ( !tempfile->next ) {
        atexit( tempfile_atexit );
    }

    return tempfile;
}

const char*
tempfile_path(TempFile*  temp)
{
    return temp ? temp->name : NULL;
}

void
tempfile_close(TempFile*  tempfile)
{
#ifdef _WIN32
    DeleteFile(tempfile->name);
#else
    unlink(tempfile->name);
#endif
}

/** TEMP FILE CLEANUP
 **
 **/

/* we don't expect to use many temporary files */
#define MAX_ATEXIT_FDS  16

typedef struct {
    int   count;
    int   fds[ MAX_ATEXIT_FDS ];
} AtExitFds;

static void
atexit_fds_add( AtExitFds*  t, int  fd )
{
    if (t->count < MAX_ATEXIT_FDS)
        t->fds[t->count++] = fd;
    else {
        dwarning("%s: over %d calls. Program exit may not cleanup all temporary files",
            __FUNCTION__, MAX_ATEXIT_FDS);
    }
}

static void
atexit_fds_del( AtExitFds*  t, int  fd )
{
    int  nn;
    for (nn = 0; nn < t->count; nn++)
        if (t->fds[nn] == fd) {
            /* move the last element to the current position */
            t->count  -= 1;
            t->fds[nn] = t->fds[t->count];
            break;
        }
}

static void
atexit_fds_close_all( AtExitFds*  t )
{
    int  nn;
    for (nn = 0; nn < t->count; nn++)
        close(t->fds[nn]);
}

static AtExitFds   _atexit_fds[1];

void
atexit_close_fd(int  fd)
{
    if (fd >= 0)
        atexit_fds_add(_atexit_fds, fd);
}

void
atexit_close_fd_remove(int  fd)
{
    if (fd >= 0)
        atexit_fds_del(_atexit_fds, fd);
}

static void
tempfile_atexit( void )
{
    TempFile*  tempfile;

    atexit_fds_close_all( _atexit_fds );

    for (tempfile = _all_tempfiles; tempfile; tempfile = tempfile->next)
        tempfile_close(tempfile);
}


/** OTHER FILE UTILITIES
 **
 **  make_empty_file() creates an empty file at a given path location.
 **  if the file already exists, it is truncated without warning
 **
 **  copy_file() copies one file into another.
 **
 **  both functions return 0 on success, and -1 on error
 **/

int
make_empty_file( const char*  path )
{
#ifdef _WIN32
    int  fd = _creat( path, S_IWRITE );
#else
    /* on Unix, only allow the owner to read/write, since the file *
     * may contain some personal data we don't want to see exposed */
    int  fd = creat(path, S_IRUSR | S_IWUSR);
#endif
    if (fd >= 0) {
        close(fd);
        return 0;
    }
    return -1;
}

int
copy_file( const char*  dest, const char*  source )
{
    int  fd, fs, result = -1;

    if ( access(source, F_OK)  < 0 ||
         make_empty_file(dest) < 0) {
        return -1;
    }

#ifdef _WIN32
    fd = _open(dest, _O_RDWR | _O_BINARY);
    fs = _open(source, _O_RDONLY |  _O_BINARY);
#else
    fd = creat(dest, S_IRUSR | S_IWUSR);
    fs = open(source, S_IREAD);
#endif
    if (fs >= 0 && fd >= 0) {
        char buf[4096];
        ssize_t total = 0;
        ssize_t n;
        result = 0; /* success */
        while ((n = read(fs, buf, 4096)) > 0) {
            if (write(fd, buf, n) != n) {
                /* write failed. Make it return -1 so that an
                 * empty file be created. */
                D("Failed to copy '%s' to '%s': %s (%d)",
                       source, dest, strerror(errno), errno);
                result = -1;
                break;
            }
            total += n;
        }
    }

    if (fs >= 0) {
        close(fs);
    }
    if (fd >= 0) {
        close(fd);
    }
    return result;
}

int
unlink_file( const char*  path )
{
#ifdef _WIN32
    int  ret = _unlink( path );
    if (ret == -1 && errno == EACCES) {
        /* a first call to _unlink will fail if the file is set read-only */
        /* we can however try to change its mode first and call unlink    */
        /* again...                                                       */
        ret = _chmod( path, _S_IREAD | _S_IWRITE );
        if (ret == 0)
            ret = _unlink( path );
    }
    return ret;
#else
    return  unlink(path);
#endif
}


void*
load_text_file(const char *fn)
{
    char *data;
    int sz;
    int fd;

    data = NULL;
    fd = open(fn, O_BINARY | O_RDONLY);
    if(fd < 0) return NULL;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz + 1);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);
    data[sz] = 0;

    return data;

oops:
    close(fd);
    if(data != 0)
        free(data);
    return NULL;
}

/** HOST RESOLUTION SETTINGS
 **
 ** return the main monitor's DPI resolution according to the host device
 ** beware: this is not always reliable or even obtainable.
 **
 ** returns 0 on success, or -1 in case of error (e.g. the system returns funky values)
 **/

/** NOTE: the following code assumes that we exclusively use X11 on Linux, and Quartz on OS X
 **/

#ifdef _WIN32
int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    HDC  displayDC = CreateDC( "DISPLAY", NULL, NULL, NULL );
    int  xdpi, ydpi;

    if (displayDC == NULL) {
        D( "%s: could not get display DC\n", __FUNCTION__ );
        return -1;
    }
    xdpi = GetDeviceCaps( displayDC, LOGPIXELSX );
    ydpi = GetDeviceCaps( displayDC, LOGPIXELSY );

    /* sanity checks */
    if (xdpi < 20 || xdpi > 400 || ydpi < 20 || ydpi > 400) {
        D( "%s: bad resolution: xpi=%d ydpi=%d", __FUNCTION__,
                xdpi, ydpi );
        return -1;
    }

    *px_dpi = xdpi;
    *py_dpi = ydpi;
    return 0;
}
#elif defined __APPLE__
int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    fprintf(stderr, "emulator: FIXME: implement get_monitor_resolution on OS X\n" );
    return -1;
}
#else  /* Linux and others */
#include <SDL.h>
#include <SDL_syswm.h>
#include <X11/Xlib.h>
#define  MM_PER_INCH   25.4

int
get_monitor_resolution( int  *px_dpi, int  *py_dpi )
{
    SDL_SysWMinfo info;
    Display*      display;
    int           screen;
    int           width, width_mm, height, height_mm, xdpi, ydpi;

    SDL_VERSION(&info.version);

    if ( !SDL_GetWMInfo(&info) ) {
        D( "%s: SDL_GetWMInfo() failed: %s", __FUNCTION__, SDL_GetError());
        return -1;
    }

    display = info.info.x11.display;
    screen  = XDefaultScreen(display);

    width     = XDisplayWidth(display, screen);
    width_mm  = XDisplayWidthMM(display, screen);
    height    = XDisplayHeight(display, screen);
    height_mm = XDisplayHeightMM(display, screen);

    if (width_mm <= 0 || height_mm <= 0) {
        D( "%s: bad screen dimensions: width_mm = %d, height_mm = %d",
                __FUNCTION__, width_mm, height_mm);
        return -1;
    }

    D( "%s: found screen width=%d height=%d width_mm=%d height_mm=%d",
            __FUNCTION__, width, height, width_mm, height_mm );

    xdpi = width  * MM_PER_INCH / width_mm;
    ydpi = height * MM_PER_INCH / height_mm;

    if (xdpi < 20 || xdpi > 400 || ydpi < 20 || ydpi > 400) {
        D( "%s: bad resolution: xpi=%d ydpi=%d", __FUNCTION__,
                xdpi, ydpi );
        return -1;
    }

    *px_dpi = xdpi;
    *py_dpi = ydpi;

    return 0;
}
#endif



void
disable_sigalrm( signal_state_t  *state )
{
#ifdef _WIN32
    (void)state;
#else
    sigset_t  set;

    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask (SIG_BLOCK, &set, &state->old);
#endif
}

void
restore_sigalrm( signal_state_t  *state )
{
#ifdef _WIN32
    (void)state;
#else
    pthread_sigmask (SIG_SETMASK, &state->old, NULL);
#endif
}

void
sleep_ms( int  timeout_ms )
{
#ifdef _WIN32
    if (timeout_ms <= 0)
        return;

    Sleep( timeout_ms );
#else
    if (timeout_ms <= 0)
        return;

    BEGIN_NOSIGALRM
    usleep( timeout_ms*1000 );
    END_NOSIGALRM
#endif
}


extern void
print_tabular( const char** strings, int  count,
               const char*  prefix,  int  width )
{
    int  nrows, ncols, r, c, n, maxw = 0;

    for (n = 0; n < count; n++) {
        int  len = strlen(strings[n]);
        if (len > maxw)
            maxw = len;
    }
    maxw += 2;
    ncols = width/maxw;
    nrows = (count + ncols-1)/ncols;

    for (r = 0; r < nrows; r++) {
        printf( "%s", prefix );
        for (c = 0; c < ncols; c++) {
            int  index = c*nrows + r;
            if (index >= count) {
                break;
            }
            printf( "%-*s", maxw, strings[index] );
        }
        printf( "\n" );
    }
}

extern void
stralloc_tabular( stralloc_t*  out, 
                  const char** strings, int  count,
                  const char*  prefix,  int  width )
{
    int  nrows, ncols, r, c, n, maxw = 0;

    for (n = 0; n < count; n++) {
        int  len = strlen(strings[n]);
        if (len > maxw)
            maxw = len;
    }
    maxw += 2;
    ncols = width/maxw;
    nrows = (count + ncols-1)/ncols;

    for (r = 0; r < nrows; r++) {
        stralloc_add_str( out, prefix );
        for (c = 0; c < ncols; c++) {
            int  index = c*nrows + r;
            if (index >= count) {
                break;
            }
            stralloc_add_format( out, "%-*s", maxw, strings[index] );
        }
        stralloc_add_str( out, "\n" );
    }
}

extern void
buffer_translate_char( char*  buff, unsigned  len,
                       char   from, char      to )
{
    char*  p   = buff;
    char*  end = p + len;

    while (p != NULL && (p = memchr(p, from, (size_t)(end - p))) != NULL)
        *p++ = to;
}

extern void
string_translate_char( char*  str, char from, char to )
{
    char*  p = str;
    while (p != NULL && (p = strchr(p, from)) != NULL)
        *p++ = to;
}

/** DYNAMIC STRINGS
 **/

extern void
stralloc_reset( stralloc_t*  s )
{
    free(s->s);
    s->s = NULL;
    s->n = 0;
    s->a = 0;
}

extern void
stralloc_ready( stralloc_t*  s, unsigned int  len )
{
    unsigned  old_max = s->a;
    unsigned  new_max = old_max;

    while (new_max < len) {
        unsigned  new_max2 = new_max + (new_max >> 1) + 16;
        if (new_max2 < new_max)
            new_max2 = UINT_MAX;
        new_max = new_max2;
    }

    s->s = realloc( s->s, new_max );
    if (s->s == NULL) {
        derror( "%s: not enough memory to reallocate %ld bytes",
                __FUNCTION__, new_max );
        exit(1);
    }
    s->a = new_max;
}

extern void
stralloc_readyplus( stralloc_t*  s, unsigned int  len )
{
    unsigned  len2 = s->n + len;

    if (len2 < s->n) { /* overflow ? */
        derror("%s: trying to grow by too many bytes: %ld",
               __FUNCTION__, len);
        exit(1);
    }
    stralloc_ready( s, len2 );
}

extern void
stralloc_copy( stralloc_t*  s, stralloc_t*  from )
{
    stralloc_ready(s, from->n);
    memcpy( s->s, from->s, from->n );
    s->n = from->n;
}

extern void
stralloc_append( stralloc_t*  s, stralloc_t*  from )
{
    stralloc_readyplus( s, from->n );
    memcpy( s->s + s->n, from->s, from->n );
    s->n += from->n;
}

extern void
stralloc_add_c( stralloc_t*  s, int  c )
{
    stralloc_add_bytes( s, (char*)&c, 1 );
}

extern void
stralloc_add_str( stralloc_t*  s, const char*  str )
{
    stralloc_add_bytes( s, str, strlen(str) );
}

extern void
stralloc_add_bytes( stralloc_t*  s, const void*  from, unsigned len )
{
    stralloc_readyplus( s, len );
    memcpy( s->s + s->n, from, len );
    s->n += len;
}

extern char*
stralloc_cstr( stralloc_t*  s )
{
    stralloc_readyplus( s, 1 );
    s->s[s->n] = 0;
    return s->s;
}

extern void
stralloc_format( stralloc_t*  s, const char*  fmt, ... )
{
    stralloc_reset(s);
    stralloc_ready(s, 10);

    while (1) {
        int      n;
        va_list  args;

        va_start(args, fmt);
        n = vsnprintf( s->s, s->a, fmt, args );
        va_end(args);

        /* funky old C libraries returns -1 when truncation occurs */
        if (n > -1 && n < s->a) {
            s->n = n;
            break;
        }
        if (n > -1) {  /* we now precisely what we need */
            stralloc_ready( s, n+1 );
        } else {
            stralloc_ready( s, s->a*2 );
        }
    }
}

extern void
stralloc_add_format( stralloc_t*  s, const char*  fmt, ... )
{
    STRALLOC_DEFINE(s2);

    stralloc_ready(s, 10);
    while (1) {
        int      n;
        va_list  args;

        va_start(args, fmt);
        n = vsnprintf( s2->s, s2->a, fmt, args );
        va_end(args);

        /* some C libraries return -1 when truncation occurs */
        if (n > -1 && n < s2->a) {
            s2->n = n;
            break;
        }
        if (n > -1) {  /* we now precisely what we need */
            stralloc_ready( s2, n+1 );
        } else {
            stralloc_ready( s2, s2->a*2 );
        }
    }

    stralloc_append( s, s2 );
    stralloc_reset( s2 );
}


extern void
stralloc_add_quote_c( stralloc_t*  s, int  c )
{
    stralloc_add_quote_bytes( s, (char*)&c, 1 );
}

extern void
stralloc_add_quote_str( stralloc_t*  s, const char*  str )
{
    stralloc_add_quote_bytes( s, str, strlen(str) );
}

extern void
stralloc_add_quote_bytes( stralloc_t*  s, const void*  from, unsigned  len )
{
    uint8_t*   p   = (uint8_t*) from;
    uint8_t*   end = p + len;

    for ( ; p < end; p++ ) {
        int  c = p[0];

        if (c == '\\') {
            stralloc_add_str( s, "\\\\" );
        } else if (c >= ' ' && c < 128) {
            stralloc_add_c( s, c );
        } else if (c == '\n') {
            stralloc_add_str( s, "\\n" );
        } else if (c == '\t') {
            stralloc_add_str( s, "\\t" );
        } else if (c == '\r') {
            stralloc_add_str( s, "\\r" );
        } else {
            stralloc_add_format( s, "\\x%02x", c );
        }
    }
}

/** TEMP CHAR STRINGS
 **
 ** implement a circular ring of temporary string buffers
 **/

typedef struct Temptring {
    struct TempString*  next;
    char*               buffer;
    int                 size;
} TempString;

#define  MAX_TEMP_STRINGS   16

static TempString  _temp_strings[ MAX_TEMP_STRINGS ];
static int         _temp_string_n;

extern char*
tempstr_get( int  size )
{
    TempString*  t = &_temp_strings[_temp_string_n];

    if ( ++_temp_string_n >= MAX_TEMP_STRINGS )
        _temp_string_n = 0;

    size += 1;  /* reserve 1 char for terminating zero */

    if (t->size < size) {
        t->buffer = realloc( t->buffer, size );
        if (t->buffer == NULL) {
            derror( "%s: could not allocate %d bytes",
                    __FUNCTION__, size );
            exit(1);
        }
        t->size   = size;
    }
    return  t->buffer;
}

extern char*
tempstr_from_stralloc( stralloc_t*  s )
{
    char*  q = tempstr_get( s->n );

    memcpy( q, s->s, s->n );
    q[s->n] = 0;
    return q;
}

/** QUOTING
 **
 ** dumps a human-readable version of a string. this replaces
 ** newlines with \n, etc...
 **/

extern const char*
quote_bytes( const char*  str, int  len )
{
    STRALLOC_DEFINE(s);
    char*  q;

    stralloc_add_quote_bytes( s, str, len );
    q = tempstr_from_stralloc( s );
    stralloc_reset(s);
    return q;
}

extern const char*
quote_str( const char*  str )
{
    int  len = strlen(str);
    return quote_bytes( str, len );
}

/** DYNAMIC ARRAYS OF POINTERS
 **/

void
qvector_init( qvector_t*  v )
{
    v->i = NULL;
    v->n = v->a = 0;
}

void
qvector_reset( qvector_t*  v )
{
    free(v->i);
    v->i = NULL;
    v->n = v->a = 0;
}

void
qvector_ready( qvector_t*  v, unsigned  len )
{
    unsigned  old_a = v->a;
    unsigned  new_a = old_a;
    unsigned  max_a = UINT_MAX/sizeof(v->i[0]);

    if (len <= old_a)
        return;

    if (len > max_a) {
        derror("panic: %s: length too long (%d)", __FUNCTION__, len);
        exit(1);
    }

    while (new_a < len) {
        unsigned  new_a2 = new_a + (new_a >> 1) + 8;
        if (new_a2 < new_a || new_a2 > max_a)
            new_a2 = max_a;
        new_a = max_a;
    }
    v->i = realloc( v->i, new_a*sizeof(v->i[0]) );
    v->a = new_a;
}

void
qvector_readyplus( qvector_t*  v, unsigned len )
{
    unsigned  len2 = len + v->n;

    if (len2 < len) {
        derror("panic: %s: length too long (%d)", __FUNCTION__, len);
        exit(1);
    }
    qvector_ready(v, len2);
}

void
qvector_add( qvector_t*  v, void*  item )
{
    qvector_readyplus(v, 1);
    v->i[v->n] = item;
    v->n += 1;
}

int
qvector_del( qvector_t*  v, void*  item )
{
    int  index = qvector_index(v, item);
    if (index < 0) return 0;
    qvector_remove(v, index);
    return 1;
}

extern void*
qvector_get( qvector_t*  v, int  index )
{
    if ((unsigned)index >= (unsigned)v->n)
        return NULL;

    return v->i[index];
}

extern void
qvector_set( qvector_t*  v, int  index, void*  item )
{
    if ((unsigned)index < (unsigned)v->n)
        v->i[index] = item;
 }

int
qvector_len( qvector_t*  v )
{
    return v->n;
}

int
qvector_index( qvector_t*  v, void*  item )
{
    int  nn;
    for (nn = 0; nn < v->n; nn++)
        if (v->i[nn] == item)
            return nn;
    return -1;
}

void
qvector_insert( qvector_t*  v, int  index, void*  item )
{
    if (index < 0) index = 0;
    if (index > v->n) index = v->n;

    memmove( v->i + index, v->i + index + 1, sizeof(v->i[0]) );
    v->i[index] = item;
    v->n += 1;
}

void
qvector_remove( qvector_t*  v, int  index )
{
    if (index < 0 || index >= v->n )
        return;

    memmove( v->i + index + 1, v->i + index, v->n - index - 1 );
    v->n -= 1;
}

void
qvector_remove_n( qvector_t*  v, int  index, int  count )
{
    int  end = index + count;

    if (index < 0 || index >= v->n || end <= index)
        return;

    if (end > v->n) {
        end = v->n;
        count = end - index;
    }

    memmove( v->i + index + count, v->i + index, v->n - index - count );
    v->n -= count;
}

