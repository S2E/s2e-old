#ifndef _INSTR_CALL_BUILDER_PASS_H_

#define _INSTR_CALL_BUILDER_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"

#include <map>

/**
 * Rebuilds function calls
 */
struct CallBuilder : public llvm::ModulePass {
  static char ID;
  CallBuilder() : ModulePass((intptr_t)&ID) {
  }
public:
  typedef std::map<uint64_t, llvm::Function *> FunctionMap;


  CallBuilder(FunctionMap &fmap) : ModulePass((intptr_t)&ID) {
    m_functions = fmap;
  }


private:
   FunctionMap m_functions;

   void resolveCall(llvm::Module &M, llvm::Function &F, llvm::CallInst *ci);
   void createMapping(llvm::Module &M);
public:

  virtual bool runOnModule(llvm::Module &M);


};


#endif
