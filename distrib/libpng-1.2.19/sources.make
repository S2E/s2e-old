# this file is included by various Makefiles and defines the set of sources used by our version of LibPng
#
LIBPNG_SOURCES := png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c \
                  pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngvcrd.c pngwio.c \
                  pngwrite.c pngwtran.c pngwutil.c

ifeq ($(HOST_OS),darwin)
    LIBPNG_CFLAGS += -DPNG_NO_MMX_CODE
else
    LIBPNG_SOURCES += pnggccrd.c
endif

LIBPNG_SOURCES := $(LIBPNG_SOURCES:%=$(LIBPNG_DIR)/%)

