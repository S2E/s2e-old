#ifndef _INSTR_INLINER_PASS_H_

#define _INSTR_INLINER_PASS_H_

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "lib/Utils/Log.h"

namespace s2etools {

/**
 * This pass inlines all calls to instruction functions into
 * one big function, better suited for analysis.
 */
class InstructionInliner : public llvm::ModulePass {
    static char ID;
    static LogKey TAG;


public:
    InstructionInliner() : ModulePass((intptr_t)&ID) {
    }

    virtual bool runOnModule(llvm::Module &M);

};

}

#endif
