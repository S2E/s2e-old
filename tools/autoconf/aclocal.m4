# generated automatically by aclocal 1.11.1 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005, 2006, 2007, 2008, 2009  Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

#
# Configure a Makefile without clobbering it if it exists and is not out of
# date.  This macro is unique to LLVM.
#
AC_DEFUN([AC_CONFIG_MAKEFILE],
[AC_CONFIG_COMMANDS($1,
  [${llvm_src}/autoconf/mkinstalldirs `dirname $1`
   ${SHELL} ${llvm_src}/autoconf/install-sh -m 0644 -c ${srcdir}/$1 $1])
])

#
# Provide the arguments and other processing needed for an LLVM project
#
AC_DEFUN([LLVM_CONFIG_PROJECT],
  [AC_ARG_WITH([llvmsrc],
    AS_HELP_STRING([--with-llvmsrc],[Location of LLVM Source Code]),
    [llvm_src="$withval"],[llvm_src="]$1["])
  AC_SUBST(LLVM_SRC,$llvm_src)
  AC_ARG_WITH([llvmobj],
    AS_HELP_STRING([--with-llvmobj],[Location of LLVM Object Code]),
    [llvm_obj="$withval"],[llvm_obj="]$2["])
  AC_SUBST(LLVM_OBJ,$llvm_obj)
  AC_CONFIG_COMMANDS([setup],,[llvm_src="${LLVM_SRC}"])
])

dnl Check for a reasonable version of Perl.
dnl   $1 - Minimum Perl version.  Typically 5.006.
dnl 
AC_DEFUN([LLVM_PROG_PERL], [
AC_PATH_PROG(PERL, [perl], [none])
if test "$PERL" != "none"; then
  AC_MSG_CHECKING(for Perl $1 or newer)
  if $PERL -e 'use $1;' 2>&1 > /dev/null; then
    AC_MSG_RESULT(yes)
  else
    PERL=none
    AC_MSG_RESULT(not found)
  fi
fi
])


