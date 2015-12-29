#!/bin/sh
#
# american fuzzy lop - QEMU build script
# --------------------------------------
#
# Written by Andrew Griffiths <agriffiths@google.com> and
#            Michal Zalewski <lcamtuf@google.com>
#
# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# This script downloads, patches, and builds a version of QEMU with
# minor tweaks to allow non-instrumented binaries to be run under
# afl-fuzz. 
#
# The modifications reside in patches/*. The standalone QEMU binary
# will be written to ../afl-qemu-trace.
#

AFL_URL="https://github.com/Epeius/afl/archive/master.zip"

echo "================================================="
echo "      AFL integrate withS2E build script         "
echo "================================================="
echo


ARCHIVE="`basename -- "AFL_URL"`"


echo "[*] Downloading afl from the web..."
rm -f "$ARCHIVE"
wget -O "$ARCHIVE" -- "$AFL_URL" || exit 1

echo "[*] Uncompressing archive (this will take a while)..."

rm -rf "afl-1.96b" || exit 1
unzip "$ARCHIVE" || exit 1
mv "afl-master" "afl-1.96b"

echo "[+] Unpacking successful."
echo "[*] Attempting to build afl (fingers crossed!)..."
cd afl-1.96b

if [ ! -f "/tmp/aflbitmap" ]; then

  touch "tmp/aflbitmap"

fi

make || exit 1
echo "[+] Build process successful!"
echo "[*] Attempting to build AFL-QEMU (fingers crossed!)..."

cd qemu_mode

./build_qemu_support.sh || exit 1
echo "[+] Build process successful!"

