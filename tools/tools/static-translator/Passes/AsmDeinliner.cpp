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

    const FunctionType *inlineAsmType = inlineAsm->getFunctionType();
    const Type *inlineAsmRetType = inlineAsmType->getReturnType();
    const Type *deinlinedRetType = NULL;
    const FunctionType *deinlinedType = NULL;

    //Prepare the deinlined function type.
    //We pass all aggregate return types as a parameter, as LLVM cannot lower
    //aggregate return values to machine code.
    std::vector<const Type *> paramTypes;

    if (inlineAsmRetType->isAggregateType()) {
        paramTypes.push_back(PointerType::getUnqual(inlineAsmRetType));
        deinlinedRetType = PointerType::getVoidTy(M->getContext());
    }else {
        deinlinedRetType = inlineAsmRetType;
    }

    foreach(it, inlineAsmType->param_begin(), inlineAsmType->param_end()) {
        paramTypes.push_back(*it);
    }

    deinlinedType = FunctionType::get(deinlinedRetType, paramTypes, false);

    Function *deinlinedFunction = dyn_cast<Function>(M->getOrInsertFunction(ss.str(), deinlinedType));
    deinlinedFunction->setLinkage(Function::InternalLinkage);

    BasicBlock *BB = BasicBlock::Create(M->getContext(), "", deinlinedFunction, NULL);

    //Create an identical call instruction
    InlineAsm *clonedInlineAsm = InlineAsm::get(inlineAsm->getFunctionType(),
                                               inlineAsm->getAsmString(),
                                               inlineAsm->getConstraintString(),
                                               inlineAsm->hasSideEffects());

    std::vector<Value*> params;
    bool useStructPtr = deinlinedRetType == PointerType::getVoidTy(M->getContext());
    bool skipFirst = useStructPtr;
    foreach (it, deinlinedFunction->arg_begin(), deinlinedFunction->arg_end()) {
        if (skipFirst) {
            skipFirst = false;
            continue;
        }
        params.push_back(&*it);
    }

    CallInst *asmCall = CallInst::Create(clonedInlineAsm, params.begin(), params.end(), "", BB);


    //Create the return instruction
    if (useStructPtr) {
        //Store aggregate result in the first parameter
        new StoreInst(asmCall, &*deinlinedFunction->arg_begin(), BB);
        //We return void
        ReturnInst::Create(M->getContext(), BB);
    }else {
        //Return an integer type directly
        ReturnInst::Create(M->getContext(), asmCall, BB);
    }

    //Replace the call site
    params.clear();

    Value *structHolder = NULL;
    if (useStructPtr) {
        //Create a data structure holder
        structHolder = new AllocaInst(inlineAsmRetType, "", ci);
        params.push_back(structHolder);
    }

    for (unsigned i = 1; i < ci->getNumOperands(); ++i) {
        params.push_back(ci->getOperand(i));
    }

    CallInst *newCi = CallInst::Create(deinlinedFunction, params.begin(), params.end(), "");
    newCi->insertBefore(ci);

    if (useStructPtr) {
        Instruction *loadedResult = new LoadInst(structHolder, "");
        loadedResult->insertAfter(newCi);
        ci->replaceAllUsesWith(loadedResult);
    }else {
        ci->replaceAllUsesWith(newCi);
    }

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
