#!/bin/bash
echo Building the ARM Hello World...
# '-EB' would mean: big endian
arm-linux-gnueabi-as -mcpu=arm926e -march=armv5te -alh -o hello.o s2earm-inst.S hello.S > hello.lst
arm-linux-gnueabi-ld -o hello.bin hello.o
