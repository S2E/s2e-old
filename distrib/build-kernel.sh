#!/bin/sh
#
# A small script used to rebuild the Android goldfish kernel image
# See docs/KERNEL.TXT for usage instructions.
#
MACHINE=goldfish
VARIANT=goldfish
OUTPUT=/tmp/kernel-qemu
CROSSPREFIX=arm-eabi-
CONFIG=goldfish

# Extract number of processors
JOBS=`grep -c "processor" /proc/cpuinfo`
JOBS=$(( $JOBS*2 ))

ARCH=arm

OPTION_HELP=no
OPTION_ARMV7=no
OPTION_OUT=
OPTION_CROSS=
OPTION_ARCH=
OPTION_CONFIG=

for opt do
    optarg=$(expr "x$opt" : 'x[^=]*=\(.*\)')
    case $opt in
    --help|-h|-\?) OPTION_HELP=yes
        ;;
    --armv7)
        OPTION_ARMV7=yes
        ;;
    --out=*)
        OPTION_OUT=$optarg
        ;;
    --cross=*)
        OPTION_CROSS=$optarg
        ;;
    --arch=*)
        OPTION_ARCH=$optarg
        ;;
    --config=*)
        OPTION_CONFIG=$optarg
        ;;
    -j*)
        JOBS=$optarg
        ;;
    *)
        echo "unknown option '$opt', use --help"
        exit 1
    esac
done

if [ $OPTION_HELP = "yes" ] ; then
    echo "Rebuild the prebuilt kernel binary for Android's emulator."
    echo ""
    echo "options (defaults are within brackets):"
    echo ""
    echo "  --help                   print this message"
    echo "  --arch=<arch>            change target architecture [$ARCH]"
    echo "  --armv7                  build ARMv7 binaries (see note below)"
    echo "  --out=<directory>        output directory [$OUTPUT]"
    echo "  --cross=<prefix>         cross-toolchain prefix [$CROSSPREFIX]"
    echo "  --config=<name>          kernel config name [$CONFIG]"
    echo ""
    echo "NOTE: --armv7 is equivalent to --config=goldfish_armv7. It is"
    echo "      ignored if --config=<name> is used."
    echo ""
    exit 0
fi

if [ -n "$OPTION_ARCH" ]; then
    ARCH="$OPTION_ARCH"
fi

if [ -n "$OPTION_CONFIG" ]; then
    CONFIG="$OPTION_CONFIG"
elif [ "$OPTION_ARMV7" = "yes" ]; then
    CONFIG=goldfish_armv7
fi

# Check that we are in the kernel directory
if [ ! -d arch/$ARCH/mach-$MACHINE ] ; then
    echo "Cannot find arch/$ARCH/mach-$MACHINE. Please cd to the kernel source directory."
    echo "Aborting."
    #exit 1
fi

# Check output directory.
if [ -n "$OPTION_OUT" ] ; then
    if [ ! -d "$OPTION_OUT" ] ; then
        echo "Output directory '$OPTION_OUT' does not exist ! Aborting."
        exit 1
    fi
    OUTPUT=$OPTION_OUT
else
    mkdir -p $OUTPUT
fi

if [ -n "$OPTION_CROSS" ] ; then
    CROSSPREFIX="$OPTION_CROSS"
else
    case $ARCH in
        arm)
            CROSSPREFIX=arm-eabi-
            ZIMAGE=zImage
            ;;
        x86)
            CROSSPREFIX=i686-android-linux-
            ZIMAGE=bzImage
            ;;
        *)
            echo "ERROR: Unsupported architecture!"
            exit 1
            ;;
    esac
fi

export CROSS_COMPILE="$CROSSPREFIX" ARCH SUBARCH=$ARCH

# Check that the cross-compiler is in our path
#
CROSS_COMPILER="${CROSS_COMPILE}gcc"
CROSS_COMPILER_VERSION=$($CROSS_COMPILER --version 2>/dev/null)
if [ $? != 0 ] ; then
    echo "It looks like $CROSS_COMPILER is not in your path ! Aborting."
    exit 1
fi

rm -f include/asm &&
make ${CONFIG}_defconfig &&    # configure the kernel
make -j$JOBS                   # build it

if [ $? != 0 ] ; then
    echo "Could not build the kernel. Aborting !"
    exit 1
fi

OUTPUT_KERNEL=kernel-qemu
OUTPUT_VMLINUX=vmlinux-qemu
if [ "$OPTION_ARMV7" = "yes" ] ; then
    OUTPUT_KERNEL=${OUTPUT_KERNEL}-armv7
    OUTPUT_VMLINUX=${OUTPUT_VMLINUX}-armv7
fi

cp -f arch/$ARCH/boot/$ZIMAGE $OUTPUT/$OUTPUT_KERNEL
cp -f vmlinux $OUTPUT/$OUTPUT_VMLINUX

echo "Kernel $CONFIG prebuilt image copied to $OUTPUT successfully !"
exit 0
