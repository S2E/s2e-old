#ifndef _INSTR_CALLBUILDER_PASS_H_

#define _INSTR_CALLBUILDER_PASS_H_

#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#include "lib/BinaryReaders/Binary.h"

namespace s2etools {

/**
 * This pass transforms all call markers in the module to actual LLVM
 * function calls. Resolves internal and library function calls.
 */
class CallBuilder : public llvm::ModulePass {
    static LogKey TAG;

    Binary *m_binary;

    llvm::MemoryDependenceAnalysis *m_memDepAnalysis;

    llvm::StoreInst *getRegisterDefinition(llvm::LoadInst *load);

    bool resolveImport(uint64_t address, std::string &functionName);
    bool processIndirectCall(llvm::CallInst *marker);
    bool processLocalCall(llvm::CallInst *marker, llvm::Function *f);
    bool processCallMarker(llvm::Module &M, llvm::CallInst *marker);

    bool processCallMarker(llvm::CallInst *marker);

public:
    static char ID;

    CallBuilder() : ModulePass(&ID){
        m_binary = NULL;
    }

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    bool runOnModule(llvm::Module &M);
};

}

#endif
