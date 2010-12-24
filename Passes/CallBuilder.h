#ifndef _INSTR_CALL_BUILDER_PASS_H_

#define _INSTR_CALL_BUILDER_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"

#include <lib/BinaryReaders/BFDInterface.h>
#include "../CFG/CFunction.h"

#include <map>

/**
 * Rebuilds function calls
 */
struct CallBuilder : public llvm::ModulePass {
  static char ID;
  CallBuilder() : ModulePass((intptr_t)&ID) {
  }
public:
  typedef std::map<uint64_t, s2etools::translator::CFunction *> FunctionMap;

  //Map an address to a pair of <module, function>
  typedef std::map<uint64_t, std::pair<std::string, std::string> > ImportMap;


  CallBuilder(s2etools::translator::CFunctions &fmap, s2etools::BFDInterface *binary) : ModulePass((intptr_t)&ID) {
    m_functions = fmap;
    m_binary = binary;
  }


private:
  s2etools::BFDInterface *m_binary;
  s2etools::translator::CFunctions m_functions;
  FunctionMap m_functionMap;
  s2etools::Imports m_imports;

   void resolveCall(llvm::Module &M, llvm::Function &F, llvm::CallInst *ci);
   void createMapping(llvm::Module &M);
   uint64_t resolveStub(llvm::Module &M, uint64_t callee);
   bool patchCallWithLibraryFunction(llvm::Module &M, llvm::CallInst *ci, std::string &functionName);
   bool resolveLibraryCall(llvm::Module &M, llvm::Function &F, llvm::CallInst *ci);
   void resolveIndirectCall(llvm::Module &M, llvm::Function &F, llvm::CallInst *ci);
public:

  virtual bool runOnModule(llvm::Module &M);


};


#endif
