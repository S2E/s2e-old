#ifndef INTERCEPTORS_H

#define INTERCEPTORS_H

#include <iostream>
#include <inttypes.h>
#include "ModuleDescriptor.h"

/** 
 *  Interface for intercepting the load one type of module
 */
class IInterceptor
{
public:
  virtual bool OnTbEnter(void *CpuState, bool Translation)=0;
  virtual bool OnTbExit(void *CpuState, bool Translation)=0;
  
  /**
   *  Called by the code translator module of the VM
   *  to see whether it has to translate the basic block to LLVM
   *  or to native binary code.
   *  The basic block is characterized by the page directory pointer
   *  (which usually identifies the process) and the program counter.
   */
  virtual bool DecideSymbExec(uint64_t cr3, uint64_t Pc)=0;
  virtual void DumpInfo(std::ostream &os)=0;
  virtual bool GetModule(ModuleDescriptor &Desc)=0;
  virtual ~IInterceptor()=0;
};

#endif
