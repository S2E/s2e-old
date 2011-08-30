#!/bin/sh
# Manual build of S2E-instruction-invocation methods 
# to get more control for CFLAG and CXXFLAG 
# (ndk-build messes up the inline ARM code 
# maybe I can fix this by passing parameters 
# from Android.mk later).
# retrieved most of the flags from ndk-build V=1

/home/aka/Android/android-ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/arm-linux-androideabi-gcc -MMD -MP -MF /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o.d.org -fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -Wno-psabi -march=armv5te -mtune=xscale -msoft-float -O0 -g -fno-strict-aliasing -finline-limit=64 -I/home/aka/S2E/s2eandroid/hellondk/jni -DANDROID  -Wa,--noexecstack -O0 -g -I/home/aka/Android/android-ndk/platforms/android-5/arch-arm/usr/include -c  /home/aka/S2E/s2eandroid/hellondk/jni/inlinearm.c -o /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o && ( if [ -f "/home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o.d.org" ]; then rm -f /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o.d && mv /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o.d.org /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o.d; fi )

/home/aka/Android/android-ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/arm-linux-androideabi-g++ -Wl,--gc-sections -Wl,-z,nocopyreloc --sysroot=/home/aka/Android/android-ndk/platforms/android-5/arch-arm  /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/objs-debug/s2etest/inlinearm.o        -Wl,--no-undefined -Wl,-z,noexecstack -L/home/aka/Android/android-ndk/platforms/android-5/arch-arm/usr/lib -llog -lc -lm -lsupc++ -o /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/s2etest

#push to running emulator session
adb push /home/aka/S2E/s2eandroid/hellondk/obj/local/armeabi/s2etest /data

#adb shell "export PATH=/data/busybox:$PATH"
#adb shell "./data/s2eandroid"
