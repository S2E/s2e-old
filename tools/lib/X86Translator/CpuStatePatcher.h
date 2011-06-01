#ifndef _CPU_STATE_PATCHER_PASS_H_

#define _CPU_STATE_PATCHER_PASS_H_

#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Target/TargetData.h>

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
    const llvm::StructType* m_envType;
    const llvm::TargetData *m_targetData;
    const llvm::StructLayout *m_envLayout;
    llvm::Function *m_transformed;

    llvm::Instruction *createGep(llvm::Instruction *instr,
                                 llvm::ConstantInt *offset,
                                 bool &dropInstruction,
                                 unsigned accessSize) const;
    void getAllLoadStores(Instructions &I, llvm::Function &F) const;
    void patchOffset(llvm::Instruction *intr) const;
    void patchOffsets(llvm::Function *transformed) const;
    void transformArguments(llvm::Function *transformed, llvm::Function *original) const;
    llvm::Function *createTransformedFunction(llvm::Function &original) const;

public:
    CpuStatePatcher(uint64_t instructionAddress);
    ~CpuStatePatcher();

    virtual bool runOnFunction(llvm::Function &F);

    llvm::Function *getTransformed() const {
        return m_transformed;
    }

    static std::string getCpuStateTypeName() {
        return s_cpuStateTypeName;
    }

    static const llvm::StructType* getCpuStateType(llvm::Module &M) {
        return llvm::dyn_cast<llvm::StructType>(M.getTypeByName(getCpuStateTypeName()));
    }

    static llvm::Value* getCpuStateParam(llvm::Function &F) {
        return F.arg_begin();
    }
};

}

#endif
