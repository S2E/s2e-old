#ifndef _TBPREPROP_PASS_H_

#define _TBPREPROP_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

#include "Translator.h"

namespace s2etools {

/**
 *  This pass inserts a call marker in the LLVM bitcode.
 *  Assumes that the passed function belongs to a call translation block.
 */
struct TbPreprocessor : public llvm::FunctionPass {
  static char ID;
  TbPreprocessor() : FunctionPass((intptr_t)&ID) {
      m_tb = NULL;
      m_callMarker = NULL;
      m_jumpMarker = NULL;
      m_instructionMarker = NULL;
      m_returnMarker = NULL;
  }

  TbPreprocessor(TranslatedBlock *tb) : FunctionPass((intptr_t)&ID) {
    m_tb = tb;
    m_callMarker = NULL;
    m_jumpMarker = NULL;
    m_instructionMarker = NULL;
    m_returnMarker = NULL;
}


public:
  typedef std::set<uint64_t> StaticTargets;
  typedef std::vector<llvm::Instruction*> Instructions;
  typedef std::vector<llvm::BasicBlock*> BasicBlocks;

private:
  static std::string TAG;

    llvm::Function *m_callMarker;
    llvm::Function *m_returnMarker;
    llvm::Function *m_jumpMarker;
    llvm::Function *m_instructionMarker;

    llvm::Function *m_forkAndConcretize;

    TranslatedBlock *m_tb;

    static std::string s_instructionMarker;
    static std::string s_jumpMarker;
    static std::string s_callMarker;
    static std::string s_returnMarker;
private:
    void initMarkers(llvm::Module *module);
    void markInstructionStart(llvm::Function &f);
    llvm::Value *getTargetPc(llvm::BasicBlock &bb, llvm::CallInst **marker);
    bool findFinalPcAssignments(llvm::Function &f, llvm::Value **destination, llvm::Value **fallback,
                                                llvm::CallInst **destMarker, llvm::CallInst **fbMarker);
    llvm::CallInst *buildCallMarker(llvm::Function &f, llvm::Value *v);
    llvm::CallInst *buildReturnMarker();
    void markCall(llvm::Function &f);
    void markReturn(llvm::Function &f);
    void findReturnInstructions(llvm::Function &f, Instructions &Result);
    void extractJumpInfo(llvm::Function &f);
    void removeForkAndConcretize(llvm::Function &F);

public:

  virtual bool runOnFunction(llvm::Function &F);

  static const std::string& getInstructionMarker() {
      return s_instructionMarker;
  }

  static const std::string& getReturnMarker() {
      return s_returnMarker;
  }

  static const std::string& getJumpMarker() {
      return s_jumpMarker;
  }

  static const std::string& getCallMarker() {
      return s_callMarker;
  }
};

}

#endif
