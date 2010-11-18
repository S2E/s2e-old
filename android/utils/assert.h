/* Copyright (C) 2009 The Android Open Source Project
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
#ifndef ANDROID_UTILS_ASSERT_H
#define ANDROID_UTILS_ASSERT_H

#include <stdarg.h>

#ifdef ACONFIG_USE_ASSERT

void  _android_assert_loc(const char*  fileName,
                          long         fileLineno,
                          const char*  functionName);

#define  AASSERT_LOC()  \
    _android_assert_loc(__FILE__,__LINE__,__FUNCTION__)

#  define  AASSERT_FAIL(...) \
    android_assert_fail(__VA_ARGS__)

void __attribute__((noreturn)) android_assert_fail(const char*  messageFmt, ...);

/* Assert we never reach some code point */
#  define  AASSERT_UNREACHED(...)   \
    do { \
        AASSERT_LOC(); \
        android_assert_fail("Unreachable code"); \
    } while (0);


/* Generic assertion, must be followed by formatted string parameters */
#  define  AASSERT(cond,...)  \
    do { \
        if (!(cond)) { \
            AASSERT_LOC(); \
            android_assert_fail(__VA_ARGS__); \
        } \
    } while (0)

/* Assert a condition evaluates to a given boolean */
#  define  AASSERT_BOOL(cond_,expected_)    \
    do { \
        int  cond_result_   = !!(cond_); \
        int  cond_expected_ = !!(expected_); \
        if (cond_result_ != cond_expected_) { \
            AASSERT_LOC(); \
            android_assert_fail("%s is %s instead of %s\n",\
               #cond_, \
               cond_result_ ? "TRUE" : "FALSE", \
               cond_expected_ ? "TRUE" : "FALSE" ); \
        } \
    } while (0)

/* Assert a condition evaluates to a given integer */
#  define  AASSERT_INT(cond_,expected_)  \
    do { \
        int  cond_result_ = (cond_); \
        int  cond_expected_ = (expected_); \
        if (cond_result_ != cond_expected_) { \
            AASSERT_LOC(); \
            android_assert_fail("%s is %d instead of %d\n", \
                                #cond_ , cond_result_, cond_expected_); \
        } \
    } while (0)

#  define  AASSERT_PTR(cond_,expected_)  \
    do { \
        void*  cond_result_ = (cond_); \
        void*  cond_expected_ = (void*)(expected_); \
        if (cond_result_ != cond_expected_) { \
            AASSERT_LOC(); \
            android_assert_fail("%s is %p instead of %p\n", \
                                #cond_ , cond_result_, cond_expected_); \
        } \
    } while (0)

#  define  ANEVER_NULL(ptr_)  \
    do { \
        void*  never_ptr_ = (ptr_); \
        if (never_ptr_ == NULL) { \
            AASSERT_LOC(); \
            android_assert_fail("%s is NULL\n", #ptr_); \
        } \
    } while (0)

#else /* !ACONFIG_USE_ASSERT */

#  define AASSERT_LOC()              ((void)0)
#  define  AASSERT_FAIL(...)        ((void)0)
#  define  AASSERT_UNREACHED(...)   ((void)0)

/* for side-effects */
#  define  AASSERT(cond,...)             ((void)(cond), (void)0)
#  define  AASSERT_BOOL(cond,val)        ((void)(cond), (void)0)
#  define  AASSERT_INT(cond,val)         AASSERT_BOOL(cond,val)
#  define  AASSERT_PTR(cond,val)         AASSERT_BOOL(cond,val)
#  define  ANEVER_NULL(ptr)              ((void)(ptr), (void)0)

#endif /* !ACONFIG_USE_ASSERT */

#  define  AASSERT_TRUE(cond_)   AASSERT_BOOL(cond_,1)
#  define  AASSERT_FALSE(cond_)  AASSERT_BOOL(cond_,0)


/* this can be used to redirect the assertion log to something
 * other than stderr. Note that android_assert_fail also calls
 * android_vpanic.
 */
typedef void (*AAssertLogFunc)( const char*  fmt, va_list  args );
void  android_assert_registerLog( AAssertLogFunc  logger );

#endif /* ANDROID_UTILS_ASSERT_H */
