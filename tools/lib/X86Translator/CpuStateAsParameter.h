#ifndef _CPUSTATE_AS_PARAM_PASS_H_

#define _CPUSTATE_AS_PARAM_PASS_H_

#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/ADT/DenseSet.h>

#include <lib/Utils/Log.h>

namespace s2etools {

/**
 * Makes the CPUState a parameter of every helper function
 * that uses it.
 */
class CpuStateAsParameter: public llvm::ModulePass {
private:
    typedef llvm::DenseSet<llvm::Value*> Values;
    typedef llvm::DenseMap<llvm::Function*, llvm::Function*> OriginalToPatched;

    static LogKey TAG;

    llvm::GlobalVariable *m_env;
    const llvm::Type *m_cpuStateType;

    llvm::DenseSet<llvm::Function *> m_functionsUsingEnv;

    bool patchInstruction(llvm::User *user, OriginalToPatched &patchMap);
    llvm::Function *createFunction(llvm::Module &M, llvm::Function *original);
    void getValuesToPatch(llvm::Module &M, llvm::Function *F,
                             Values &toPatch);

    void appendEnvParam(llvm::Module &M, Values &toPatch,
                                             OriginalToPatched &patchMap);

    void patchCallSite(llvm::CallInst *ci, OriginalToPatched &patchMap);
    void patchUses(llvm::Module &M, OriginalToPatched &patchMap);
    llvm::Function* patchFunction(llvm::Module &M, llvm::Function *F);
    void patchEnv(llvm::Module &M);
public:
    static char ID;

    CpuStateAsParameter() : ModulePass(&ID){

    }

    bool runOnModule(llvm::Module &M);
};

}

#endif
