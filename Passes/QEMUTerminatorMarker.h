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
struct QEMUTerminatorMarker : public llvm::FunctionPass {
  static char ID;
  QEMUTerminatorMarker() : FunctionPass((intptr_t)&ID) {
      m_callMarker = NULL;
      m_returnMarker = NULL;

      m_inlinable = false;
      m_return = false;
  }

  QEMUTerminatorMarker(bool inlinable, bool ret) : FunctionPass((intptr_t)&ID) {
      assert(!(inlinable && ret));

      m_callMarker = NULL;
      m_returnMarker = NULL;

      m_inlinable = inlinable;
      m_return = ret;
  }


public:
  typedef std::set<uint64_t> StaticTargets;
  typedef std::vector<llvm::Instruction*> Instructions;
  typedef std::vector<llvm::BasicBlock*> BasicBlocks;

private:

  llvm::Function *m_callMarker;
  llvm::Function *m_returnMarker;

  bool m_inlinable;
  bool m_return;
private:
  void initMarkers(llvm::Module *module);
  void markCall(llvm::CallInst *Ci);
  void markReturn(llvm::CallInst *Ci);

  StaticTargets m_staticTargets;

public:
  static void findReturnInstructions(llvm::Function &F, Instructions &Result);

  virtual bool runOnFunction(llvm::Function &F);

  const StaticTargets &getStaticTargets() const {
      return m_staticTargets;
  }

};


#endif
