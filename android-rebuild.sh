#!/bin/bash
#
# this script is used to rebuild all QEMU binaries for the host
# platforms.
#
# assume that the device tree is in TOP
#

cd `dirname $0`
./android-configure.sh $* && \
make clean && \
make -j4 && \
echo "Done. !!"
