#!/bin/sh
#
# this script is used to rebuild the Android emulator from sources
# in the current directory. It also contains logic to speed up the
# rebuild if it detects that you're using the Android build system
#
# in this case, it will use prebuilt binaries for the compiler,
# the audio library and the SDL library. You can disable this
# by using the --no-prebuilt-libs and --cc=<compiler> options
#
#
# here's the list of environment variables you can define before
# calling this script to control it (besides options):
#
#

# first, let's see which system we're running this on
cd `dirname $0`

# source common functions definitions
. android/build/common.sh

# Parse options
OPTION_TARGETS=""
OPTION_DEBUG=no
OPTION_IGNORE_AUDIO=no
OPTION_NO_PREBUILTS=no
OPTION_TRY_64=no
OPTION_HELP=no
OPTION_DEBUG=no

if [ -z "$CC" ] ; then
  CC=gcc
fi

for opt do
  optarg=`expr "x$opt" : 'x[^=]*=\(.*\)'`
  case "$opt" in
  --help|-h|-\?) OPTION_HELP=yes
  ;;
  --verbose)
    if [ "$VERBOSE" = "yes" ] ; then
        VERBOSE2=yes
    else
        VERBOSE=yes
    fi
  ;;
  --debug) OPTION_DEBUG=yes
  ;;
  --install=*) OPTION_TARGETS="$OPTION_TARGETS $optarg";
  ;;
  --sdl-config=*) SDL_CONFIG=$optarg
  ;;
  --cc=*) CC="$optarg" ; HOSTCC=$CC
  ;;
  --no-strip) OPTION_NO_STRIP=yes
  ;;
  --debug) OPTION_DEBUG=yes
  ;;
  --ignore-audio) OPTION_IGNORE_AUDIO=yes
  ;;
  --no-prebuilts) OPTION_NO_PREBUILTS=yes
  ;;
  --try-64) OPTION_TRY_64=yes
  ;;
  *)
    echo "unknown option '$opt', use --help"
    exit 1
  esac
done

# Print the help message
#
if [ "$OPTION_HELP" = "yes" ] ; then
    cat << EOF

Usage: rebuild.sh [options]
Options: [defaults in brackets after descriptions]
EOF
    echo "Standard options:"
    echo "  --help                   print this message"
    echo "  --install=FILEPATH       copy emulator executable to FILEPATH [$TARGETS]"
    echo "  --cc=PATH                specify C compiler [$CC]"
    echo "  --sdl-config=FILE        use specific sdl-config script [$SDL_CONFIG]"
    echo "  --no-strip               do not strip emulator executable"
    echo "  --debug                  enable debug (-O0 -g) build"
    echo "  --ignore-audio           ignore audio messages (may build sound-less emulator)"
    echo "  --no-prebuilts           do not use prebuilt libraries and compiler"
    echo "  --try-64                 try to build a 64-bit executable (may crash)"
    echo "  --verbose                verbose configuration"
    echo "  --debug                  build debug version of the emulator"
    echo ""
    exit 1
fi

# we only support generating 32-bit binaris on 64-bit systems.
# And we may need to add a -Wa,--32 to CFLAGS to let the assembler
# generate 32-bit binaries on Linux x86_64.
#
if [ "$OPTION_TRY_64" != "yes" ] ; then
    force_32bit_binaries
fi

# Are we running in the Android build system ?
check_android_build


# Adjust a few things when we're building within the Android build
# system:
#    - locate prebuilt directory
#    - locate and use prebuilt libraries
#    - copy the new binary to the correct location
#
if [ "$OPTION_NO_PREBUILTS" = "yes" ] ; then
    IN_ANDROID_BUILD=no
fi

if [ "$IN_ANDROID_BUILD" = "yes" ] ; then
    locate_android_prebuilt

    # use ccache if USE_CCACHE is defined and the corresponding
    # binary is available.
    #
    # note: located in PREBUILT/ccache/ccache in the new tree layout
    #       located in PREBUILT/ccache in the old one
    #
    if [ -n "$USE_CCACHE" ] ; then
        CCACHE="$ANDROID_PREBUILT/ccache/ccache$EXE"
        if [ ! -f $CCACHE ] ; then
            CCACHE="$ANDROID_PREBUILT/ccache$EXE"
        fi
        if [ -f $CCACHE ] ; then
            CC="$CCACHE $CC"
        fi
        log "Prebuilt   : CCACHE=$CCACHE"
    fi

    # if the user didn't specify an sdl-config script, get the prebuilt one
    if [ -z "$SDL_CONFIG" -a "$OPTION_NO_PREBUILTS" = "no" ] ; then
        # always use our own static libSDL by default
        SDL_CONFIG=$ANDROID_PREBUILT/sdl/bin/sdl-config
        log "Prebuilt   : SDL_CONFIG=$SDL_CONFIG"
    fi

    # finally ensure that our new binary is copied to the 'out'
    # subdirectory as 'emulator'
    HOST_BIN=$(get_android_abs_build_var HOST_OUT_EXECUTABLES)
    if [ -n "$HOST_BIN" ] ; then
        OPTION_TARGETS="$OPTION_TARGETS $HOST_BIN/emulator$EXE"
        log "Targets    : TARGETS=$OPTION_TARGETS"
    fi
fi  # IN_ANDROID_BUILD = no


# we can build the emulator with Cygwin, so enable it
enable_cygwin

setup_toolchain

###
###  SDL Probe
###

# if the user didn't specify a sdl-config script, get the prebuilt one
if [ -z "$SDL_CONFIG" -a "$OPTION_NO_PREBUILTS" = "no" ] ; then
    #  try to find one from our git repository
    SDL_CONFIG=../sdl/out/$OS/bin/sdl-config
    if [ -f $SDL_CONFIG ] ; then
        log "Prebuilt   : SDL_CONFIG=$SDL_CONFIG"
    else
        echo "WARNING: YOU SHOULD USE THE --sdl-config OPTION"
        SDL_CONFIG=
    fi
fi

# For now, we require an external libSDL library, if SDL_CONFIG is not
# defined, try to grab it from the environment
#
if [ -z "$SDL_CONFIG" ] ; then
    SDL_CONFIG=`which sdl-config`
    if [ $? != 0 ] ; then
        echo "Please ensure that you have the emulator's patched libSDL"
        echo "built somewhere and point to its sdl-config script either"
        echo "with the SDL_CONFIG env. variable, or the --sdl-config=<script>"
        echo "option."
        clean_exit
    fi
fi

# check that we can link statically with the library.
#
SDL_CFLAGS=`$SDL_CONFIG --cflags`
SDL_LIBS=`$SDL_CONFIG --static-libs`

# quick hack, remove the -D_GNU_SOURCE=1 of some SDL Cflags
# since they break recent Mingw releases
SDL_CFLAGS=`echo $SDL_CFLAGS | sed -e s/-D_GNU_SOURCE=1//g`

log "SDL-probe  : SDL_CFLAGS = $SDL_CFLAGS"
log "SDL-probe  : SDL_LIBS   = $SDL_LIBS"


EXTRA_CFLAGS="$SDL_CFLAGS"
EXTRA_LDFLAGS="$SDL_LIBS"

cat > $TMPC << EOF
#include <SDL.h>
#undef main
int main( int argc, char** argv ) {
   return SDL_Init (SDL_INIT_VIDEO); 
}
EOF
feature_check_link  SDL_LINKING

if [ $SDL_LINKING != "yes" ] ; then
    echo "You provided an explicit sdl-config script, but the corresponding library"
    echo "cannot be statically linked with the Android emulator directly."
    echo "Error message:"
    cat $TMPL
    clean_exit
fi
log "SDL-probe  : static linking ok"

# now, let's check that the SDL library has the special functions
# we added to our own sources
#
cat > $TMPC << EOF
#include <SDL.h>
#undef main
int main( int argc, char** argv ) {
    int  x, y;
    SDL_Rect  r;
    SDL_WM_GetPos(&x, &y);
    SDL_WM_SetPos(x, y);
    SDL_WM_GetMonitorDPI(&x, &y);
    SDL_WM_GetMonitorRect(&r);
    return SDL_Init (SDL_INIT_VIDEO); 
}
EOF
feature_check_link  SDL_LINKING

if [ $SDL_LINKING != "yes" ] ; then
    echo "You provided an explicit sdl-config script in SDL_CONFIG, but the"
    echo "corresponding library doesn't have the patches required to link"
    echo "with the Android emulator. Unsetting SDL_CONFIG will use the"
    echo "sources bundled with the emulator instead"
    echo "Error:"
    cat $TMPL
    clean_exit
fi

log "SDL-probe  : extra features ok"
clean_temp

EXTRA_CFLAGS=
EXTRA_LDFLAGS=

###
###  Audio subsystems probes
###
PROBE_COREAUDIO=no
PROBE_ALSA=no
PROBE_OSS=no
PROBE_ESD=no
PROBE_WINAUDIO=no

case "$OS" in
    darwin*) PROBE_COREAUDIO=yes;
    ;;
    linux-*) PROBE_ALSA=yes; PROBE_OSS=yes; PROBE_ESD=yes;
    ;;
    windows) PROBE_WINAUDIO=yes
    ;;
esac

ORG_CFLAGS=$CFLAGS
ORG_LDFLAGS=$LDFLAGS

if [ "$PROBE_ESD" = yes ] ; then
    CFLAGS="$ORG_CFLAGS"
    LDFLAGS="$ORG_LDFLAGS -ldl"
    cp -f android/config/check-esd.c $TMPC
    compile && link && $TMPE
    if [ $? = 0 ] ; then
        log "AudioProbe : ESD seems to be usable on this system"
    else
        if [ "$OPTION_IGNORE_AUDIO" = no ] ; then
            echo "the EsounD development files do not seem to be installed on this system"
            echo "Are you missing the libesd-dev package ?"
            echo "Correct the errors below and try again:"
            cat $TMPL
            clean_exit
        fi
        PROBE_ESD=no
        log "AudioProbe : ESD seems to be UNUSABLE on this system !!"
    fi
fi

if [ "$PROBE_ALSA" = yes ] ; then
    CFLAGS="$ORG_CFLAGS"
    LDFLAGS="$ORG_CFLAGS -ldl"
    cp -f android/config/check-alsa.c $TMPC
    compile && link && $TMPE
    if [ $? = 0 ] ; then
        log "AudioProbe : ALSA seems to be usable on this system"
    else
        if [ "$OPTION_IGNORE_AUDIO" = no ] ; then
            echo "the ALSA development files do not seem to be installed on this system"
            echo "Are you missing the libasound-dev package ?"
            echo "Correct the erros below and try again"
            cat $TMPL
            clean_exit
        fi
        PROBE_ALSA=no
        log "AudioProbe : ALSA seems to be UNUSABLE on this system !!"
    fi
fi

CFLAGS=$ORG_CFLAGS
LDFLAGS=$ORG_LDFLAGS

# create the objs directory that is going to contain all generated files
# including the configuration ones
#
mkdir -p objs

###
###  Compiler probe
###

####
####  Host system probe
####

# because the previous version could be read-only
rm -f $TMPC

# check host endianess
#
HOST_BIGENDIAN=no
cat > $TMPC << EOF
#include <inttypes.h>
int main(int argc, char ** argv){
        volatile uint32_t i=0x01234567;
        return (*((uint8_t*)(&i))) == 0x67;
}
EOF
feature_run_exec HOST_BIGENDIAN

# check size of host long bits
cat > $TMPC << EOF
int main(void) {
        return sizeof(void*)*8;
}
EOF
feature_run_exec HOST_LONGBITS

# check whether we have <byteswap.h>
#
feature_check_header HAVE_BYTESWAP_H "<byteswap.h>"

# Build the config.make file
#

create_config_mk

PWD=`pwd`
echo "TARGET_ARCH := arm" >> $config_mk
echo "SRC_PATH          := $PWD" >> $config_mk
echo "SDL_CONFIG         := $SDL_CONFIG" >> $config_mk
echo "CONFIG_COREAUDIO  := $PROBE_COREAUDIO" >> $config_mk
echo "CONFIG_WINAUDIO   := $PROBE_WINAUDIO" >> $config_mk
echo "CONFIG_ESD        := $PROBE_ESD" >> $config_mk
echo "CONFIG_ALSA       := $PROBE_ALSA" >> $config_mk
echo "CONFIG_OSS        := $PROBE_OSS" >> $config_mk
echo "BUILD_STANDALONE_EMULATOR := true" >> $config_mk
if [ $OPTION_DEBUG = yes ] ; then
    echo "BUILD_DEBUG_EMULATOR := true" >> $config_mk
fi

# Build the config-host.h file
#
config_h=objs/config-host.h
echo "/* This file was autogenerated by '$PROGNAME' */" > $config_h
echo "#define CONFIG_QEMU_SHAREDIR   \"/usr/local/share/qemu\"" >> $config_h
echo "#define HOST_LONG_BITS  $HOST_LONGBITS" >> $config_h
if [ "$HAVE_BYTESWAP_H" = "yes" ] ; then
  echo "#define HAVE_BYTESWAP_H 1" >> $config_h
fi
echo "#define CONFIG_GDBSTUB  1" >> $config_h
echo "#define CONFIG_SLIRP    1" >> $config_h
echo "#define CONFIG_SKINS    1" >> $config_h
echo "#define CONFIG_TRACE    1" >> $config_h
# the -nand-limits options can only work on non-windows systems
if [ "$OS" != "windows" ] ; then
    echo "#define CONFIG_NAND_LIMITS  1" >> $config_h
fi
echo "#define QEMU_VERSION    \"0.10.50\"" >> $config_h
echo "#define QEMU_PKGVERSION \"Android\"" >> $config_h
case "$CPU" in
    x86) CONFIG_CPU=I386
    ;;
    ppc) CONFIG_CPU=PPC
    ;;
    x86_64) CONFIG_CPU=X86_64
    ;;
    *) CONFIG_CPU=$CPU
    ;;
esac
echo "#define HOST_$CONFIG_CPU    1" >> $config_h
BSD=0
case "$OS" in
    linux-*) CONFIG_OS=LINUX
    ;;
    darwin-*) CONFIG_OS=DARWIN
              BSD=1
    ;;
    windows*) CONFIG_OS=WIN32
    ;;
    *) CONFIG_OS=$OS
esac

case $OS in
    linux-*|darwin-*)
        echo "#define HAVE_IOVEC 1" >> $config_h
        ;;
esac

echo "#define CONFIG_$CONFIG_OS   1" >> $config_h
if [ $BSD = 1 ] ; then
    echo "#define _BSD             1" >> $config_h
    echo "#define O_LARGEFILE      0" >> $config_h
    echo "#define MAP_ANONYMOUS    MAP_ANON" >> $config_h
fi

log "Generate   : $config_h"

echo "Ready to go. Type 'make' to build emulator"
