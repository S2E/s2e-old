#!/bin/bash
#
# this script is used to build a source distribution package for the Android emulator
# the package includes:
#  - the sources of our patched SDL library
#  - the sources of our patched QEMU emulator
#  - appropriate scripts to rebuild the emulator binary
#

# create temporary directory
TMPROOT=/tmp/android-package
DATE=$(date +%Y%m%d)
PACKAGE=android-emulator-$DATE
TMPDIR=$TMPROOT/$PACKAGE
if ! ( rm -rf $TMPROOT && mkdir -p $TMPDIR ) then
    echo "could not create temporary directory $TMPDIR"
    exit 3
fi

locate_qemu_viewpath ()
{
    viewpath=$(p4 files $0 | sed -e "s/\(.*\)#.*/\\1/g")
    # assumes that this program is in the 'distrib' directory of the QEMU sources
    echo $(dirname $(dirname $viewpath))
}

locate_depot_files ()
{
    root=$(p4 where $1) || (
        echo "you need to map $1 into your workspace to build an emulator source release package"
        exit 3
    )
    root=$(echo $root | cut -d" " -f3 | sed -e "s%/\.\.\.%%")
    echo $root
}

locate_source_files ()
{
    files=$(p4 files $1/... | grep -v "delete change" | sed -e "s/\(.*\)#.*/\\1/g")
    files=$(echo $files | sed -e "s%$1/%%g")
    echo $files
}

# locate SDL root directory in client workspace
if [ -z "$SDLROOT" ] ; then
    SDLROOT=$(locate_depot_files //toolchain/sdl/...)
    echo "SDLROOT is $SDLROOT"
fi

if [ ! -x "$SDLROOT" ] ; then
    if [ -z "$TOP" ] ; then
        echo "please define the TOP variable"
        exit 3
    fi
    echo "unable to find $SDLROOT as the SDL root directory"
    echo "please define SDLROOT to point to the correct location"
    exit 3
fi

# locate QEMU root directory
if  [ -z "$QEMUROOT" ] ; then
    QEMUVIEW=$(locate_qemu_viewpath)
    echo "QEMUVIEW is $QEMUVIEW"
    QEMUROOT=$(locate_depot_files $QEMUVIEW/...)
    echo "QEMUROOT is $QEMUROOT"
fi

if [ ! -x "$QEMUROOT" ] ; then
    if [ -z "$TOP" ] ; then
        echo "please define the TOP variable"
        exit 3
    fi
    echo "unable to find $QEMUROOT as the QEMU root directory"
    echo "please define QEMUROOT to point to the correct location"
    exit 3
fi

copy_source_files ()
{
  DSTDIR=$1
  SRCDIR=$2
  files=$(locate_source_files $3)
  mkdir $DSTDIR && for f in $files; do
    mkdir -p $(dirname $DSTDIR/$f);
    cp $SRCDIR/$f $DSTDIR/$f
  done
}

# copy and cleanup the SDL sources
echo "copying SDL sources"
SDLDIR=$TMPDIR/sdl
copy_source_files $SDLDIR $SDLROOT //toolchain/sdl

# copy and cleanup the QEMU sources
echo "copying QEMU sources"
QEMUDIR=$TMPDIR/qemu
copy_source_files $QEMUDIR $QEMUROOT $QEMUVIEW

echo "copying control scripts"
cp $QEMUDIR/distrib/build-emulator.sh $TMPDIR/build-emulator.sh
cp $QEMUDIR/distrib/README $TMPDIR/README

echo "packaging release into a tarball"
cd $TMPROOT
tar cjf $PACKAGE.tar.bz2 $PACKAGE

echo "cleaning up"
rm -rf $TMPDIR

echo "please grab $TMPROOT/$PACKAGE.tar.bz2"
