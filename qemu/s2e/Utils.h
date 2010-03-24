#ifndef S2E_UTILS_H
#define S2E_UTILS_H

#include <cstdio>
#include <cassert>

#ifdef NDEBUG
#define DPRINTF(...)
#else
#define DPRINTF(...) printf(__VA_ARGS__)
#endif

#define foreach(_i, _b, _e) \
      for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)

#endif // S2E_UTILS_H
