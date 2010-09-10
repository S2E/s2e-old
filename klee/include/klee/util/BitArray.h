//===-- BitArray.h ----------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_UTIL_BITARRAY_H
#define KLEE_UTIL_BITARRAY_H

namespace klee {

  // XXX would be nice not to have
  // two allocations here for allocated
  // BitArrays
class BitArray {
private:
  // XXX(s2e) for now we keep this first to access from C code
  // (yes, we do need to access if really fast)
  uint32_t *bits;
  
protected:
  static uint32_t length(unsigned size) { return (size+31)/32; }

public:
  BitArray(unsigned size, bool value = false) : bits(new uint32_t[length(size)]) {
    memset(bits, value?0xFF:0, sizeof(*bits)*length(size));
  }
  BitArray(const BitArray &b, unsigned size) : bits(new uint32_t[length(size)]) {
    memcpy(bits, b.bits, sizeof(*bits)*length(size));
  }
  ~BitArray() { delete[] bits; }

  inline bool get(unsigned idx) { return (bool) ((bits[idx/32]>>(idx&0x1F))&1); }
  inline void set(unsigned idx) { bits[idx/32] |= 1<<(idx&0x1F); }
  inline void unset(unsigned idx) { bits[idx/32] &= ~(1<<(idx&0x1F)); }
  inline void set(unsigned idx, bool value) { if (value) set(idx); else unset(idx); }

  bool isAllZeros(unsigned size) {
    for(unsigned i = 0; i < size/32; ++i)
      if(bits[i] != 0)
        return false;
    uint32_t mask = (1 << (size&0x1F)) - 1;
    return (bits[size/32] & mask) == 0;
  }

  bool isAllOnes(unsigned size) {
    for(unsigned i = 0; i < size/32; ++i)
      if(bits[i] != 0xffffffff)
        return false;
    uint32_t mask = (1 << (size&0x1F)) - 1;
    return (bits[size/32] & mask) == mask;
  }
};

} // End klee namespace

#endif
