#ifndef _ASM_NATIVEADAPTER_PASS_H_

#define _ASM_NATIVEADAPTER_PASS_H_

#include <llvm/Instructions.h>
#include <llvm/InlineAsm.h>
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Target/TargetData.h>

#include <lib/Utils/Log.h>

#include <map>

namespace s2etools {

/**
 * This pass replaces extracted inline assembly functions
 * with the equivalent native code.
 *
 * XXX: assumes x86 32-bit translated code
 *
 */
class AsmNativeAdapter: public llvm::ModulePass {
private:
    typedef std::map<llvm::Function*, llvm::AllocaInst*> CpuStateAllocs;

    static LogKey TAG;
    CpuStateAllocs m_cpuStateAllocs;

    llvm::Function *createNativeWrapper(llvm::Module &M,
                                        llvm::Function *deinlinedFunction,
                                        llvm::Function *nativeFunction);

    bool replaceDeinlinedFunction(llvm::Module &M,
                                  llvm::Function *deinlinedFunction,
                                  llvm::Function *nativeWrapper);

public:
    /**
     *  Maps an asmdein_ function to the native function_xxx.
     */
    typedef std::map<llvm::Function*, llvm::Function*> FunctionMap;

    static char ID;
    const FunctionMap &m_functionMap;
    const llvm::StructType* m_cpuStateType;
    llvm::TargetData *m_targetData;

    AsmNativeAdapter(const FunctionMap &functions) : ModulePass(&ID), m_functionMap(functions)  {

    }



    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    bool runOnModule(llvm::Module &M);
};

}

#endif
