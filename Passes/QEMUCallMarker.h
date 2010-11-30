#ifndef _INSTR_CALL_MARK_PASS_H_

#define _INSTR_CALL_MARK_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

/**
 *  This pass inserts a call marker in the LLVM bitcode.
 *  Assumes that the passed function belongs to a call translation block.
 */
struct QEMUCallMarker : public llvm::FunctionPass {
  static char ID;
  QEMUCallMarker() : FunctionPass((intptr_t)&ID) {
      m_callMarker = NULL;
  }

  llvm::Function *m_callMarker;
private:
  void initCallMarker(llvm::Module *module);
  void markCall(llvm::CallInst *Ci);

public:
  virtual bool runOnFunction(llvm::Function &F);


};


#endif
