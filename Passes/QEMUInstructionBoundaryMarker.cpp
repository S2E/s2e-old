#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "QEMUInstructionBoundaryMarker.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>


using namespace llvm;

char QEMUInstructionBoundaryMarker::ID = 0;
RegisterPass<QEMUInstructionBoundaryMarker>
  QEMUInstructionBoundaryMarker("qemuinstructionmarker", "Marks instruction boundaries",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

void QEMUInstructionBoundaryMarker::initInstructionMarker(llvm::Module *module)
{
    if (m_instructionMarker) {
        return;
    }

    FunctionType *type;
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));

    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_instructionMarker = dyn_cast<Function>(module->getOrInsertFunction("instruction_marker", type));
}

void QEMUInstructionBoundaryMarker::markBoundary(CallInst *Ci)
{
  Value *programCounter = Ci->getOperand(1);

  //The program counter must be a constant!
  if (!isa<ConstantInt>(programCounter)) {
      return;
  }

  std::vector<Value*> CallArguments;
  CallArguments.push_back(programCounter);
  CallInst *marker = CallInst::Create(m_instructionMarker, CallArguments.begin(), CallArguments.end());
  marker->insertBefore(Ci);

  Ci->replaceAllUsesWith(programCounter);
  Ci->eraseFromParent();
}

bool QEMUInstructionBoundaryMarker::runOnFunction(llvm::Function &F)
{
  bool modified = false;
  std::vector<CallInst *> Forks;

  initInstructionMarker(F.getParent());

  Function::iterator bbit;
  for(bbit=F.begin(); bbit != F.end(); ++bbit) {
    BasicBlock *bb = &*bbit;

    BasicBlock::iterator iit;
    for(iit = bb->begin(); iit != bb->end(); ++iit) {
      Instruction *I = &*iit;
    
      CallInst *Ci = dyn_cast<CallInst>(I);
      if (!Ci) {
        continue;
      }

      Function *F = Ci->getCalledFunction();
      if (!F) {
        continue;
      }
      const char *Fn = F->getNameStr().c_str();
      bool IsAFork = strstr(Fn, "tcg_llvm_fork_and_concretize") != NULL;

      if (IsAFork) {
        Forks.push_back(Ci);
      }
    }
    
  }

  std::vector<CallInst *>::iterator fit;
  for(fit = Forks.begin(); fit != Forks.end(); ++fit) {
      markBoundary(*fit);
      modified = true;
  } 

  return modified;
}
