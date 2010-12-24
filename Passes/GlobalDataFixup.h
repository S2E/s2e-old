#ifndef _INSTR_GLOBAL_FIXUP_PASS_H_

#define _INSTR_GLOBAL_FIXUP_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <lib/BinaryReaders/BFDInterface.h>
#include <string>
#include <set>
#include <stdint.h>

/**
 * Injects global data from the binary into the LLVM module.
 * Also fixes all references to it in the code.
 */
struct GlobalDataFixup : public llvm::ModulePass {
  static char ID;
  GlobalDataFixup() : ModulePass((intptr_t)&ID) {

  }

  GlobalDataFixup(s2etools::BFDInterface *binary) : ModulePass((intptr_t)&ID) {
    m_binary = binary;
  }

private:
  typedef std::map<s2etools::BFDSection, llvm::GlobalVariable*> Sections;
  typedef std::map<uint64_t, llvm::Instruction*> ProgramCounters;

  s2etools::BFDInterface *m_binary;
  Sections m_sections;

  void createGlobalArray(llvm::Module &M, const std::string &name, uint8_t *data, uint64_t va, unsigned size);
  void injectDataSections(llvm::Module &M);

  bool patchFunctionPointer(llvm::Module &M, llvm::Value *ptr);
  bool patchImportedDataPointer(llvm::Module &M,
                                llvm::Instruction *instructionMarker,
                                llvm::Value *ptr, const s2etools::RelocationEntry &relEntry);
  void patchDataPointer(llvm::Module &M, llvm::Value *ptr);
  void patchRelocations(llvm::Module &M);

//  void patchAddress(llvm::Module &M, llvm::CallInst *ci, bool isLoad, unsigned accessSize);
  void initMemOps(llvm::Module &M);

  void getProgramCounters(llvm::Module &M, ProgramCounters &counters);
  llvm::ConstantInt *findValue(llvm::Instruction *boundary, uint64_t value, llvm::Instruction **owner);

  //The bool indicates load (true) or store (false)
  std::map<llvm::Function *, std::pair<unsigned, bool> > m_memops;

public:

  virtual bool runOnModule(llvm::Module &M);

};


#endif
