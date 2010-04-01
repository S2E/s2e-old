#ifndef QEMU_KLEE_GLUE_H
#define QEMU_KLEE_GLUE_H

#include <string>
#include <inttypes.h>

namespace s2e {

// XXX: all of the following will be in S2EExecutionState

class QEMU
{
public:
  static bool GetAsciiz(uint64_t Addr, std::string &Ret);
  static std::string GetUnicode(uint64_t Addr, unsigned Length);
  static bool ReadVirtualMemory(uint64_t Addr, void *Buffer, unsigned Length);
  static uint64_t GetPhysAddr(uint64_t va);
  static void DumpVirtualMemory(uint64_t Addr, unsigned Length);
  static bool ReadInteger(uint64_t Addr, unsigned Size, uint64_t &Result);
  static char *GetAsciiz(uint64_t base);
};

} // namespace s2e

#endif
