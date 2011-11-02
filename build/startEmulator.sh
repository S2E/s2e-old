#!/bin/sh

$LEAKALIZER/build/android-release/objs-nos2e/emulator  -no-snapshot-save  -no-snapshot-load  -noskin -sysdir $LEAKALIZER/android-images/ledroid.avd/system -datadir $LEAKALIZER/android-images/ledroid.avd -kernel $LEAKALIZER/android-images/ledroid.avd/system/zImage -data $LEAKALIZER/android-images/ledroid.avd/userdata.img -snapstorage $LEAKALIZER/android-images/ledroid.avd/snapshots.img -sdcard $LEAKALIZER/android-images/ledroid.avd/sdcard.img -memory 256 -show-kernel  -verbose  -qemu -monitor stdio
