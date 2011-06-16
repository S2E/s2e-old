#include <sstream>
#include <lib/Utils/Utils.h>
#include "AsmDeinliner.h"

using namespace llvm;

namespace s2etools {

// Register this pass...
static RegisterPass<AsmDeinliner> X("asmdeinliner", "Moves inline assembly instructions to separate LLVM functions");

char AsmDeinliner::ID = 0;
LogKey AsmDeinliner::TAG = LogKey("AsmDeinliner");

void AsmDeinliner::processInlineAsm(CallInst *ci, InlineAsm *inlineAsm)
{
    Module *M = ci->getParent()->getParent()->getParent();

    //Generate a new inline assembly function
    std::stringstream ss;
    ss << "asmdein_" << ci->getParent()->getParent()->getName().str() << "_" << m_inlineAsmId;

    Function *f = dyn_cast<Function>(M->getOrInsertFunction(ss.str(), inlineAsm->getFunctionType()));

}

bool AsmDeinliner::runOnModule(llvm::Module &M)
{
    bool found = false;

    foreach(fit, M.begin(), M.end()) {
        Function &F = *fit;
        foreach(bbit, F.begin(), F.end()) {
            BasicBlock &BB = *bbit;
            foreach(iit, BB.begin(), BB.end()) {
                Instruction &I = *iit;
                CallInst *ci = dyn_cast<CallInst>(&I);
                Value *v = ci->getCalledValue();                
                InlineAsm *asmInst = dyn_cast<InlineAsm>(v);
                if (!asmInst) {
                    continue;
                }

                processInlineAsm(ci, asmInst);
                found = true;
            }
        }
    }

    return found;
}



}
