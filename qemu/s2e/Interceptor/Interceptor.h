#ifndef INTERCEPTORS_H

#define INTERCEPTORS_H

#include <iostream>
#include <inttypes.h>
#include "ModuleDescriptor.h"

/** 
 *  Interface for intercepting the load of a module
 */
struct IInterceptor
{
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
  
  //Process to intercept
  virtual bool SetModule(const std::string &Name)=0;
  
  //Library name to intercept
  virtual bool SetSubModule(const std::string &Name)=0;
};

#endif
