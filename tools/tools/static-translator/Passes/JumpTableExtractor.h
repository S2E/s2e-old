#ifndef _INSTR_JMPTBL_EXTRACTOR_PASS_H_

#define _INSTR_JMPTBL_EXTRACTOR_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include "lib/Utils/Log.h"

#include <set>
#include <string>

namespace s2etools {

/**
 * Extracts all constant integers from an LLVM function.
 */
struct JumpTableExtractor : public llvm::FunctionPass {
  static char ID;
  JumpTableExtractor() : FunctionPass((intptr_t)&ID) {
    m_jumpTableAddress = 0;
    m_jumpTableSize = 0;
  }

private:
  static LogKey TAG;
  uint64_t m_jumpTableAddress;
  uint64_t m_jumpTableSize;

  llvm::Value *getIndirectCallAddress(llvm::Function &F);
  uint64_t getOffset(llvm::Value *address) const;

public:
  virtual bool runOnFunction(llvm::Function &F);

  uint64_t getJumpTableAddress() const {
      return m_jumpTableAddress;
  }

};

}

#endif
