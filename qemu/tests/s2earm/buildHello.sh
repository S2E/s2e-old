#!/bin/bash
echo Building the ARM Linux Hello World...
# '-EB' means: big endian
arm-linux-gnueabi-as -mcpu=arm926e -march=armv5te -alh -o hello.o hello.S > hello.lst
arm-linux-gnueabi-ld --strip-all -o hello.bin hello.o
