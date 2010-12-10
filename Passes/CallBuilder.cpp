#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/InstIterator.h>

#include "CallBuilder.h"
#include "Utils.h"

#include <set>

using namespace llvm;

char CallBuilder::ID = 0;
RegisterPass<CallBuilder>
  CallBuilder("CallBuilder", "Rebuild all function calls",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

void CallBuilder::resolveCall(llvm::Module &M, llvm::Function &F, CallInst *ci)
{
    ConstantInt *targetPcCi = dyn_cast<ConstantInt>(ci->getOperand(1));
    assert(targetPcCi);
    const uint64_t targetPc = *targetPcCi->getValue().getRawData();

    //Fetch the right function
    FunctionMap::iterator it = m_functions.find(targetPc);

    //Might be null if from dll?
    assert(it != m_functions.end());

    Function *targetFunction = (*it).second;

    std::vector<Value*> CallArguments;
    CallArguments.push_back(&*F.arg_begin());
    CallInst *targetCall = CallInst::Create(targetFunction, CallArguments.begin(), CallArguments.end());
    targetCall->insertAfter(ci);
}

bool CallBuilder::runOnModule(llvm::Module &M)
{
    bool modified = false;

    Function *callMarker = M.getFunction("call_marker");
    assert(callMarker && "Run TerminatorMarker first");

    foreach(fit, M.begin(), M.end()) {
        Function &F = *fit;
        if (F.getNameStr().find("function_") == std::string::npos) {
            continue;
        }

        foreach(iit, inst_begin(F), inst_end(F)) {
            CallInst *ci = dyn_cast<CallInst>(&*iit);
            if (!ci || ci->getCalledFunction() != callMarker) {
                continue;
            }

            Value *isInlinable = ci->getOperand(2);
            if (isInlinable == ConstantInt::getTrue(M.getContext())) {
                //Skip jumps
                continue;
            }

            ConstantInt *targetPc = dyn_cast<ConstantInt>(ci->getOperand(1));
            if (!targetPc) {
                //Skip indirect calls
                continue;
            }

            resolveCall(M, F, ci);
            modified = true;
        }
    }

    return modified;
}
