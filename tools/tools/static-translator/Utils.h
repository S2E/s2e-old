#ifndef TRANSLATOR_UTILS_H

#define TRANSLATOR_UTILS_H

#define foreach(_i, _b, _e) \
      for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)

#endif
