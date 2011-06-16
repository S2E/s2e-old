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

    BasicBlock *BB = BasicBlock::Create(M->getContext(), "", f, NULL);

    //Create an identical call instruction
    InlineAsm *clonedInlineAsm = InlineAsm::get(inlineAsm->getFunctionType(),
                                               inlineAsm->getAsmString(),
                                               inlineAsm->getConstraintString(),
                                               inlineAsm->hasSideEffects());

    std::vector<Value*> params;
    foreach (it, f->arg_begin(), f->arg_end()) {
        params.push_back(&*it);
    }

    CallInst *asmCall = CallInst::Create(clonedInlineAsm, params.begin(), params.end(), "", BB);

    ReturnInst::Create(M->getContext(), asmCall, BB);

    //Replace the call site
    params.clear();
    for (unsigned i = 1; i < ci->getNumOperands(); ++i) {
        params.push_back(ci->getOperand(i));
    }

    CallInst *newCi = CallInst::Create(f, params.begin(), params.end(), "");
    newCi->insertBefore(ci);
    ci->replaceAllUsesWith(newCi);
    ci->eraseFromParent();
}

bool AsmDeinliner::runOnModule(llvm::Module &M)
{
    bool found = false;

    std::vector<CallInst *> toProcess;

    foreach(fit, M.begin(), M.end()) {
        Function &F = *fit;
        foreach(bbit, F.begin(), F.end()) {
            BasicBlock &BB = *bbit;
            foreach(iit, BB.begin(), BB.end()) {
                Instruction &I = *iit;
                CallInst *ci = dyn_cast<CallInst>(&I);
                if (!ci) {
                    continue;
                }

                Value *v = ci->getCalledValue();                
                InlineAsm *asmInst = dyn_cast<InlineAsm>(v);
                if (!asmInst) {
                    continue;
                }

                toProcess.push_back(ci);
            }
        }
    }

    //Do processing after scanning, as processing may invalidate iterators
    foreach(it, toProcess.begin(), toProcess.end()) {
        CallInst *ci = *it;
        InlineAsm *asmInst = dyn_cast<InlineAsm>(ci->getCalledValue());
        processInlineAsm(ci, asmInst);
        found = true;
    }

    return found;
}



}
