#ifndef _WINDBG_STARTSIZE_

#define _WINDBG_STARTSIZE_

#include <inttypes.h>

struct StartSize {
    uint64_t Start, Size;
    bool operator<(const StartSize &i2) const {
        return Start+Size <= i2.Start;
    }
    StartSize() {
        Start = Size = 0;
    }
    StartSize(uint64_t st, uint64_t sz) {
        Start = st;
        Size = sz;
    }
};


#endif
