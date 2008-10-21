#!/bin/bash
#
# this script is used to rebuild all QEMU binaries for the host
# platforms.
#
# assume that the device tree is in TOP
#

cd `dirname $0`

OS=`uname -s`
EXE=""
case "$OS" in
    Darwin)
        CPU=`uname -p`
        if [ "$CPU" = "i386" ] ; then
            OS=darwin-x86
        else
            OS=darwin-ppc
        fi
        ;;
    *_NT-*)
        OS=windows
        EXE=.exe
        ;;
esac

# select the compiler: on OS X PPC, we're forced to use gcc-3.3
# also use ccache if we can
CC=gcc
HOSTCC=gcc
cpu=$(uname -p)
if [ "$cpu" = "powerpc" ] ; then
  HOSTCC=gcc-3.3
fi

unset TOP
# if ANDROID_PRODUCT_OUT is defined we maybe in an Android build
if [ -n "$ANDROID_PRODUCT_OUT" ] ; then
    TOP=$(cd $ANDROID_PRODUCT_OUT/../../../.. && pwd)
    echo "TOP found at $TOP"
    if [ ! -f "$TOP/config/envsetup.make" ] ; then
        echo "Cannot find build system root (TOP)"
        echo "defaulting to non-Android build"
        unset TOP
    fi
fi

# normalize the TOP variable, we don't want any trailing /
IN_ANDROID_BUILD=
if [ -n "$TOP" ] ; then
    if [ ! "$(dirname $TOP)" = "." ] ; then
        TOP=$(dirname $TOP)/$(basename $TOP)
    fi
    IN_ANDROID_BUILD=1
    echo "In Android Build"
fi

TARGETS=
DEBUG=no
IGNORE_AUDIO=no
for opt do
  optarg=`expr "x$opt" : 'x[^=]*=\(.*\)'`
  case "$opt" in
  --help|-h|-\?) show_help=yes
  ;;
  --install=*) TARGETS="$TARGETS $optarg";
  ;;
  --sdl-config=*) SDL_CONFIG=$optarg
  ;;
  --cc=*) CC="$optarg" ; HOSTCC=$CC
  ;;
  --no-strip) NOSTRIP=1
  ;;
  --no-android) IN_ANDROID_BUILD=
  ;;
  --debug) DEBUG=yes
  ;;
  --android-build) IN_ANDROID_BUILD=1
  ;;
  --ignore-audio) IGNORE_AUDIO=yes
  ;;
  esac
done

if test x"$show_help" = x"yes" ; then
    cat << EOF

Usage: android-rebuild.sh [options]
Options: [defaults in brackets after descriptions]

EOF
    echo "Standard options:"
    echo "  --help                   print this message"
    echo "  --install=FILEPATH       copy emulator executable to FILEPATH [$TARGETS]"
    echo "  --no-strip               do not strip emulator executable"
    echo "  --sdl-config=FILE        use specific sdl-config script [$SDL_CONFIG]"
    echo "  --debug                  enable debug (-O0 -g) build"
    echo "  --no-android             perform clean build, without Android build tools & prebuilt"
    echo "  --ignore-audio           ignore audio messages (may build sound-less emulator)"
    echo "  --cc=PATH                specify C compiler [$CC]"
    echo ""
    exit 1
fi

if [ -n "$IN_ANDROID_BUILD" ] ; then
    # Get the value of a build variable as an absolute path.
    function get_abs_build_var()
    {
       (cd "$TOP" &&
        CALLED_FROM_SETUP=true make -f config/envsetup.make dumpvar-abs-$1)
    }

    PREBUILT=$TOP/prebuilt/$OS
    if [ ! -d $PREBUILT ] ; then
        echo "Can't find the prebuilt directory $PREBUILT in Android build"
        exit 1
    fi
    if [ -n "$USE_CCACHE" ] ; then
        CCACHE="$TOP/prebuilt/$OS/ccache$EXE"
        if [ -f $CCACHE ] ; then
                CC="$TOP/prebuilt/$OS/ccache$EXE $CC"
                HOSTCC="$CC"
        fi
    fi

    if [ -z "$SDL_CONFIG" ] ; then
        # always use our own static libSDL by default
        SDL_CONFIG=$TOP/prebuilt/$OS/sdl/bin/sdl-config
    fi
    HOST_BIN=$(get_abs_build_var HOST_OUT_EXECUTABLES)
    if [ -n "$HOST_BIN" ] ; then
        TARGETS="$TARGETS $HOST_BIN/emulator$EXE"
    fi
else
    # try to find sdl-config
    if [ -z "$SDL_CONFIG" ] ; then
        SDL_CONFIG=$(which sdl-config)
    fi
    if [ -z "$SDL_CONFIG" ] ; then
        echo "Could not find the 'sdl-config' script"
        echo "You need to have the development version of the SDL library on this machine to build this program"
        echo "See (www.libsdl.org for details)"
        if [ "$OS" = "Linux" ] ; then
            echo "Try to install the 'libsdl-dev' package on this machine"
        fi
        exit 1
    fi

    # check that the static version is usable, this is performed by the configure script
    # too, but we can be more informative when checking it here
    TMPC=/tmp/android-qemu-sdl-check.c
    TMPE=/tmp/android-qemu-sdl-check$EXE
    TMPL=/tmp/android-qemu-sdl.log
    cat > $TMPC << EOF
#include <SDL.h>
#undef main
int main( void ) { return SDL_Init (SDL_INIT_VIDEO); }
EOF
    if $HOSTCC -o $TMPE $TMPC `$SDL_CONFIG --cflags` `$SDL_CONFIG --static-libs` 2> $TMPL; then
        rm -f $TMPC $TMPE
    else
        echo "static linking with your installed SDL library doesn't work"
        echo "please correct the following compilation/link error messages, then try again"
        cat $TMPL
        rm -f $TMPL $TMPC $TMPE
        exit 1
    fi
fi

# if we're in tools/qemu, we output the build commands to the standard outputs
# if not, we hide them...
if [ -n "$SHOW_COMMANDS" -o `dirname $0` = "." ] ; then
    STDOUT=/dev/stdout
    STDERR=/dev/stderr
else
    STDOUT=/dev/null
    STDERR=/dev/null
fi

if ! [ -f $SDL_CONFIG ] ; then
    echo "SDL_CONFIG is set to '$SDL_CONFIG' which doesn't exist"
    exit 3
fi

use_sdl_config="--use-sdl-config=$SDL_CONFIG"

# use Makefile.qemu if available
#
if [ -f Makefile.qemu ] ; then
    MAKEFILE=Makefile.qemu
else
    MAKEFILE=Makefile
fi

# compute options for the 'configure' program
CONFIGURE_OPTIONS="--disable-user --disable-kqemu --enable-trace --enable-shaper"
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-skins --enable-nand --enable-sdl $use_sdl_config"

if [ "$OS" != "windows" ] ; then
    # Windows doesn't have signals, so -nand-limits cannot work on this platform
    CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-nand-limits"
fi

CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --static-png --static-sdl --target-list=arm-softmmu"

# we don't want to use the SDL audio driver when possible, since it doesn't support
# audio input. select a platform-specific one instead...
#
AUDIO_OPTIONS=""
case $OS in
    darwin*) AUDIO_OPTIONS=" --enable-coreaudio"
    ;;
    windows) AUDIO_OPTIONS=" --enable-winaudio"
    ;;
    Linux)
        if `pkg-config --exists alsa`; then
           AUDIO_OPTIONS="$AUDIO_OPTIONS --enable-alsa"
        else
            if [ "$IGNORE_AUDIO" = "no" ] ; then
                echo "please install the libasound2-dev package on this machine, or use the --ignore-audio option"
                exit 3
            fi
        fi
        if `pkg-config --exists esound`; then
           AUDIO_OPTIONS="$AUDIO_OPTIONS --enable-esd"
        else
            if [ "$IGNORE_AUDIO" = "no" ] ; then
                echo "please install the libesd0-dev package on this machine, or use the --ignore-audio option"
                exit 3
            fi
        fi
        AUDIO_OPTIONS="$AUDIO_OPTIONS --enable-oss"
    ;;
esac
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS$AUDIO_OPTIONS"

if [ "$DEBUG" = "yes" ] ; then
    CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --debug"
fi

export CC HOSTCC

echo "rebuilding the emulator binary"
if ! (
    if [ -f arm-softmmu/Makefile ] ; then
        make -f $MAKEFILE clean
    fi
    echo ./configure $CONFIGURE_OPTIONS &&
    ./configure $CONFIGURE_OPTIONS &&
    make -f $MAKEFILE -j4 ) 2>$STDERR >$STDOUT ; then
    echo "Error while rebuilding the emulator. please check the sources"
    exit 3
fi

for target in $TARGETS; do
    ( echo "copying binary to $target" &&
      cp -f arm-softmmu/qemu-system-arm$EXE $target &&
      ( if [ -z "$NOSTRIP" ] ; then
            echo "stripping $target"
            strip $target ;
        fi )
    ) ;
done

