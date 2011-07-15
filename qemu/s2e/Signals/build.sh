#!/bin/sh

FLAGS=`pkg-config --libs --cflags sigc++-2.0`
LLVM="-I/Users/vitaly/S2E/llvm-2.6/include/ -I/Users/vitaly/S2E/llvm-2.6-obj/include/"
g++ -o test $LLVM $FLAGS *.cpp
