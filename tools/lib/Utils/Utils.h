#ifndef TRANSLATOR_UTILS_H

#define TRANSLATOR_UTILS_H

#define foreach(_i, _b, _e) \
      for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)

#include <llvm/ADT/DenseMapInfo.h>

namespace llvm {
  // Provide DenseMapInfo for uint64_t
  template<> struct DenseMapInfo<uint64_t> {
    static inline uint64_t getEmptyKey() { return ~0L; }
    static inline uint64_t getTombstoneKey() { return ~0L - 1L; }
    static unsigned getHashValue(const uint64_t& Val) {
      return (unsigned)(Val * 37L);
    }
    static bool isPod() { return true; }
    static bool isEqual(const uint64_t& LHS, const uint64_t& RHS) {
    return LHS == RHS;
    }
  };
}

#endif
