#!/bin/bash
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
PROGNAME=`basename $0`

# this function will be used to execute commands and eventually
# dump them if VERBOSE is 'yes'
VERBOSE=yes
VERBOSE2=no

function log()
{
    if [ "$VERBOSE" = "yes" ] ; then
        echo "$1"
    fi
}

function log2()
{
    if [ "$VERBOSE2" = "yes" ] ; then
        echo "$1"
    fi
}

function execute()
{
    log2 "Running: $*"
    $*
}

function compile()
{
    log2 "Object     : $CC -o $TMPO -c $CFLAGS $TMPC"
    $CC -o $TMPO -c $CFLAGS $TMPC 2> $TMPL
}

function link()
{
    log2 "Link      : $LD $LDFLAGS -o $TMPE $TMPO"
    $LD $LDFLAGS -o $TMPE $TMPO 2> $TMPL
}

function compile-exec-run()
{
    log2 "RunExec    : $CC -o $TMPE $CFLAGS $TMPC"
    compile
    if [ $? != 0 ] ; then
        echo "Failure to compile test program"
        cat $TMPL
        exit 1
    fi
    link
    if [ $? != 0 ] ; then
        echo "Failure to link test program"
        cat $TMPL
        exit 1
    fi
    $TMPE
}

OS=`uname -s`
CPU=`uname -m`
EXE=""
case "$CPU" in
    i?86) CPU=x86
    ;;
esac

case "$OS" in
    Darwin)
        OS=darwin-$CPU
        ;;
    Linux)
        # note that building on x86_64 is handled later
        OS=linux-$CPU
        ;;
    *_NT-*)
        OS=windows
        EXE=.exe
        ;;
esac

# Are we running in the Android build system ?
unset TOP
if [ -n "$ANDROID_PRODUCT_OUT" ] ; then
    TOP=`cd $ANDROID_PRODUCT_OUT/../../../.. && pwd`
    log "TOP found at $TOP"
    # $TOP/config/envsetup.make is for the old tree layout
    # $TOP/build/envsetup.sh is for the new one
    ANDROID_CONFIG_MK=$TOP/config/envsetup.make
    if [ ! -f $ANDROID_CONFIG_MK ] ; then
        ANDROID_CONFIG_MK=$TOP/build/core/config.mk
    fi
    if [ ! -f $ANDROID_CONFIG_MK ] ; then
        echo "Cannot find build system root (TOP)"
        echo "defaulting to non-Android build"
        unset TOP
    fi
fi

# normalize the TOP variable, we don't want any trailing /
IN_ANDROID_BUILD=no
if [ -n "$TOP" ] ; then
    TOPDIR=`dirname $TOP`
    if [ "$TOPDIR" != "." ] ; then
        TOP=$TOPDIR/`basename $TOP`
    fi
    IN_ANDROID_BUILD=yes
    log "In Android Build"
fi

# Parse options
OPTION_TARGETS=""
OPTION_DEBUG=no
OPTION_IGNORE_AUDIO=no
OPTION_NO_PREBUILTS=no
OPTION_TRY_64=no
OPTION_HELP=no

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
  --install=*) OPTION_TARGETS="$TARGETS $optarg";
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
    echo ""
    exit 1
fi

# Various probes are going to need to run a small C program
TMPC=/tmp/android-qemu-$$.c
TMPO=/tmp/android-qemu-$$.o
TMPE=/tmp/android-qemu-$$$EXE
TMPL=/tmp/android-qemu-$$.log

function clean-exit ()
{
    rm -f $TMPC $TMPO $TMPL $TMPE
    exit 1
}

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

    # Get the value of a build variable as an absolute path.
    function get_abs_build_var()
    {
       (cd $TOP && CALLED_FROM_SETUP=true BUILD_SYSTEM=build/core make -f $ANDROID_CONFIG_MK dumpvar-abs-$1)
    }

    # locate prebuilt directory
    PREBUILT_HOST_TAG=$OS
    case $OS in
        linux-*)
            # Linux is a special case because in the old tree layout
            # we simply used 'Linux' as the prebuilt host tag, but
            # are now using "linux-x86" in the new layout
            # check which one should be used
            #
            if [ -d $TOP/prebuilt/Linux ] ; then
                PREBUILT_HOST_TAG=Linux
            fi
            ;;
    esac
    PREBUILT=$TOP/prebuilt/$PREBUILT_HOST_TAG
    if [ ! -d $PREBUILT ] ; then
        # this can happen when building on x86_64
        case $OS in
            linux-x86_64)
                PREBUILT_HOST_TAG=linux-x86
                PREBUILT=$TOP/prebuilt/$PREBUILT_HOST_TAG
                log "Forcing usage of 32-bit prebuilts"
                ;;
            *)
        esac
        if [ ! -d $PREBUILT ] ; then
            echo "Can't find the prebuilt directory $PREBUILT in Android build"
            exit 1
        fi
    fi
    log "Prebuilt   : PREBUILT=$PREBUILT"

    # use ccache if USE_CCACHE is defined and the corresponding
    # binary is available.
    #
    # note: located in PREBUILT/ccache/ccache in the new tree layout
    #       located in PREBUILT/ccache in the old one
    #
    if [ -n "$USE_CCACHE" ] ; then
        CCACHE="$PREBUILT/ccache/ccache$EXE"
        if [ ! -f $CCACHE ] ; then
            CCACHE="$PREBUILT/ccache$EXE"
        fi
        if [ -f $CCACHE ] ; then
            CC="$CCACHE $CC"
        fi
        log "Prebuilt   : CCACHE=$CCACHE"
    fi

    # if the user didn't specify a sdl-config script, get the prebuilt one
    if [ -z "$SDL_CONFIG" -a "$OPTION_NO_PREBUILTS" = "no" ] ; then
        # always use our own static libSDL by default
        SDL_CONFIG=$PREBUILT/sdl/bin/sdl-config
        log "Prebuilt   : SDL_CONFIG=$SDL_CONFIG"
    fi

    # finally ensure that our new binary is copied to the 'out'
    # subdirectory as 'emulator'
    HOST_BIN=$(get_abs_build_var HOST_OUT_EXECUTABLES)
    if [ -n "$HOST_BIN" ] ; then
        TARGETS="$TARGETS $HOST_BIN/emulator$EXE"
        log "Targets    : TARGETS=$TARGETS"
    fi
fi  # IN_ANDROID_BUILD = no


####
####  Compiler checks
####
####
if [ -z "$CC" ] ; then
    CC=gcc
    if [ $CPU = "powerpc" ] ; then
        CC=gcc-3.3
    fi
fi

cat > $TMPC <<EOF
int main(void) {}
EOF

if [ -z "$LD" ] ; then
    LD=$CC
fi

# we only support generating 32-bit binaris on 64-bit systems.
# And we may need to add a -Wa,--32 to CFLAGS to let the assembler
# generate 32-bit binaries on Linux x86_64.
#
if [ "$OPTION_TRY_64" != "yes" ] ; then
    if [ "$CPU" = "x86_64" -o "$CPU" = "amd64" ] ; then
        log "Check32Bits: Forcing generation of 32-bit binaries (--try-64 to disable)"
        CPU="i386"
        case $OS in
            linux-*)
                OS=linux-x86
                ;;
            darwin-*)
                OS=darwin-x86
                ;;
        esac
        CFLAGS="$CFLAGS -m32"
        LDFLAGS="$LDFLAGS -m32"
        compile
        if [ $? != 0 ] ; then
            CFLAGS="$CFLAGS -Wa,--32"
        fi
        # check that the compiler can link 32-bit executables
        # if not, try the host linker
        link
        if [ $? != 0 ] ; then
            OLD_LD=$LD
            LD=gcc
            compile
            link
            if [ $? != 0 ] ; then
                log "not using gcc for LD"
                LD=$OLD_LD
            fi
        fi
    fi
fi

compile
if [ $? != 0 ] ; then
    echo "C compiler doesn't seem to work:"
    cat $TMPL
    clean-exit
fi
log "CC         : compiler check ok ($CC)"

# on 64-bit systems, some of our prebuilt compilers are not
# capable of linking 32-bit executables properly
#
link
if [ $? != 0 ] ; then
    echo "Linker doesn't seem to work:"
    cat $TMPL
    clean-exit
fi
log "LD         : linker check ok ($LD)"

# We may need to add -fno-stack-protector but this is not
# supported by older versions of GCC
#
cat > $TMPC <<EOF
int main(void) {}
EOF
OLD_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fno-stack-protector"
compile
if [ $? != 0 ] ; then
    log "CFLAGS     : C compiler doesn't support -fno-stack-protector"
    CFLAGS="$OLD_CFLAGS"
else
    log "CFLAGS     : Adding -fno-stack-protector to CFLAGS"
fi

###
###  SDL Probe
###

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
        clean-exit
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

OLD_CFLAGS=$CFLAGS
OLD_LDFLAGS=$LDFLAGS

CFLAGS="$CFLAGS $SDL_CFLAGS"
LDFLAGS="$LDFLAGS $SDL_LIBS"

cat > $TMPC << EOF
#include <SDL.h>
#undef main
int main( void ) {
   return SDL_Init (SDL_INIT_VIDEO); 
}
EOF
compile
if [ $? != 0 ] ; then
    echo "You provided an explicit sdl-config script, but the corresponding library"
    echo "cannot be statically linked with the Android emulator directly."
    echo "Error message:"
    cat $TMPL
    clean-exit
fi
log "SDL-probe  : static linking ok"

# now, let's check that the SDL library has the special functions
# we added to our own sources
#
cat > $TMPC << EOF
#include <SDL.h>
#undef main
int main( void ) {
    int  x, y;
    SDL_WM_GetPos(&x, &y);
    SDL_WM_SetPos(x, y);
    return SDL_Init (SDL_INIT_VIDEO); 
}
EOF
compile
if [ $? != 0 ] ; then
    echo "You provided an explicit sdl-config script in SDL_CONFIG, but the"
    echo "corresponding library doesn't have the patches required to link"
    echo "with the Android emulator. Unsetting SDL_CONFIG will use the"
    echo "sources bundled with the emulator instead"
    echo "Error:"
    cat $TMPL
    clean-exit
fi

log "SDL-probe  : extra features ok"
rm -f $TMPL $TMPC $TMPE

CFLAGS=$OLD_CFLAGS
LDFLAGS=$OLD_LDFLAGS


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
            clean-exit
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
            clean-exit
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
compile-exec-run && HOST_BIGENDIAN=yes
log "Host       : HOST_BIGENDIAN=$HOST_BIGENDIAN"

# check size of host long bits
HOST_LONGBITS=32
cat > $TMPC << EOF
int main(void) {
        return sizeof(void*)*8;
}
EOF
compile-exec-run
HOST_LONGBITS=$?
log "Host       : HOST_LONGBITS=$HOST_LONGBITS"

# check whether we have <byteswap.h>
#
HAVE_BYTESWAP_H=yes
cat > $TMPC << EOF
#include <byteswap.h>
EOF
compile
if [ $? != 0 ] ; then
    HAVE_BYTESWAP_H=no
fi
log "Host       : HAVE_BYTESWAP_H=$HAVE_BYTESWAP_H"

# Build the config.make file
#
rm -rf objs
mkdir -p objs
config_mk=objs/config.make
echo "# This file was autogenerated by $PROGNAME" > $config_mk
echo "TARGET_ARCH := arm" >> $config_mk
case $OS in
    linux-*) HOST_OS=linux
    ;;
    darwin-*) HOST_OS=darwin
    ;;
    *) HOST_OS=$OS
esac
echo "OS          := $OS" >> $config_mk
echo "HOST_OS     := $HOST_OS" >> $config_mk
case $CPU in
	i?86) HOST_ARCH=x86
	;;
	amd64) HOST_ARCH=x86_64
	;;
	powerpc) HOST_ARCH=ppc
	;;
	*) HOST_ARCH=$CPU
esac
echo "HOST_ARCH         := $HOST_ARCH" >> $config_mk
PWD=`pwd`
echo "SRC_PATH          := $PWD" >> $config_mk
echo "CC                := $CC" >> $config_mk
echo "HOST_CC           := $CC" >> $config_mk
echo "LD                := $LD" >> $config_mk
echo "NO_PREBUILT       := $OPTION_NO_PREBUILTS" >> $config_mk
echo "HOST_PREBUILT_TAG := $PREBUILT_HOST_TAG" >> $config_mk
echo "PREBUILT          := $PREBUILT" >> $config_mk
echo "CFLAGS            := $CFLAGS" >> $config_mk
echo "LDFLAGS           := $LDFLAGS" >> $config_mk
echo "SDL_CONFIG         := $SDL_CONFIG" >> $config_mk
echo "CONFIG_COREAUDIO  := $PROBE_COREAUDIO" >> $config_mk
echo "CONFIG_WINAUDIO   := $PROBE_WINAUDIO" >> $config_mk
echo "CONFIG_ESD        := $PROBE_ESD" >> $config_mk
echo "CONFIG_ALSA       := $PROBE_ALSA" >> $config_mk
echo "CONFIG_OSS        := $PROBE_OSS" >> $config_mk
echo "" >> $config_mk
echo "BUILD_STANDALONE_EMULATOR := true" >> $config_mk
echo "HOST_PREBUILT_TAG         := $PREBUILT_HOST_TAG" >> $config_mk

log "Generate   : $config_mk"

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
# the -nand-limits options can only work on non-windows systems
if [ "$OS" != "windows" ] ; then
    echo "#define CONFIG_NAND_LIMITS  1" >> $config_h
fi
echo "#define QEMU_VERSION    \"0.8.2\"" >> $config_h
case "$CPU" in
    i386) HOST_CPU=I386
    ;;
    powerpc) HOST_CPU=PPC
    ;;
    x86_64|amd64) HOST_CPU=X86_64
    ;;
    *) HOST_CPU=$CPU
    ;;
esac
echo "#define HOST_$HOST_CPU    1" >> $config_h
log "Generate   : $config_h"

echo "Ready to go. Type 'make' to build emulator"
