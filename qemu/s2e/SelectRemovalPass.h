#ifndef _SELECT_REMOVE_PASS_H_

#define _SELECT_REMOVE_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"


  struct SelectRemovalPass : public llvm::FunctionPass {
     static char ID;
     SelectRemovalPass() : FunctionPass((intptr_t)&ID) {}

     virtual bool runOnFunction(llvm::Function &F);
     

  };



#endif
