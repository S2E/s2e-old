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
#ifndef _ANDROID_UTILS_H
#define _ANDROID_UTILS_H

#include "android_debug.h"
#include <stdint.h>  /* for uint64_t */

#ifndef STRINGIFY
#define  _STRINGIFY(x)  #x
#define  STRINGIFY(x)  _STRINGIFY(x)
#endif

#ifndef GLUE
#define  _GLUE(x,y)  x##y
#define  GLUE(x,y)   _GLUE(x,y)

#define  _GLUE3(x,y,z)  x##y##z
#define  GLUE3(x,y,z)    _GLUE3(x,y,z)
#endif

/* O_BINARY is required in the MS C library to avoid opening file
 * in text mode (the default, ahhhhh)
 */
#if !defined(_WIN32) && !defined(O_BINARY)
#  define  O_BINARY  0
#endif

/* define  PATH_SEP as a string containing the directory separateor */
#ifdef _WIN32
#  define  PATH_SEP   "\\"
#else
#  define  PATH_SEP   "/"
#endif

/* get MAX_PATH, note that MAX_PATH is set to 260 on Windows for
 * stupid backwards-compatibility reason, though any 32-bit version
 * of the OS handles much much longer paths
 */
#ifdef _WIN32
#  undef   MAX_PATH
#  define  MAX_PATH    1024
#else
#  include <limits.h>
#  define  MAX_PATH    PATH_MAX
#endif

/** NON-GRAPHIC USAGE
 **
 ** this variable is TRUE if the -no-window argument was used.
 ** the emulator will still simulate a framebuffer according to the selected skin
 ** but no window will be displayed on the host computer.
 **/
extern int    arg_no_window;

/** EINTR HANDLING
 **
 ** since QEMU uses SIGALRM pretty extensively, having a system call returning
 ** EINTR on Unix happens very frequently. provide a simple macro to guard against
 ** this.
 **/

#ifdef _WIN32
#  define   CHECKED(ret, call)    (ret) = (call)
#else
#  define   CHECKED(ret, call)    do { (ret) = (call); } while ((ret) < 0 && errno == EINTR)
#endif

/** MISC FILE AND DIRECTORY HANDLING
 **/

/* checks that a given file exists */
extern int   path_exists( const char*  path );

/* checks that a path points to a regular file */
extern int   path_is_regular( const char*  path );

/* checks that a path points to a directory */
extern int   path_is_dir( const char*  path );

/* checks that one can read/write a given (regular) file */
extern int   path_can_read( const char*  path );
extern int   path_can_write( const char*  path );

/* try to make a directory. returns 0 on success, -1 on error */
extern int   path_mkdir( const char*  path, int  mode );

/* ensure that a given directory exists, create it if not, 
   0 on success, -1 on error */
extern int   path_mkdir_if_needed( const char*  path, int  mode );

/* return the size of a given file in '*psize'. returns 0 on
 * success, -1 on failure (error code in errno) */
extern int   path_get_size( const char*  path, uint64_t  *psize );

/** PATH HANDLING ROUTINES
 **
 **  path_parent() can be used to return the n-level parent of a given directory
 **  this understands . and .. when encountered in the input path
 **/

extern char*  path_parent( const char*  path, int  levels );

/** FORMATTED BUFFER PRINTING
 **
 **  bufprint() allows your to easily and safely append formatted string
 **  content to a given bounded character buffer, in a way that is easier
 **  to use than raw snprintf()
 **
 **  'buffer'  is the start position in the buffer,
 **  'buffend' is the end of the buffer, the function assumes (buffer <= buffend)
 **  'format'  is a standard printf-style format string, followed by any number
 **            of formatting arguments
 **
 **  the function returns the next position in the buffer if everything fits
 **  in it. in case of overflow or formatting error, it will always return "buffend"
 **
 **  this allows you to chain several calls to bufprint() and only check for
 **  overflow at the end, for exemple:
 **
 **     char   buffer[1024];
 **     char*  p   = buffer;
 **     char*  end = p + sizeof(buffer);
 **
 **     p = bufprint(p, end, "%s/%s", first, second);
 **     p = bufprint(p, end, "/%s", third);
 **     if (p >= end) ---> overflow
 **
 **  as a convenience, the appended string is zero-terminated if there is no overflow.
 **  (this means that even if p >= end, the content of "buffer" is zero-terminated)
 **
 **  vbufprint() is a variant that accepts a va_list argument
 **/

extern char*   vbufprint(char*  buffer, char*  buffend, const char*  fmt, va_list  args );
extern char*   bufprint (char*  buffer, char*  buffend, const char*  fmt, ... );

/** USEFUL DIRECTORY SUPPORT
 **
 **  bufprint_add_dir() appends the application's directory to a given bounded buffer
 **
 **  bufprint_config_path() appends the applications' user-specific configuration directory
 **  to a bounded buffer. on Unix this is usually ~/.android, and something a bit more
 **  complex on Windows
 **
 **  bufprint_config_file() appends the name of a file or directory relative to the
 **  user-specific configuration directory to a bounded buffer. this really is equivalent
 **  to concat-ing the config path + path separator + 'suffix'
 **
 **  bufprint_temp_dir() appends the temporary directory's path to a given bounded buffer
 **
 **  bufprint_temp_file() appens the name of a file or directory relative to the
 **  temporary directory. equivalent to concat-ing the temp path + path separator + 'suffix'
 **/

extern char*  bufprint_app_dir    (char*  buffer, char*  buffend);
extern char*  bufprint_config_path(char*  buffer, char*  buffend);
extern char*  bufprint_config_file(char*  buffer, char*  buffend, const char*  suffix);
extern char*  bufprint_temp_dir   (char*  buffer, char*  buffend);
extern char*  bufprint_temp_file  (char*  buffer, char*  buffend, const char*  suffix);

/** FILE LOCKS SUPPORT
 **
 ** a FileLock is useful to prevent several emulator instances from using the same
 ** writable file (e.g. the userdata.img disk images).
 **
 ** create a FileLock object with filelock_create(), note that the function will *not*
 ** return NULL if the file doesn't exist.
 *
 *  then call filelock_lock() to try to acquire a lock for the corresponding file.
 ** returns 0 on success, or -1 in case of error, which means that another program
 ** is using the file or that the directory containing the file is read-only.
 **
 ** all file locks are automatically released and destroyed when the program exits.
 ** the filelock_lock() function can also detect stale file locks that can linger
 ** when the emulator crashes unexpectedly, and will happily clean them for you
 **/

typedef struct FileLock  FileLock;

extern FileLock*  filelock_create( const char*  path );
extern int        filelock_lock( FileLock*  lock );
extern void       filelock_unlock( FileLock*  lock );

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

typedef struct TempFile   TempFile;

extern  TempFile*    tempfile_create( void );
extern  const char*  tempfile_path( TempFile*  temp );
extern  void         tempfile_close( TempFile*  temp );

/** TEMP FILE CLEANUP
 **
 ** We delete all temporary files in atexit()-registered callbacks.
 ** however, the Win32 DeleteFile is unable to remove a file unless
 ** all HANDLEs to it are closed in the terminating process.
 **
 ** Call 'atexit_close_fd' on a newly open-ed file descriptor to indicate
 ** that you want it closed in atexit() time. You should always call
 ** this function unless you're certain that the corresponding file
 ** cannot be temporary.
 **
 ** Call 'atexit_close_fd_remove' before explicitely closing a 'fd'
 **/
extern void          atexit_close_fd(int  fd);
extern void          atexit_close_fd_remove(int  fd);

/** OTHER FILE UTILITIES
 **
 **  make_empty_file() creates an empty file at a given path location.
 **  if the file already exists, it is truncated without warning
 **
 **  copy_file() copies one file into another.
 **
 **  unlink_file() is equivalent to unlink() on Unix, on Windows,
 **  it will handle the case where _unlink() fails because the file is
 **  read-only by trying to change its access rights then calling _unlink()
 **  again.
 **
 **  these functions return 0 on success, and -1 on error
 **
 **  load_text_file() reads a file into a heap-allocated memory block,
 **  and appends a 0 to it. the caller must free it
 **/

extern int     make_empty_file( const char*  path );
extern int     copy_file( const char*  dest, const char*  source );
extern int     unkink_file( const char*  path );
extern void*   load_text_file( const char*  path );

/** HOST RESOLUTION SETTINGS
 **
 ** return the main monitor's DPI resolution according to the host device
 ** beware: this is not always reliable or even obtainable.
 **
 ** returns 0 on success, or -1 in case of error (e.g. the system returns funky values)
 **/
extern  int    get_monitor_resolution( int  *px_dpi, int  *py_dpi );

/** SIGNAL HANDLING
 **
 ** the following can be used to block SIGALRM for a given period of time.
 ** use with caution, the QEMU execution loop uses SIGALRM extensively
 **
 **/
#ifdef _WIN32
typedef struct { int  dumy; }      signal_state_t;
#else
#include <signal.h>
typedef struct { sigset_t  old; }  signal_state_t;
#endif

extern  void   disable_sigalrm( signal_state_t  *state );
extern  void   restore_sigalrm( signal_state_t  *state );

#ifdef _WIN32

#define   BEGIN_NOSIGALRM  \
    {

#define   END_NOSIGALRM  \
    }

#else /* !WIN32 */

#define   BEGIN_NOSIGALRM  \
    { signal_state_t  __sigalrm_state; \
      disable_sigalrm( &__sigalrm_state );

#define   END_NOSIGALRM  \
      restore_sigalrm( &__sigalrm_state );  \
    }

#endif /* !WIN32 */

/** TIME HANDLING
 **
 ** sleep for a given time in milliseconds. note: this uses
 ** disable_sigalrm()/restore_sigalrm()
 **/

extern  void   sleep_ms( int  timeout );

/** TABULAR OUTPUT
 **
 ** prints a list of strings in row/column format
 **
 **/

extern void   print_tabular( const char** strings, int  count,
                             const char*  prefix,  int  width );

/** CHARACTER TRANSLATION
 **
 ** converts one character into another in strings
 **/

extern void   buffer_translate_char( char*  buff, unsigned  len,
                                     char   from, char      to );

extern void   string_translate_char( char*  str, char from, char to );

/** DYNAMIC STRINGS
 **/

typedef struct {
    char*     s;
    unsigned  n;
    unsigned  a;
} stralloc_t;

#define  STRALLOC_INIT        { NULL, 0, 0 }
#define  STRALLOC_DEFINE(s)   stralloc_t   s[1] = { STRALLOC_INIT }

extern void   stralloc_reset( stralloc_t*  s );
extern void   stralloc_ready( stralloc_t*  s, unsigned  len );
extern void   stralloc_readyplus( stralloc_t*  s, unsigned  len );

extern void   stralloc_copy( stralloc_t*  s, stralloc_t*  from );
extern void   stralloc_append( stralloc_t*  s, stralloc_t*  from );

extern void   stralloc_add_c( stralloc_t*  s, int  c );
extern void   stralloc_add_str( stralloc_t*  s, const char*  str );
extern void   stralloc_add_bytes( stralloc_t*  s, const void*  from, unsigned  len );

extern char*  stralloc_cstr( stralloc_t*  s );

extern void   stralloc_format( stralloc_t*  s, const char*  fmt, ... );
extern void   stralloc_add_format( stralloc_t*  s, const char*  fmt, ... );

extern void   stralloc_add_quote_c( stralloc_t*  s, int  c );
extern void   stralloc_add_quote_str( stralloc_t*  s, const char*  str );
extern void   stralloc_add_quote_bytes( stralloc_t*  s, const void*  from, unsigned   len );

extern void   stralloc_tabular( stralloc_t*  s, const char** strings, int  count,
                                                const char*  prefix,  int  width );


/** TEMP CHAR STRINGS
 **
 ** implement a circular ring of temporary string buffers
 **/

extern char*  tempstr_get( int   size );
extern char*  tempstr_format( const char*  fmt, ... );
extern char*  tempstr_from_stralloc( stralloc_t*  s );

/** QUOTING
 **
 ** dumps a human-readable version of a string. this replaces
 ** newlines with \n, etc...
 **/

extern const char*   quote_bytes( const char*  str, int  len );
extern const char*   quote_str( const char*  str );

/** DYNAMIC ARRAYS OF POINTERS
 **/

typedef struct {
    void**     i;
    unsigned   n;
    unsigned   a;
} qvector_t;

#define  QVECTOR_INIT                   { NULL, 0, 0 }
#define  QVECTOR_DEFINE(v)  qvector_t   v[1] = QVECTOR_INIT

extern void   qvector_init( qvector_t*  v );
extern void   qvector_reset( qvector_t*  v );
extern void   qvector_ready( qvector_t*  v, unsigned  len );
extern void   qvector_readyplus( qvector_t*  v, unsigned len );
extern void   qvector_add( qvector_t*  v, void*  item );
extern int    qvector_del( qvector_t*  v, void*  item );  /* returns 1 if deleted, 0 otherwise */
extern void*  qvector_get( qvector_t*  v, int  index );
extern int    qvector_len( qvector_t*  v );
extern int    qvector_index( qvector_t*  v, void*  item );  /* returns -1 is not found */
extern void   qvector_insert( qvector_t*  v, int  index, void*  item );
extern void   qvector_remove( qvector_t*  v, int  index );
extern void   qvector_remove_n( qvector_t*  v, int  index, int  count );

#endif /* _ANDROID_UTILS_H */
