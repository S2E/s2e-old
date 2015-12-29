#!/bin/sh
#
# fuzzys2e: add fuzz compability to s2e
# --------------------------------------
#
# Written by Epeius<binzhang08d@gmail.com>
#
# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#
#

echo "================================================="
echo "      FuzzyS2E build script         "
echo "================================================="
echo

S2E_Dir=$(pwd)
AFL_Dir=$S2E_Dir/afl

cd $AFL_Dir
./buildafl.sh

cd $S2E_Dir
cd ../

if [ ! -d "s2ebuild" ]; then
  mkdir s2ebuild
fi

cd s2ebuild
echo "[*] building S2E..."
make -f $S2E_Dir/Makefile
