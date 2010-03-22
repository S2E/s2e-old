#ifndef INTERCEPTORS_H

#define INTERCEPTORS_H

#include <iostream>
#include <inttypes.h>
#include "ModuleDescriptor.h"
#include "ExecutableImage.h"

struct IInterceptorEvent
{
  virtual void OnProcessLoad(
    struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc
  ) = 0;

  virtual void OnLibraryLoad(
    struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Exports) = 0;
};




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
  /**
   *  XXX: This should be moved to the policy module.
   *  The IInterceptor interface defines an interface for mechanisms.
   */
  //virtual bool DecideSymbExec(uint64_t cr3, uint64_t Pc)=0;
  
  
  virtual void DumpInfo(std::ostream &os)=0;
  virtual bool GetModule(ModuleDescriptor &Desc)=0;
  
  virtual void SetEventHandler(struct IInterceptorEvent *Hdlr) = 0;
};


#endif
