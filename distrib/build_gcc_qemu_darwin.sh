#!/bin/sh
# APPLE LOCAL file B&I

set -x

# set BUILD_CPLUSPLUS to 'true' if you want to compile support for C++
BUILD_CPLUSPLUS=false

# set BUILD_DOCS to 'true' to build and install the documentation
BUILD_DOCS=false

# set BUILD_SYM to 'true' to build and install symbolic binaries
BUILD_SYM=false

# -arch arguments are different than configure arguments. We need to
# translate them.

TRANSLATE_ARCH="sed -e s/ppc/powerpc/ -e s/i386/i686/"

# Build GCC the "Apple way".
# Parameters:

# The first parameter is a space-separated list of the architectures
# the compilers will run on.  For instance, "ppc i386".  If the
# current machine isn't in the list, it will (effectively) be added.
#HOSTS=`echo $1 | $TRANSLATE_ARCH `
HOSTS=i686

# The second parameter is a space-separated list of the architectures the
# compilers will generate code for.  If the current machine isn't in
# the list, a compiler for it will get built anyway, but won't be
# installed.
#TARGETS=`echo $2 | $TRANSLATE_ARCH`
TARGETS=i686

# The GNU makefile target ('bootstrap' by default).
BOOTSTRAP=${BOOTSTRAP-bootstrap}

# The B&I build srcript (~rc/bin/buildit) accepts an '-othercflags'
# command-line flag, and captures the argument to that flag in
# $RC_NONARCH_CFLAGS (and mysteriously prepends '-pipe' thereto).
# We will allow this to override the default $CFLAGS and $CXXFLAGS.

CFLAGS="-g -O2 ${RC_NONARCH_CFLAGS/-pipe/}"

# This isn't a parameter; it is the architecture of the current machine.
BUILD=`arch | $TRANSLATE_ARCH`

# The third parameter is the path to the compiler sources.  There should
# be a shell script named 'configure' in this directory.  This script
# makes a copy...
#ORIG_SRC_DIR="$3"
ORIG_SRC_DIR=`dirname $0`
ORIG_SRC_DIR=`pwd`/$ORIG_SRC_DIR

# The fourth parameter is the location where the compiler will be installed,
# normally "/usr".  You can move it once it's built, so this mostly controls
# the layout of $DEST_DIR.
#DEST_ROOT="$4"
DEST_ROOT=/

# The fifth parameter is the place where the compiler will be copied once
# it's built.
#DEST_DIR="$5"
DEST_DIR=/Volumes/Android/device/prebuilt/darwin-x86/gcc-qemu

# The sixth parameter is a directory in which to place information (like
# unstripped executables and generated source files) helpful in debugging
# the resulting compiler.
#SYM_DIR="$6"
SYM_DIR=`pwd`/sym

# The current working directory is where the build will happen.
# It may already contain a partial result of an interrupted build,
# in which case this script will continue where it left off.
DIR=`pwd`

# This isn't a parameter; it's the version of the compiler that we're
# about to build.  It's included in the names of various files and
# directories in the installed image.
VERS=`sed -n -e '/version_string/s/.*\"\([^ \"]*\)[ \"].*/\1/p' \
  < $ORIG_SRC_DIR/gcc/version.c || exit 1`

# This isn't a parameter either, it's the major version of the compiler
# to be built.  It's VERS but only up to the second '.' (if there is one).
MAJ_VERS=`echo $VERS | sed 's/\([0-9]*\.[0-9]*\)[.-].*/\1/'`

# This is the default architecture for i386 configurations.
I386_CPU="--with-arch=pentium-m --with-tune=prescott"

# This is the libstdc++ version to use.
LIBSTDCXX_VERSION=4.0.0

# Sniff to see if we can do ppc64 building.
DARWIN_VERS=8
if [ x"`file /usr/lib/crt1.o | grep 'architecture ppc64'`" == x ]; then
    DARWIN_VERS=7
fi

echo DARWIN_VERS = $DARWIN_VERS

########################################
# Run the build.

# Create the source tree we'll actually use to build, deleting
# tcl since it doesn't actually build properly in a cross environment
# and we don't really need it.
SRC_DIR=$DIR/src
rm -rf $SRC_DIR || exit 1
mkdir $SRC_DIR || exit 1
ln -s $ORIG_SRC_DIR/* $SRC_DIR/ || exit 1
rm -rf $SRC_DIR/tcl $SRC_DIR/expect $SRC_DIR/dejagnu || exit 1
# Also remove libstdc++ since it is built from a separate project.
rm -rf $SRC_DIR/libstdc++-v3 || exit 1
# Clean out old specs files
rm -f /usr/lib/gcc/*/4.0.0/specs

ENABLE_LANGUAGES="--enable-languages=c,objc"
if [ $BUILD_CPLUSPLUS = true ] ; then
    ENABLE_LANGUAGES="$ENABLE_LANGUAGES,c++,obj-c++"
fi

# These are the configure and build flags that are used.
CONFIGFLAGS="--disable-checking -enable-werror \
  --prefix=$DEST_ROOT \
  --mandir=\${prefix}/share/man \
  $ENABLE_LANGUAGES \
  --program-transform-name=/^[cg][^.-]*$/s/$/-$MAJ_VERS/ \
  --with-gxx-include-dir=\${prefix}/include/c++/$LIBSTDCXX_VERSION \
  --with-slibdir=/usr/lib \
  --build=$BUILD-apple-darwin$DARWIN_VERS
  --disable-nls"

# Figure out how many make processes to run.
SYSCTL=`sysctl -n hw.activecpu`

# hw.activecpu only available in 10.2.6 and later
if [ -z "$SYSCTL" ]; then
  SYSCTL=`sysctl -n hw.ncpu`
fi

# sysctl -n hw.* does not work when invoked via B&I chroot /BuildRoot. 
# Builders can default to 2, since even if they are single processor, 
# nothing else is running on the machine.
if [ -z "$SYSCTL" ]; then
  SYSCTL=2
fi

# The $LOCAL_MAKEFLAGS variable can be used to override $MAKEFLAGS.
MAKEFLAGS=${LOCAL_MAKEFLAGS-"-j $SYSCTL"}

# Build the native GCC.  Do this even if the user didn't ask for it
# because it'll be needed for the bootstrap.
mkdir -p $DIR/obj-$BUILD-$BUILD $DIR/dst-$BUILD-$BUILD || exit 1
cd $DIR/obj-$BUILD-$BUILD || exit 1
if [ \! -f Makefile ]; then
 $SRC_DIR/configure $CONFIGFLAGS \
   `if [ $BUILD = i686 ] ; then echo $I386_CPU ; fi` \
   --host=$BUILD-apple-darwin$DARWIN_VERS --target=$BUILD-apple-darwin$DARWIN_VERS || exit 1
fi
make $MAKEFLAGS $BOOTSTRAP CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1
if [ $BUILD_DOCS = "true" ] ; then
make $MAKEFLAGS html CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1
fi
make $MAKEFLAGS DESTDIR=$DIR/dst-$BUILD-$BUILD install-gcc install-target \
  CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1

# Add the compiler we just built to the path, giving it appropriate names.
D=$DIR/dst-$BUILD-$BUILD/$DEST_ROOT/bin
ln -f $D/gcc-$MAJ_VERS $D/gcc || exit 1
ln -f $D/gcc $D/$BUILD-apple-darwin$DARWIN_VERS-gcc || exit 1
PATH=$DIR/dst-$BUILD-$BUILD/$DEST_ROOT/bin:$PATH

# The cross-tools' build process expects to find certain programs
# under names like 'i386-apple-darwin$DARWIN_VERS-ar'; so make them.
# Annoyingly, ranlib changes behaviour depending on what you call it,
# so we have to use a shell script for indirection, grrr.
rm -rf $DIR/bin || exit 1
mkdir $DIR/bin || exit 1
for prog in ar nm ranlib strip lipo ; do
  for t in `echo $TARGETS $HOSTS | sort -u`; do
    P=$DIR/bin/${t}-apple-darwin$DARWIN_VERS-${prog}
    echo '#!/bin/sh' > $P || exit 1
    echo 'exec /usr/bin/'${prog}' $*' >> $P || exit 1
    chmod a+x $P || exit 1
  done
done
for t in `echo $1 $2 | sort -u`; do
  gt=`echo $t | $TRANSLATE_ARCH`
  P=$DIR/bin/${gt}-apple-darwin$DARWIN_VERS-as
  echo '#!/bin/sh' > $P || exit 1
  echo 'exec /usr/bin/as -arch '${t}' $*' >> $P || exit 1
  chmod a+x $P || exit 1
done
PATH=$DIR/bin:$PATH

# Build the cross-compilers, using the compiler we just built.
for t in $TARGETS ; do
 if [ $t != $BUILD ] ; then
  mkdir -p $DIR/obj-$BUILD-$t $DIR/dst-$BUILD-$t || exit 1
   cd $DIR/obj-$BUILD-$t || exit 1
   if [ \! -f Makefile ]; then
    $SRC_DIR/configure $CONFIGFLAGS --enable-werror-always \
     `if [ $t = i686 ] ; then echo $I386_CPU ; fi` \
      --program-prefix=$t-apple-darwin$DARWIN_VERS- \
      --host=$BUILD-apple-darwin$DARWIN_VERS --target=$t-apple-darwin$DARWIN_VERS || exit 1
   fi
   make $MAKEFLAGS all CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1
   make $MAKEFLAGS DESTDIR=$DIR/dst-$BUILD-$t install-gcc install-target \
     CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1

    # Add the compiler we just built to the path.
   PATH=$DIR/dst-$BUILD-$t/$DEST_ROOT/bin:$PATH
 fi
done

# Rearrange various libraries, for no really good reason.
for t in $TARGETS ; do
  DT=$DIR/dst-$BUILD-$t
  D=`echo $DT/$DEST_ROOT/lib/gcc/$t-apple-darwin$DARWIN_VERS/$VERS`
  mv $D/static/libgcc.a $D/libgcc_static.a || exit 1
  mv $D/kext/libgcc.a $D/libcc_kext.a || exit 1
  rm -r $D/static $D/kext || exit 1
done

# Build the cross-hosted compilers.
for h in $HOSTS ; do
  if [ $h != $BUILD ] ; then
    for t in $TARGETS ; do
      mkdir -p $DIR/obj-$h-$t $DIR/dst-$h-$t || exit 1
      cd $DIR/obj-$h-$t || exit 1
      if [ $h = $t ] ; then
	pp=
      else
	pp=$t-apple-darwin$DARWIN_VERS-
      fi

      if [ \! -f Makefile ]; then
        $SRC_DIR/configure $CONFIGFLAGS \
	  `if [ $t = i686 ] ; then echo $I386_CPU ; fi` \
          --program-prefix=$pp \
          --host=$h-apple-darwin$DARWIN_VERS --target=$t-apple-darwin$DARWIN_VERS || exit 1
      fi
      make $MAKEFLAGS all-gcc CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1
      make $MAKEFLAGS DESTDIR=$DIR/dst-$h-$t install-gcc \
        CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" || exit 1
    done
  fi
done

########################################
# Construct the actual destination root, by copying stuff from
# $DIR/dst-* to $DEST_DIR, with occasional 'lipo' commands.

cd $DEST_DIR || exit 1

# Clean out DEST_DIR in case -noclean was passed to buildit.
rm -rf * || exit 1

if [ $BUILD_DOCS = "true" ] ; then
# HTML documentation
HTMLDIR="/Developer/ADC Reference Library/documentation/DeveloperTools"
mkdir -p ".$HTMLDIR" || exit 1
cp -Rp $DIR/obj-$BUILD-$BUILD/gcc/HTML/* ".$HTMLDIR/" || exit 1

# Manual pages
mkdir -p .$DEST_ROOT/share || exit 1
cp -Rp $DIR/dst-$BUILD-$BUILD$DEST_ROOT/share/man .$DEST_ROOT/share/ \
  || exit 1
fi

# libexec
cd $DIR/dst-$BUILD-$BUILD$DEST_ROOT/libexec/gcc/$BUILD-apple-darwin$DARWIN_VERS/$VERS \
  || exit 1
LIBEXEC_FILES=`find . -type f -print || exit 1` 
LIBEXEC_DIRS=`find . -type d -print || exit 1`
cd $DEST_DIR || exit 1
for t in $TARGETS ; do
  DL=$DEST_ROOT/libexec/gcc/$t-apple-darwin$DARWIN_VERS/$VERS
  for d in $LIBEXEC_DIRS ; do
    mkdir -p .$DL/$d || exit 1
  done
  for f in $LIBEXEC_FILES ; do
    if file $DIR/dst-*-$t$DL/$f | grep -q 'Mach-O executable' ; then
      lipo -output .$DL/$f -create $DIR/dst-*-$t$DL/$f || exit 1
    else
      cp -p $DIR/dst-$BUILD-$t$DL/$f .$DL/$f || exit 1
    fi
  done
done

# bin
# The native drivers ('native' is different in different architectures).
BIN_FILES=`ls $DIR/dst-$BUILD-$BUILD$DEST_ROOT/bin | grep '^[^-]*-[0-9.]*$' \
  | grep -v gccbug | grep -v gcov || exit 1`
mkdir .$DEST_ROOT/bin
for f in $BIN_FILES ; do 
  lipo -output .$DEST_ROOT/bin/$f -create $DIR/dst-*$DEST_ROOT/bin/$f || exit 1
done
# gcov, which is special only because it gets built multiple times and lipo
# will complain if we try to add two architectures into the same output.
TARG0=`echo $TARGETS | cut -d ' ' -f 1`
lipo -output .$DEST_ROOT/bin/gcov-$MAJ_VERS -create \
  $DIR/dst-*-$TARG0$DEST_ROOT/bin/*gcov* || exit 1
# The fully-named drivers, which have the same target on every host.
for t in $TARGETS ; do
  lipo -output .$DEST_ROOT/bin/$t-apple-darwin$DARWIN_VERS-gcc-$VERS -create \
    $DIR/dst-*-$t$DEST_ROOT/bin/$t-apple-darwin$DARWIN_VERS-gcc-$VERS || exit 1
  if [ $BUILD_CPLUSPLUS = "true" ] ; then
      lipo -output .$DEST_ROOT/bin/$t-apple-darwin$DARWIN_VERS-g++-$VERS -create \
        $DIR/dst-*-$t$DEST_ROOT/bin/$t-apple-darwin$DARWIN_VERS-g++* || exit 1
  fi
done

# lib
mkdir -p .$DEST_ROOT/lib/gcc || exit 1
for t in $TARGETS ; do
  cp -Rp $DIR/dst-$BUILD-$t$DEST_ROOT/lib/gcc/$t-apple-darwin$DARWIN_VERS \
    .$DEST_ROOT/lib/gcc || exit 1
done

SHARED_LIBS="libgcc_s.1.dylib libgcc_s.10.4.dylib libgcc_s.10.5.dylib"
if echo $HOSTS | grep -q powerpc ; then
  SHARED_LIBS="${SHARED_LIBS} libgcc_s_ppc64.1.dylib"
fi
for l in $SHARED_LIBS ; do
  CANDIDATES=()
  for t in $TARGETS ; do
    if [ -e $DIR/dst-$t-$t$DEST_ROOT/lib/$l ] ; then
      CANDIDATES[${#CANDIDATES[*]}]=$DIR/dst-$t-$t$DEST_ROOT/lib/$l
    fi
  done
  if [ -L ${CANDIDATES[0]} ] ; then
    ln -s `readlink ${CANDIDATES[0]}` .$DEST_ROOT/lib/$l || exit 1
  else
    lipo -output .$DEST_ROOT/lib/$l -create "${CANDIDATES[@]}" || exit 1
  fi
done

if [ $BUILD_CPLUSPLUS = "true" ] ; then
for t in $TARGETS ; do
  ln -s ../../../libstdc++.6.dylib \
    .$DEST_ROOT/lib/gcc/$t-apple-darwin$DARWIN_VERS/$VERS/libstdc++.dylib \
    || exit 1
done
fi

# include
HEADERPATH=$DEST_ROOT/include/gcc/darwin/$MAJ_VERS
mkdir -p .$HEADERPATH || exit 1

# Some headers are installed from more-hdrs/.  They all share
# one common feature: they shouldn't be installed here.  Sometimes,
# they should be part of FSF GCC and installed from there; sometimes,
# they should be installed by some completely different package; sometimes,
# they only exist for codewarrior compatibility and codewarrior should provide
# its own.  We take care not to install the headers if Libc is already
# providing them.
cd $SRC_DIR/more-hdrs
for h in `echo *.h` ; do
  if [ ! -f /usr/include/$h -o -L /usr/include/$h ] ; then
    cp -R $h $DEST_DIR$HEADERPATH/$h || exit 1
    for t in $TARGETS ; do
      THEADERPATH=$DEST_DIR$DEST_ROOT/lib/gcc/${t}-apple-darwin$DARWIN_VERS/$VERS/include
      [ -f $THEADERPATH/$h ] || \
        ln -s ../../../../../include/gcc/darwin/$MAJ_VERS/$h $THEADERPATH/$h || \
        exit 1
    done
  fi
done
mkdir -p $DEST_DIR$HEADERPATH/machine
for h in `echo */*.h` ; do
  if [ ! -f /usr/include/$h -o -L /usr/include/$h ] ; then
    cp -R $h $DEST_DIR$HEADERPATH/$h || exit 1
    for t in $TARGETS ; do
      THEADERPATH=$DEST_DIR$DEST_ROOT/lib/gcc/${t}-apple-darwin$DARWIN_VERS/$VERS/include
      mkdir -p $THEADERPATH/machine
      # In fixincludes/fixinc.in we created this file...  always link for now
      [ -f /disable/$THEADERPATH/$h ] || \
        ln -f -s ../../../../../../include/gcc/darwin/$MAJ_VERS/$h $THEADERPATH/$h || \
        exit 1
    done
  fi
done

if [ $BUILD_DOCS = "true" ] ; then
if [ $BUILD_CPLUSPLUS = "true" ] ; then
# Add extra man page symlinks for 'c++' and for arch-specific names.
MDIR=$DEST_DIR$DEST_ROOT/share/man/man1
ln -f $MDIR/g++-$MAJ_VERS.1 $MDIR/c++-$MAJ_VERS.1 || exit 1
for t in $TARGETS ; do
  ln -f $MDIR/gcc-$MAJ_VERS.1 $MDIR/$t-apple-darwin$DARWIN_VERS-gcc-$VERS.1 \
      || exit 1
  ln -f $MDIR/g++-$MAJ_VERS.1 $MDIR/$t-apple-darwin$DARWIN_VERS-g++-$VERS.1 \
      || exit 1
done
fi
fi

# Build driver-driver using fully-named drivers
for h in $HOSTS ; do
    $DEST_DIR$DEST_ROOT/bin/$h-apple-darwin$DARWIN_VERS-gcc-$VERS                          \
	$ORIG_SRC_DIR/gcc/config/darwin-driver.c                               \
	-DPDN="\"-apple-darwin$DARWIN_VERS-gcc-$VERS\""                                    \
	-DIL="\"$DEST_ROOT/bin/\"" -I  $ORIG_SRC_DIR/include                   \
	-I  $ORIG_SRC_DIR/gcc -I  $ORIG_SRC_DIR/gcc/config                     \
	-liberty -L$DIR/dst-$BUILD-$h$DEST_ROOT/lib/                           \
	-L$DIR/dst-$BUILD-$h$DEST_ROOT/$h-apple-darwin$DARWIN_VERS/lib/                    \
        -L$DIR/obj-$h-$BUILD/libiberty/                                        \
	-o $DEST_DIR/$DEST_ROOT/bin/tmp-$h-gcc-$MAJ_VERS || exit 1

    if [ $BUILD_CPLUSPLUS = "true" ] ; then
    $DEST_DIR$DEST_ROOT/bin/$h-apple-darwin$DARWIN_VERS-gcc-$VERS                          \
	$ORIG_SRC_DIR/gcc/config/darwin-driver.c                               \
	-DPDN="\"-apple-darwin$DARWIN_VERS-g++-$VERS\""                                    \
	-DIL="\"$DEST_ROOT/bin/\"" -I  $ORIG_SRC_DIR/include                   \
	-I  $ORIG_SRC_DIR/gcc -I  $ORIG_SRC_DIR/gcc/config                     \
	-liberty -L$DIR/dst-$BUILD-$h$DEST_ROOT/lib/                           \
	-L$DIR/dst-$BUILD-$h$DEST_ROOT/$h-apple-darwin$DARWIN_VERS/lib/                    \
        -L$DIR/obj-$h-$BUILD/libiberty/                                        \
	-o $DEST_DIR/$DEST_ROOT/bin/tmp-$h-g++-$MAJ_VERS || exit 1
    fi
done

lipo -output $DEST_DIR/$DEST_ROOT/bin/gcc-$MAJ_VERS -create \
  $DEST_DIR/$DEST_ROOT/bin/tmp-*-gcc-$MAJ_VERS || exit 1

if [ $BUILD_CPLUSPLUS = "true" ] ; then
lipo -output $DEST_DIR/$DEST_ROOT/bin/g++-$MAJ_VERS -create \
  $DEST_DIR/$DEST_ROOT/bin/tmp-*-g++-$MAJ_VERS || exit 1

ln -f $DEST_DIR/$DEST_ROOT/bin/g++-$MAJ_VERS $DEST_DIR/$DEST_ROOT/bin/c++-$MAJ_VERS || exit 1
fi

rm $DEST_DIR/$DEST_ROOT/bin/tmp-*-gcc-$MAJ_VERS || exit 1 
if [ $BUILD_CPLUPLUS = "true" ] ; then
rm $DEST_DIR/$DEST_ROOT/bin/tmp-*-g++-$MAJ_VERS || exit 1
fi

########################################
# Save the source files and objects needed for debugging
if [ $BUILD_SYM = "true" ] ; then
cd $SYM_DIR || exit 1

# Clean out SYM_DIR in case -noclean was passed to buildit.
rm -rf * || exit 1

# Save executables and libraries.
cd $DEST_DIR || exit 1
find . \( -perm -0111 -or -name \*.a -or -name \*.dylib \) -type f -print \
  | cpio -pdml $SYM_DIR || exit 1
# Save source files.
mkdir $SYM_DIR/src || exit 1
cd $DIR || exit 1
find obj-* -name \*.\[chy\] -print | cpio -pdml $SYM_DIR/src || exit 1
fi

########################################
# Strip the executables and libraries
find $DEST_DIR -perm -0111 \! -name \*.dylib \! -name fixinc.sh \
    \! -name mkheaders -type f -print \
  | xargs strip || exit 1
find $DEST_DIR \( -name \*.a -or -name \*.dylib \) \
    \! -name libgcc_s.10.*.dylib -type f -print \
  | xargs strip -SX || exit 1
find $DEST_DIR -name \*.a -type f -print \
  | xargs ranlib || exit 1
chgrp -h -R wheel $DEST_DIR
chgrp -R wheel $DEST_DIR

#########################################3
# Rename the executables
FILES="gcc cpp"
if [ $BUILD_CPLUSPLUS = "true" ] ; then
    FILES="$FILES g++"
fi
for ff in $FILES; do
    ln -f $DEST_DIR/bin/$ff-$MAJ_VERS $DEST_DIR/bin/$ff || exit 1
done

# Done!
exit 0
