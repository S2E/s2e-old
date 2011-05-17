#ifndef _INSTR_CSTE_EXTRACTOR_PASS_H_

#define _INSTR_CSTE_EXTRACTOR_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

/**
 * Extracts all constant integers from an LLVM function.
 */
struct ConstantExtractor : public llvm::FunctionPass {
  static char ID;
  ConstantExtractor() : FunctionPass((intptr_t)&ID) {
  }


public:
  typedef std::set<uint64_t> Constants;

private:
    Constants m_constants;

public:

  virtual bool runOnFunction(llvm::Function &F);

  const Constants &getConstants() const {
      return m_constants;
  }

};


#endif
