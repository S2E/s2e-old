#ifndef _INSTR_CALLBUILDER_PASS_H_

#define _INSTR_CALLBUILDER_PASS_H_

#include "lib/BinaryReaders/Binary.h"

namespace s2etools {

/**
 * This pass transforms all call markers in the module to actual LLVM
 * function calls. Resolves internal and library function calls.
 */
class CallBuilder : public llvm::ModulePass {
    static char ID;
    static LogKey TAG;

    Binary *m_binary;

    bool resolveImport(uint64_t address, std::string &functionName);
    bool processIndirectCall(llvm::CallInst *marker);
    bool processLocalCall(llvm::CallInst *marker, llvm::Function *f);
    bool processCallMarker(llvm::Module &M, llvm::CallInst *marker);

    bool processCallMarker(llvm::CallInst *marker);

public:
    CallBuilder(Binary *binary) : ModulePass((intptr_t)&ID) {
        m_binary = binary;
    }

    virtual bool runOnModule(llvm::Module &M);

};

}

#endif
