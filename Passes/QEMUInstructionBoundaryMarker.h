#ifndef _INSTR_BOUND_MARK_PASS_H_

#define _INSTR_BOUND_MARK_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

/**
 *  This pass inserts a marker in the LLVM bitcode between blocks of
 *  LLVM instructions belonging to the same original x86 machine instruction.
 */
struct QEMUInstructionBoundaryMarker : public llvm::FunctionPass {
  static char ID;
  QEMUInstructionBoundaryMarker() : FunctionPass((intptr_t)&ID) {
      m_instructionMarker = NULL;
  }

  llvm::Function *m_instructionMarker;
private:
  void initInstructionMarker(llvm::Module *module);
  void markBoundary(llvm::CallInst *Ci);

public:
  virtual bool runOnFunction(llvm::Function &F);


};


#endif
