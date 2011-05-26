#!/bin/sh

############################################
#This script builds the the S2E toolchain prerequesites from scratch.

#Change this to reflect the number of CPU cores you have
JOBS=-j8
#The following files must be present in this script's directory.
#The working directory must be the script's directory.
ARCHIVES=
ARCHIVES="$ARCHIVES perl-5.6.1_2-1-msys-1.0.11-bin.tar.lzma"
ARCHIVES="$ARCHIVES libcrypt-1.1_1-2-msys-1.0.11-dll-0.tar.lzma"
ARCHIVES="$ARCHIVES bison-2.4.1-1-msys-1.0.11-bin.tar.lzma"
ARCHIVES="$ARCHIVES flex-2.5.35-1-msys-1.0.11-bin.tar.lzma"
ARCHIVES="$ARCHIVES libregex-0.12-1-msys-1.0.11-dll-0.tar.lzma"
ARCHIVES="$ARCHIVES libregex-1.20090805-2-msys-1.0.13-dll-1.tar.lzma"
ARCHIVES="$ARCHIVES unzip-6.0-1-msys-1.0.13-bin.tar.lzma"
ARCHIVES="$ARCHIVES groff-1.20.1-1-msys-1.0.11-bin.tar.lzma"
ARCHIVES="$ARCHIVES groff-1.20.1-1-msys-1.0.11-ext.tar.lzma"
ARCHIVES="$ARCHIVES coreutils-5.97-2-msys-1.0.11-bin.tar.lzma"
ARCHIVES="$ARCHIVES coreutils-5.97-2-msys-1.0.11-ext.tar.lzma"
ARCHIVES="$ARCHIVES sed-4.2.1-1-msys-1.0.11-bin.tar.lzma"

MINGW_ARCHIVE="mingw-w64-bin_x86_64-mingw_20091224_sezero.zip"
SDL_ARCHIVE="SDL-1.2.14.zip"

############################################
if [ $# -ne 1 ]; then
  echo "Usage: $0 [path to toolchain root]"
  echo ""
  echo "Example: $0 /d/s2e-toolchain"
  exit
fi

ROOT="$1"
ARCHIVES_DIR="`pwd`"
OLDDIR="`pwd`"

if [ ! -d "$ROOT" ]; then
  echo "$ROOT does not exist, make sure you unpack msysCORE-1.0.11-bin.tar.gz in it."
  exit
fi

if [ ! -d "$ROOT/bin" ]; then
  echo "$ROOT/bin does not exist, make sure you unpack msysCORE-1.0.11-bin.tar.gz in it."
  exit
fi

cd "$ROOT"

for archive in $ARCHIVES; do
  if [ ! -f $archive ]; then
      echo "Unpacking $ARCHIVES_DIR/$archive into `pwd`"
      cp "$ARCHIVES_DIR/$archive" .
      lzma -d "$archive"
      UNTARED=`basename "$archive" .lzma`
      tar xvf "$UNTARED"
  fi
done

if [ ! -d mingw64 ]; then
  unzip "$ARCHIVES_DIR/$MINGW_ARCHIVE" -d .
  cp "$ARCHIVES_DIR/nasm.exe" "mingw64/x86_64-w64-mingw32/bin/"
fi

MSYS_BASE="`pwd`"
MINGW_BASE="$MSYS_BASE/mingw64/x86_64-w64-mingw32"
MINGW_BIN="$MINGW_BASE/bin"
MINGW_LIB="$MINGW_BASE/lib"
MINGW_INCLUDE="$MINGW_BASE/include"

#Setup PATH
export PATH="`pwd`/mingw64/x86_64-w64-mingw32/bin:$PATH"
export PATH="`pwd`/mingw64/bin:$PATH"

#Get rid of the make supplied with mingw to avoid interference.
#It does not understand msys paths
mv "$MSYS_BASE/mingw64/bin/make.exe" "$MINGW_BIN/mingw32-make.exe"

cp "$ARCHIVES_DIR/wget.exe" /bin

cp "$ARCHIVES_DIR"/cygwintools/* /bin

#Install zlib
cd
if [ ! -d "zlib-1.2.5" ]; then
cp "$ARCHIVES_DIR/zlib-1.2.5.tar.gz" .
tar xzvf zlib-1.2.5.tar.gz
cd zlib-1.2.5
make -f win32/Makefile.gcc BINARY_PATH="$MINGW_BIN" INCLUDE_PATH="$MINGW_INCLUDE" LIBRARY_PATH="$MINGW_LIB"
make -f win32/Makefile.gcc install BINARY_PATH="$MINGW_BIN" INCLUDE_PATH="$MINGW_INCLUDE" LIBRARY_PATH="$MINGW_LIB"
else
echo "zlib already installed, skipping"
fi

#Install binutils
cd
if [ ! -d "binutils-2.21" ]; then
cp "$ARCHIVES_DIR/binutils-2.21.tar.bz2" .
tar xjvf binutils-2.21.tar.bz2
cd binutils-2.21
./configure --enable-all-targets --build=x86_64-w64-mingw32 --prefix="$MINGW_BASE" --enable-ld=no
make $JOBS

cd bfd
make install
cd ..

cp intl/libintl.a "$MINGW_LIB"
else
echo "binutils already installed, skipping"
fi


#Install iconv
cd
if [ ! -d "libiconv-1.13.1" ]; then
cp "$ARCHIVES_DIR/libiconv-1.13.1.tar.gz" .
tar xzvf libiconv-1.13.1.tar.gz
cd libiconv-1.13.1
./configure --prefix="$MINGW_BASE" --build=x86_64-w64-mingw32
make install $JOBS
else
echo "iconv already installed, skipping"
fi

#Install gettext
cd
if [ ! -d "gettext-0.18.1.1" ]; then
cp "$ARCHIVES_DIR/gettext-0.18.1.1.tar.gz" .
tar xzvf gettext-0.18.1.1.tar.gz
cd gettext-0.18.1.1
./configure --prefix="$MINGW_BASE" --build=x86_64-w64-mingw32
make install $JOBS
else
echo "gettext already installed, skipping"
fi



#Install SDL
cd
if [ ! -d SDL-1.2.14 ]; then
   unzip "$SDL_ARCHIVE" -d .
   cd SDL-1.2.14
   ./configure CFLAGS=-DNO_STDIO_REDIRECT --prefix="$MINGW_BASE" --build=x86_64-w64-mingw32
   make install $JOBS
else
echo "sdl already installed, skipping"
fi

#Configure sdl-config
# AG: I have removed this step since the configuration
# is now done by the make install itself
#SDL_CONFIG="$MINGW_BASE/bin/sdl-config"
#SDL_PREFIX=`grep -E '^prefix=' "$SDL_CONFIG"`
#if [ "x$SDL_PREFIX" = "x" ]; then
# echo "Something went wrong with SDL prefix"
# exit
#fi

#sed "s:$SDL_PREFIX:prefix=$MINGW_BASE:g" "$SDL_CONFIG" > __tmp
#mv __tmp "$SDL_CONFIG"

#Install LUA
cd
if [ ! -d "lua-5.1.4" ]; then
cp "$ARCHIVES_DIR/lua-5.1.4.tar.gz" .
tar xzvf lua-5.1.4.tar.gz
cd lua-5.1.4
LUA_PREFIX=`grep -E '^INSTALL_TOP=' Makefile`
sed "s:$LUA_PREFIX:INSTALL_TOP= $MINGW_BASE:g" Makefile > __tmp
mv __tmp Makefile
make mingw
echo "installing..."
make install
else
echo "LUA already installed, skipping"
fi

#Install libsigc++
cd
if [ ! -d "libsigc++-2.2.9" ]; then
cp "$ARCHIVES_DIR/libsigc++-2.2.9.tar.gz" .
tar xzvf libsigc++-2.2.9.tar.gz
cd libsigc++-2.2.9
./configure --prefix="$MINGW_BASE" CC=gcc CXX=g++
make install $JOBS
./configure --prefix="$MINGW_BASE" CC=gcc CXX=g++ --enable-static
make install $JOBS

mv "$MINGW_BASE/include/sigc++-2.0/sigc++" "$MINGW_BASE/include/"
mv "sigc++config.h" "$MINGW_BASE/include/"
else
echo "sigc++ already installed, skipping"
fi

#Install pdcurses
cd
if [ ! -d "PDCurses-3.4" ]; then
cp "$ARCHIVES_DIR/PDCurses-3.4.tar.gz" .
tar xzvf PDCurses-3.4.tar.gz
cd PDCurses-3.4/win32
make -f gccwin32.mak DLL=N

cp pdcurses.a "$MINGW_BASE/lib/libcurses.a"
cp pdcurses.dll "$MINGW_BASE/bin/"
cp ../*.h "$MINGW_BASE/include/"
cd ..
else
echo "pdcurses already installed, skipping"
fi

#Install libexpat
cd
if [ ! -d "expat-2.0.1" ]; then
cp "$ARCHIVES_DIR/expat-2.0.1.tar.gz" .
tar xzvf expat-2.0.1.tar.gz
cd expat-2.0.1
./configure --prefix="$MINGW_BASE" --build=x86_64-w64-mingw32
make install
else
echo "libexpat already installed, skipping"
fi

#Install gdb
cd
if [ ! -d "gdb-7.0.1" ]; then
cp "$ARCHIVES_DIR/gdb-7.0.1.tar.bz2" .
tar xjvf gdb-7.0.1.tar.bz2
cd gdb-7.0.1
./configure -enable-tui --prefix="$MINGW_BASE" --build=x86_64-w64-mingw32
make $JOBS
cp gdb/gdb.exe $MINGW_BASE/bin
else
echo "gdb already installed, skipping"
fi


echo ""
echo "==============================================================="
echo "Add the following folders on your %PATH%, in the control panel."
echo "Convert them to Windows format first."
echo ""
echo "  $MSYS_BASE/mingw64/x86_64-w64-mingw32/bin"
echo "  $MSYS_BASE/mingw64/bin"
cd "$OLDDIR"
