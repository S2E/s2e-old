#!/bin/sh

################################################
# Start Leakalizer with snapshot 'start1'      #
# stored in 'snapshots.img'                    #
################################################

$LEAKALIZER/build/android-release/objs/emulator -snapshot start1 -no-snapshot-save -noskin -sysdir $LEAKALIZER/android-images/ledroid.avd/system -datadir $LEAKALIZER/android-images/ledroid.avd -kernel $LEAKALIZER/android-images/ledroid.avd/system/zImage -data $LEAKALIZER/android-images/ledroid.avd/userdata.img -snapstorage $LEAKALIZER/android-images/ledroid.avd/snapshots.img -sdcard $LEAKALIZER/android-images/ledroid.avd/sdcard.img -memory 256 -show-kernel -verbose -qemu -monitor stdio -L $LEAKALIZER/android-emulator/pc-bios -s2e-config-file $LEAKALIZER/build/config.lua -s2e-verbose 
