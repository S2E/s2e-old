#!/bin/bash
#
#  this script is used to build a static version of the Android emulator
#  from our distribution package.
#
cd $(dirname $0)
CURDIR=$(pwd)

show_help=
TARGET=emulator
for opt; do
  optarg=`expr "x$opt" : 'x[^=]*=\(.*\)'`
  case "$opt" in
  --help|-h|-\?) show_help=yes
  ;;
  --target=*) TARGET=$optarg
  ;;
  esac
done

if [ -n "$show_help" ] ; then
    echo "usage: build-emulator [--target=FILEPATH]"
    exit 1
fi

# directory where we'll place the temporary SDL binaries
LOCAL=$CURDIR/local

cd $CURDIR/sdl
if ! (ANDROID_QEMU_PATH=$CURDIR/qemu ./android-configure.sh --prefix=$LOCAL && make && make install); then
    echo "ERROR: could not build SDL library, please check their sources"
fi

cd $CURDIR/qemu
if ! (./android-rebuild.sh --sdl-config=$LOCAL/bin/sdl-config); then
    echo "ERROR: could not build the emulator, please check the sources"
fi

cp objs/emulator $CURDIR/emulator
