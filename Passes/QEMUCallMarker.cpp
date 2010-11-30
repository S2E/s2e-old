#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "QEMUCallMarker.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>


using namespace llvm;

char QEMUCallMarker::ID = 0;
RegisterPass<QEMUCallMarker>
  QEMUCallMarker("qemucallmarker", "Inserts a call marker at the end of the block",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

void QEMUCallMarker::initCallMarker(llvm::Module *module)
{
    if (m_callMarker) {
        return;
    }

    FunctionType *type;
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));

    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_callMarker = dyn_cast<Function>(module->getOrInsertFunction("call_marker", type));
}

/**
 * XXX: also extract info about indirect calls
 */
void QEMUCallMarker::markCall(CallInst *Ci)
{
  Value *programCounter = Ci->getOperand(1);

  std::vector<Value*> CallArguments;
  CallArguments.push_back(programCounter);
  CallInst *marker = CallInst::Create(m_callMarker, CallArguments.begin(), CallArguments.end());
  marker->insertBefore(Ci);

  Ci->replaceAllUsesWith(programCounter);
  Ci->eraseFromParent();
}

bool QEMUCallMarker::runOnFunction(llvm::Function &F)
{
    bool modified = false;
    std::vector<CallInst *> calls;

    initCallMarker(F.getParent());

    //The translation block must have only one LLVM basic block in it!
    Function::iterator bbit;
    bbit = F.begin();
    ++bbit;
    assert(bbit == F.end());


    BasicBlock &bb = *F.begin();

    ilist<Instruction>::reverse_iterator riit(bb.end());
    ilist<Instruction>::reverse_iterator riitEnd(bb.begin());


    for(; riit != riitEnd; ++riit) {
      Instruction *I = &*riit;

      CallInst *Ci = dyn_cast<CallInst>(I);
      if (!Ci) {
        continue;
      }

      Function *F = Ci->getCalledFunction();
      if (!F) {
        continue;
      }
      const char *Fn = F->getNameStr().c_str();
      bool isACall = strstr(Fn, "tcg_llvm_fork_and_concretize") != NULL;

      if (isACall) {
        calls.push_back(Ci);
      }
    }



  std::vector<CallInst *>::iterator fit;
  for(fit = calls.begin(); fit != calls.end(); ++fit) {
      markCall(*fit);
      modified = true;
  }

  return modified;
}
