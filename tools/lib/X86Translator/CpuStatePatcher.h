#ifndef _CPU_STATE_PATCHER_PASS_H_

#define _CPU_STATE_PATCHER_PASS_H_

#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>

#include <vector>
#include <string>

#include "lib/Utils/Log.h"

namespace s2etools {

/**
 * This pass transforms all call markers in the module to actual LLVM
 * function calls. Resolves internal and library function calls.
 */
class CpuStatePatcher : public llvm::FunctionPass {
    static char ID;
    static LogKey TAG;

    typedef std::vector<llvm::Instruction*> Instructions;

    static std::string s_cpuStateTypeName;

    uint64_t m_instructionAddress;

    void getAllLoadStores(Instructions &I, llvm::Function &F) const;
    void transformArguments(llvm::Function *transformed, llvm::Function *original) const;
    llvm::Function *createTransformedFunction(llvm::Function &original) const;

public:
    CpuStatePatcher(uint64_t instructionAddress);

    virtual bool runOnFunction(llvm::Function &F);

};

}

#endif
