#!/bin/sh
#
# this script is used to rebuild the Android emulator from sources
# in the current directory. It also contains logic to speed up the
# rebuild if it detects that you're using the Android build system
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
OPTION_STATIC=no
OPTION_MINGW=no
OPTION_S2E=no
OPTION_LLVM=no
HOST_CC=${CC:-gcc}
OPTION_CC=

TARGET_ARCH=arm

#default parameters for s2e
cxx="g++"
asm="nasm"

llvm="no"
llvmdir=""
s2e="no"
kleedir=""
stpdir=""
llvmgcc=""
zeromalloc="no"

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
  --mingw) OPTION_MINGW=yes
  ;;
  --debug) OPTION_DEBUG=yes
  ;;
  --ignore-audio) OPTION_IGNORE_AUDIO=yes
  ;;
  --no-prebuilts) OPTION_NO_PREBUILTS=yes
  ;;
  --try-64) OPTION_TRY_64=yes
  ;;
  --static) OPTION_STATIC=yes
  ;;
  --cxx=*) cxx=$optarg
  ;;
  --arch=*) TARGET_ARCH=$optarg
  ;;
  --enable-llvm) llvm=yes
  ;;
  --with-llvm=*) llvmdir=$optarg
  ;;
  --enable-s2e) s2e=yes
  ;;
  --with-klee=*) kleedir=$optarg
  ;;
  --with-stp=*) stpdir=$optarg
  ;;
  --with-llvmgcc=*) llvmgcc=$optarg
  ;;
  --zero-malloc) zeromalloc="yes"
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
    echo "  --cc=PATH                specify C compiler [$HOST_CC]"
    echo "  --cxx=PATH               specify C++ compiler [$cxx]"
    echo "  --arch=ARM               specify target architecture [$TARGET_ARCH]"
    echo "  --sdl-config=FILE        use specific sdl-config script [$SDL_CONFIG]"
    echo "  --no-strip               do not strip emulator executable"
    echo "  --debug                  enable debug (-O0 -g) build"
    echo "  --ignore-audio           ignore audio messages (may build sound-less emulator)"
    echo "  --no-prebuilts           do not use prebuilt libraries and compiler"
    echo "  --try-64                 try to build a 64-bit executable (may crash)"
    echo "  --mingw                  build Windows executable on Linux"
    echo "  --static                 build a completely static executable"
    echo "  --verbose                verbose configuration"
    echo "  --debug                  build debug version of the emulator"
    echo "  --enable-llvm            enable LLVM support (for all targets)"
    echo "  --with-llvm=PATH         LLVM path (PATH/bin/llvm-config must exist)"
    echo "  --enable-s2e             enable S2E"
    echo "  --with-klee=PATH         KLEE path (PATH/bin/klee-config must exist)"
    echo "  --with-stp=PATH         STP path (PATH/lib/libstp.a must exist)"
    echo "  --with-llvmgcc=PATH      path to llvm-gcc"
    echo "  --zero-malloc			 allow zero memory allocations"
    echo ""
    exit 1
fi

# On Linux, try to use our 32-bit prebuilt toolchain to generate binaries
# that are compatible with Ubuntu 8.04
if [ -z "$CC" -a -z "$OPTION_CC" -a "$HOST_OS" = linux -a "$OPTION_TRY_64" != "yes" ] ; then
    HOST_CC=`dirname $0`/../../prebuilt/linux-x86/toolchain/i686-linux-glibc2.7-4.4.3/bin/i686-linux-gcc
    if [ -f "$HOST_CC" ] ; then
        echo "Using prebuilt 32-bit toolchain: $HOST_CC"
        CC="$HOST_CC"
    fi
fi

echo "OPTION_CC='$OPTION_CC'"
if [ -n "$OPTION_CC" ]; then
    echo "Using specified C compiler: $OPTION_CC"
    CC="$OPTION_CC"
fi

# we only support generating 32-bit binaris on 64-bit systems.
# And we may need to add a -Wa,--32 to CFLAGS to let the assembler
# generate 32-bit binaries on Linux x86_64.
#
if [ "$OPTION_TRY_64" != "yes" ] ; then
    force_32bit_binaries
fi

TARGET_OS=$OS
if [ "$OPTION_MINGW" == "yes" ] ; then
    enable_linux_mingw
    TARGET_OS=windows
else
    enable_cygwin
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

    # finally ensure that our new binary is copied to the 'out'
    # subdirectory as 'emulator'
    HOST_BIN=$(get_android_abs_build_var HOST_OUT_EXECUTABLES)
    if [ -n "$HOST_BIN" ] ; then
        OPTION_TARGETS="$OPTION_TARGETS $HOST_BIN/emulator$EXE"
        log "Targets    : TARGETS=$OPTION_TARGETS"
    fi

    # find the Android SDK Tools revision number
    TOOLS_PROPS=$ANDROID_TOP/sdk/files/tools_source.properties
    if [ -f $TOOLS_PROPS ] ; then
        ANDROID_SDK_TOOLS_REVISION=`awk -F= '/Pkg.Revision/ { print $2; }' $TOOLS_PROPS 2> /dev/null`
        log "Tools      : Found tools revision number $ANDROID_SDK_TOOLS_REVISION"
    else
        log "Tools      : Could not locate $TOOLS_PROPS !?"
    fi
fi  # IN_ANDROID_BUILD = no


# we can build the emulator with Cygwin, so enable it
enable_cygwin

setup_toolchain

###
###  SDL Probe
###

if [ -n "$SDL_CONFIG" ] ; then

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

	case "$OS" in
		freebsd-*)
		EXTRA_LDFLAGS="$EXTRA_LDFLAGS -lm -lpthread"
		;;
	esac

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
fi

###
###  Audio subsystems probes
###
PROBE_COREAUDIO=no
PROBE_ALSA=no
PROBE_OSS=no
PROBE_ESD=no
PROBE_PULSEAUDIO=no
PROBE_WINAUDIO=no

case "$TARGET_OS" in
    darwin*) PROBE_COREAUDIO=yes;
    ;;
    linux-*) PROBE_ALSA=yes; PROBE_OSS=yes; PROBE_ESD=yes; PROBE_PULSEAUDIO=yes;
    ;;
    freebsd-*) PROBE_OSS=yes;
    ;;
    windows) PROBE_WINAUDIO=yes
    ;;
esac

ORG_CFLAGS=$CFLAGS
ORG_LDFLAGS=$LDFLAGS

if [ "$OPTION_IGNORE_AUDIO" = "yes" ] ; then
PROBE_ESD_ESD=no
PROBE_ALSA=no
PROBE_PULSEAUDIO=no
fi

##########################################
# asm support probe

cat > $TMPASM <<EOF
mov eax, 1
EOF

if assemble_object_asm ; then
  :  assembler works ok
else
  echo "ERROR: \"$asm\" either does not exist or does not work"
  exit 1
fi

##########################################
# C++ support probe

if test "$llvm" != "no" -o "$s2e" != "no" ; then

# check that the CXX compiler works.
cat > $TMPCXX <<EOF
int main(void) {}
EOF

if compile_object_cxx ; then
  : CXX compiler works ok
else
  echo "ERROR: \"$cxx\" (required for --enable-llvm and --enable-s2e) either does not exist or does not work"
  exit 1
fi

fi

##########################################
# llvm support probe

if test "$llvm" != "no" -o "$s2e" != "no" ; then
  cat > $TMPCXX << EOF
#include <llvm/LLVMContext.h>
int main(void) { llvm::LLVMContext& c = llvm::getGlobalContext(); return 0; }
EOF
  if test "$llvmdir" != ""; then
      llvm_config="$llvmdir/bin/llvm-config"
  else
      llvm_config="llvm-config"
      echo "use default llvmdir: $llvm_config"
  fi
  echo "llvm_config: $llvm_config"
  llvm_components="jit bitreader bitwriter ipo linker engine"
  llvm_cxxflags=`$llvm_config $llvm_components --cflags 2> /dev/null`
  echo "llvm_components: $llvm_components"

  llvm_ldflags=`$llvm_config $llvm_components --ldflags 2> /dev/null`
  llvm_libs=`$llvm_config $llvm_components --libs 2> /dev/null`

  echo "llvm_cxxflags: $llvm_cxxflags --- llvm_libs: $llvm_libs --- llvm_ldflags: $llvm_ldflags"

  if compile_prog_cxx "$llvm_cxxflags" "$llvm_libs $llvm_ldflags" ; then
    : LLVM found
  else
    echo "AKA: feature not found"
    feature_not_found "llvm (required for --enable-llvm and --enable-s2e)"
    exit 1
  fi
fi

if test "$llvm" != "no" ; then
  llvm="yes"
  LIBS="$llvm_libs $LIBS $llvm_ldflags"
  linker="$cxx"
fi

##########################################
# s2e probe: KLEE

if test "$s2e" != "no" ; then
  cat > $TMPCXX << EOF
#include <llvm/LLVMContext.h>
#include <klee/Common.h>
int main(void) { llvm::LLVMContext& c = llvm::getGlobalContext(); klee::klee_message("a"); return 0; }
EOF
  if test "$kleedir" != ""; then
      klee_config="$kleedir/bin/klee-config"
  else
      echo "AKA: klee default dir"
      klee_config="klee-config"
  fi
  klee_cxxflags=`$klee_config --cflags 2> /dev/null`
  klee_ldflags=`$klee_config --ldflags 2> /dev/null`
  klee_libs=`$klee_config --libs 2> /dev/null`
  klee_libdir=`$klee_config --libdir 2> /dev/null | sed s:Debug/lib$:Release/lib:`
  klee_libfiles=`$klee_config --libfiles 2> /dev/null`

  if test "$stpdir" != ""; then
    stpdir=$(cd $stpdir>/dev/null && pwd)
    klee_ldflags="$klee_ldflags -L$stpdir/lib"
    klee_libs="$klee_libs -lstp"
  fi

  if compile_prog_cxx "$klee_cxxflags $llvm_cxxflags" "$klee_ldflags $klee_libs $llvm_libs $llvm_ldflags" ; then
    s2e="yes"
  else
    feature_not_found "klee (required for s2e)"
    exit 1
  fi
fi

##########################################
# s2e probe: lua

if test "$s2e" != "no" ; then
  cat > $TMPCXX << EOF
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
int main(void) { lua_State *L = lua_open(); lua_close(L); return 0; }
EOF
  if [ "$mingw32" = "yes" ]; then
    lua_libs="-llua"
  else
    if pkg-config --exists lua5.1 ; then
        lua_pkg="lua5.1"
    else
        lua_pkg="lua"
    fi
    lua_cxxflags=`pkg-config $lua_pkg --cflags 2> /dev/null`
    lua_libs=`pkg-config $lua_pkg --libs 2> /dev/null`
  fi

  if compile_prog_cxx "$lua_cxxflags" "$lua_libs" ; then
    : LUA found
  else
    feature_not_found "lua (required for s2e)"
    exit 1
  fi
fi

##########################################
# s2e probe: libsigc++-2

if test "$s2e" != "no" ; then
  cat > $TMPCXX << EOF
#include <sigc++/sigc++.h>
int main(void) { sigc::signal<void, int> s; s.emit(1); return 0; }
EOF
  if [ "$mingw32" = "yes" ]; then
    sigcxx2_libs="-lsigc-2.0.dll"
  else
    sigcxx2_cxxflags=`pkg-config sigc++-2.0 --cflags 2> /dev/null`
    sigcxx2_libs=`pkg-config sigc++-2.0 --libs 2> /dev/null`
  fi

  if compile_prog_cxx "$sigcxx2_cxxflags" "$sigcxx2_libs" ; then
    : libsigc++-2.0 found
  else
    feature_not_found "libsigc++ (required for s2e)"
    exit 1
  fi
fi

##########################################
# s2e probe: llvmgcc

if test "$s2e" != "no" ; then
  if test -z "$llvmgcc"; then
    llvmgcc=`which llvm-gcc`
  fi
  if [ -x "$llvmgcc" ]; then
    : llvm-gcc found
  else
    feature_not_found "llvmgcc (required for s2e)"
    exit 1
  fi

  if test "$llvmdir" != ""; then
      llvmar="$llvmdir/bin/llvm-ar"
  else
      llvmar="llvm-ar"
  fi
fi

# Probe a system library
#
# $1: Variable name (e.g. PROBE_ESD)
# $2: Library name (e.g. "Alsa")
# $3: Path to source file for probe program (e.g. android/config/check-alsa.c)
# $4: Package name (e.g. libasound-dev)
#
probe_system_library ()
{
    if [ `var_value $1` = yes ] ; then
        CFLAGS="$ORG_CFLAGS"
        LDFLAGS="$ORG_LDFLAGS -ldl"
        cp -f android/config/check-esd.c $TMPC
        compile
        if [ $? = 0 ] ; then
            log "AudioProbe : $2 seems to be usable on this system"
        else
            if [ "$OPTION_IGNORE_AUDIO" = no ] ; then
                echo "The $2 development files do not seem to be installed on this system"
                echo "Are you missing the $4 package ?"
                echo "Correct the errors below and try again:"
                cat $TMPL
                clean_exit
            fi
            eval $1=no
            log "AudioProbe : $2 seems to be UNUSABLE on this system !!"
        fi
    fi
}

probe_system_library PROBE_ESD        ESounD     android/config/check-esd.c libesd-dev
probe_system_library PROBE_ALSA       Alsa       android/config/check-alsa.c libasound-dev
probe_system_library PROBE_PULSEAUDIO PulseAudio android/config/check-pulseaudio.c libpulse-dev

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
if [ "$TARGET_OS" = "$OS" ] ; then
cat > $TMPC << EOF
#include <inttypes.h>
int main(int argc, char ** argv){
        volatile uint32_t i=0x01234567;
        return (*((uint8_t*)(&i))) == 0x01;
}
EOF
feature_run_exec HOST_BIGENDIAN
fi

# check size of host long bits
HOST_LONGBITS=32
if [ "$TARGET_OS" = "$OS" ] ; then
cat > $TMPC << EOF
int main(void) {
        return sizeof(void*)*8;
}
EOF
feature_run_exec HOST_LONGBITS
fi

# check whether we have <byteswap.h>
#
feature_check_header HAVE_BYTESWAP_H      "<byteswap.h>"
feature_check_header HAVE_MACHINE_BSWAP_H "<machine/bswap.h>"
feature_check_header HAVE_FNMATCH_H       "<fnmatch.h>"

#s2e assembler flags
if [ "TARGET_ARCH" = "x86" ]; then
  ASMFLAGS="-f elf32"
elif [ "TARGET_ARCH" = "x86_64" ] ; then
  ASMFLAGS="-f elf64"
fi

# Build the config.make file
#

case $TARGET_OS in
    windows)
        TARGET_EXEEXT=.exe
        ;;
    *)
        TARGET_EXEEXT=
        ;;
esac

create_config_mk
echo "" >> $config_mk
if [ $TARGET_ARCH = arm ] ; then
echo "TARGET_ARCH       := arm" >> $config_mk
fi

if [ $TARGET_ARCH = x86 ] ; then
echo "TARGET_ARCH       := x86" >> $config_mk
fi

echo "CXX=$cxx" >> $config_mk
echo "ASM=$asm" >> $config_mk
echo "LINKER=$cxx" >> $config_mk


echo "HOST_PREBUILT_TAG := $TARGET_OS" >> $config_mk
echo "HOST_EXEEXT       := $TARGET_EXEEXT" >> $config_mk
echo "PREBUILT          := $ANDROID_PREBUILT" >> $config_mk

PWD=`pwd`
echo "SRC_PATH          := $PWD" >> $config_mk
if [ -n "$SDL_CONFIG" ] ; then
echo "SDL_CONFIG         := $SDL_CONFIG" >> $config_mk
fi
echo "CONFIG_COREAUDIO  := $PROBE_COREAUDIO" >> $config_mk
echo "CONFIG_WINAUDIO   := $PROBE_WINAUDIO" >> $config_mk
echo "CONFIG_ESD        := $PROBE_ESD" >> $config_mk
echo "CONFIG_ALSA       := $PROBE_ALSA" >> $config_mk
echo "CONFIG_OSS        := $PROBE_OSS" >> $config_mk
echo "CONFIG_PULSEAUDIO := $PROBE_PULSEAUDIO" >> $config_mk
echo "BUILD_STANDALONE_EMULATOR := true" >> $config_mk
if [ $OPTION_DEBUG = yes ] ; then
    echo "BUILD_DEBUG_EMULATOR := true" >> $config_mk
  llvm_cxxflags=`echo ${llvm_cxxflags} | sed 's/\-O[23]//' | sed 's/\-DNDEBUG//'`
  klee_cxxflags=`echo ${klee_cxxflags} | sed 's/\-O[23]//' | sed 's/\-DNDEBUG//'`
else 
  CFLAGS="-O2 $CFLAGS"
  CXXFLAGS="-O2 $CXXFLAGS"
fi

if [ "$s2e" = yes ] ; then
  echo "CONFIG_S2E=y" >> $config_mk
  echo "CONFIG_LLVM=y" >> $config_mk
  echo "LLVM_CXXFLAGS:=$llvm_cxxflags" >> $config_mk
  echo "S2E_CXXFLAGS:=\$(filter-out $llvm_cxxflags, $klee_cxxflags)" >> $config_mk
  echo "S2E_CXXFLAGS+=$lua_cxxflags $sigcxx2_cxxflags" >> $config_mk
  echo "S2E_CXXFLAGS+=-DKLEE_LIBRARY_DIR='\"$klee_libdir\"'" >> $config_mk
  echo "LLVMCC:=$llvmgcc" >> $config_mk
  echo "LLVMAR:=$llvmar" >> $config_mk
  echo "LINKER=$cxx" >> $config_mk
  echo "LIBS+=$klee_libs $llvm_libs" >> $config_mk
  echo "LIBS+=$lua_libs $sigcxx2_libs" >> $config_mk
  echo "LIBS+=\$(filter-out $llvm_ldflags, $klee_ldflags) $llvm_ldflags" >> $config_mk
  echo "KLEE_LIBFILES=$klee_libfiles" >> $config_mk
 
fi


if [ $OPTION_STATIC = yes ] ; then
    echo "CONFIG_STATIC_EXECUTABLE := true" >> $config_mk
fi

if [ "$llvm" = "yes" ] ; then
  echo "CONFIG_LLVM=y" >> $config_mk
  echo "LLVM_CXXFLAGS=$llvm_cxxflags" >> $config_mk
fi

if [ -n "$ANDROID_SDK_TOOLS_REVISION" ] ; then
    echo "ANDROID_SDK_TOOLS_REVISION := $ANDROID_SDK_TOOLS_REVISION" >> $config_mk
fi

if [ "$OPTION_MINGW" = "yes" ] ; then
    echo "" >> $config_mk
    echo "USE_MINGW := 1" >> $config_mk
    echo "HOST_OS   := windows" >> $config_mk
fi


# Build the config-host.h file
#
config_h=objs/config-host.h
echo "/* This file was autogenerated by '$PROGNAME' */" > $config_h
echo "#define CONFIG_QEMU_SHAREDIR   \"/usr/local/share/qemu\"" >> $config_h
echo "#define HOST_LONG_BITS  $HOST_LONGBITS" >> $config_h
if [ "$HAVE_BYTESWAP_H" = "yes" ] ; then
  echo "#define CONFIG_BYTESWAP_H 1" >> $config_h
fi
if [ "$HAVE_MACHINE_BYTESWAP_H" = "yes" ] ; then
  echo "#define CONFIG_MACHINE_BSWAP_H 1" >> $config_h
fi
if [ "$HAVE_FNMATCH_H" = "yes" ] ; then
  echo "#define CONFIG_FNMATCH  1" >> $config_h
fi
echo "#define CONFIG_GDBSTUB  1" >> $config_h
echo "#define CONFIG_SLIRP    1" >> $config_h
echo "#define CONFIG_SKINS    1" >> $config_h
echo "#define CONFIG_TRACE    1" >> $config_h

if [ "$s2e" = yes ] ; then
  echo "#define CONFIG_S2E    1" >> $config_h
  echo "#define CONFIG_LLVM	  1" >> $config_h
fi

if test "$zeromalloc" = "yes" ; then
  echo "#define CONFIG_ZERO_MALLOC	1" >> $config_h
fi


# only Linux has fdatasync()
case "$TARGET_OS" in
    linux-*)
        echo "#define CONFIG_FDATASYNC    1" >> $config_h
        ;;
esac

# the -nand-limits options can only work on non-windows systems
if [ "$TARGET_OS" != "windows" ] ; then
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
if [ "$HOST_BIGENDIAN" = "1" ] ; then
  echo "#define HOST_WORDS_BIGENDIAN 1" >> $config_h
fi
BSD=0
case "$TARGET_OS" in
    linux-*) CONFIG_OS=LINUX
    ;;
    darwin-*) CONFIG_OS=DARWIN
              BSD=1
    ;;
    freebsd-*) CONFIG_OS=FREEBSD
              BSD=1
    ;;
    windows*) CONFIG_OS=WIN32
    ;;
    *) CONFIG_OS=$OS
esac

if [ "$OPTION_STATIC" = "yes" ] ; then
    echo "CONFIG_STATIC_EXECUTABLE := true" >> $config_mk
    echo "#define CONFIG_STATIC_EXECUTABLE  1" >> $config_h
fi

case $TARGET_OS in
    linux-*|darwin-*)
        echo "#define CONFIG_IOVEC 1" >> $config_h
        ;;
esac

echo "#define CONFIG_$CONFIG_OS   1" >> $config_h
if [ $BSD = 1 ] ; then
    echo "#define CONFIG_BSD       1" >> $config_h
    echo "#define O_LARGEFILE      0" >> $config_h
    echo "#define MAP_ANONYMOUS    MAP_ANON" >> $config_h
fi

echo "#define CONFIG_ANDROID       1" >> $config_h

log "Generate   : $config_h"

echo "Ready to go. Type 'make' to build emulator"
