#ifndef _ASM_DEINLINER_PASS_H_

#define _ASM_DEINLINER_PASS_H_

#include <llvm/Instructions.h>
#include <llvm/InlineAsm.h>
#include <llvm/Pass.h>
#include <llvm/Module.h>

#include <lib/Utils/Log.h>

namespace s2etools {

class AsmDeinliner: public llvm::ModulePass {
private:
    static LogKey TAG;
    unsigned m_inlineAsmId;

    void processInlineAsm(llvm::CallInst *ci, llvm::InlineAsm *inlineAsm);
public:
    static char ID;

    AsmDeinliner() : ModulePass(&ID){
        m_inlineAsmId = 0;
    }
    bool runOnModule(llvm::Module &M);
};

}

#endif
