#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "QEMUTerminatorMarker.h"
#include "Utils.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>


using namespace llvm;

char QEMUTerminatorMarker::ID = 0;
RegisterPass<QEMUTerminatorMarker>
  QEMUTerminatorMarker("QEMUTerminatorMarker", "Inserts a terminator marker at the end of the block",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

void QEMUTerminatorMarker::initMarkers(llvm::Module *module)
{
    if (m_callMarker || m_returnMarker) {
        return;
    }

    FunctionType *type;
    std::vector<const Type *> paramTypes;

    //1. Deal with call markers
    //First parameter is a pointer
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));
    //Second parameter: 0=not inlinable, 1=inlinable
    paramTypes.push_back(Type::getInt1Ty(module->getContext()));
    //Third parameter: pointer to the CPU state, for indirect calls
    paramTypes.push_back(PointerType::getUnqual(Type::getInt64Ty(module->getContext())));

    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_callMarker = dyn_cast<Function>(module->getOrInsertFunction("call_marker", type));

    std::cout << *m_callMarker << std::endl;

    //2. Handle the return marker
    paramTypes.clear();
    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_returnMarker = dyn_cast<Function>(module->getOrInsertFunction("return_marker", type));
}

/**
 * XXX: also extract info about indirect calls
 */
void QEMUTerminatorMarker::markCall(CallInst *Ci)
{
  assert(!m_return);

  Module *module = Ci->getParent()->getParent()->getParent();
  Value *programCounter = Ci->getOperand(1);

  if (ConstantInt *cste = dyn_cast<ConstantInt>(programCounter)) {
    const uint64_t* target = cste->getValue().getRawData();
    m_staticTargets.insert(*target);

    //Take into account the case where the call is used to get
    //the current program counter. The program calls the next instruction, and pops the return address
    if (*target == m_successor) {
        m_call = false;
        m_inlinable = true;
    }
  }

  //It is impossible to inline a call if it is indirect
  //XXX: figure out indirect branches/switches in another pass?
  assert(isa<ConstantInt>(programCounter) || !m_inlinable);

  std::vector<Value*> CallArguments;
  CallArguments.push_back(programCounter);
  CallArguments.push_back(m_inlinable ? ConstantInt::getTrue(module->getContext()) : ConstantInt::getFalse(module->getContext()));
  CallArguments.push_back(&*Ci->getParent()->getParent()->arg_begin());
  CallInst *marker = CallInst::Create(m_callMarker, CallArguments.begin(), CallArguments.end());

  //We must insert the call right before the terminator, to avoid messing up use/defs
  //during inlining. All defs should dominate the uses, and if we don't put the call at the end
  //we'll get into trouble.
  marker->insertBefore(Ci->getParent()->getTerminator());

  //If we have a block with a call instruction, insert an inlinable call to the successor, i.e.,
  //put a "branch" (call marker) to the following basic block.
  if (m_call) {
      CallArguments.clear();
      CallArguments.push_back(ConstantInt::get(module->getContext(), APInt(64,  m_successor)));
      CallArguments.push_back(ConstantInt::getTrue(module->getContext()));
      CallArguments.push_back(&*Ci->getParent()->getParent()->arg_begin());
      CallInst *marker = CallInst::Create(m_callMarker, CallArguments.begin(), CallArguments.end());
      marker->insertBefore(Ci->getParent()->getTerminator());
  }

  Ci->replaceAllUsesWith(programCounter);
  Ci->eraseFromParent();
}

void QEMUTerminatorMarker::markReturn(CallInst *Ci)
{
    assert(m_return);

    Value *programCounter = Ci->getOperand(1);
    assert(!isa<ConstantInt>(programCounter));

    std::vector<Value*> CallArguments;
    CallInst *marker = CallInst::Create(m_returnMarker, CallArguments.begin(), CallArguments.end());
    marker->insertBefore(Ci);

    Ci->replaceAllUsesWith(programCounter);
    Ci->eraseFromParent();
}

void QEMUTerminatorMarker::findReturnInstructions(Function &F, Instructions &Result)
{
  foreach(bbit, F.begin(), F.end()) {
      BasicBlock &bb = *bbit;
      TerminatorInst  *term = bb.getTerminator();
      if (term->getOpcode() == Instruction::Ret) {
          Result.push_back(term);
      }
  }
}

bool QEMUTerminatorMarker::runOnFunction(llvm::Function &F)
{
    bool modified = false;
    initMarkers(F.getParent());

    Function *forkAndConcretize = F.getParent()->getFunction("tcg_llvm_fork_and_concretize");
    Function *helper_hlt = F.getParent()->getFunction("helper_hlt");

    //Step 1: find all basic blocks with a return instruction in them
    Instructions returnInstructions;
    findReturnInstructions(F, returnInstructions);
    assert(!m_return || returnInstructions.size() == 1);

    BasicBlocks returnBlocks;
    foreach(rbit, returnInstructions.begin(), returnInstructions.end()) {
        returnBlocks.push_back((*rbit)->getParent());
    }


    //Step 2: iterate backwards over the blocks, and mark the
    //last assignment to the program counter
    foreach(bbit, returnBlocks.begin(), returnBlocks.end()) {

        BasicBlock *bb = *bbit;

        ilist<Instruction>::reverse_iterator riit(bb->end());
        ilist<Instruction>::reverse_iterator riitEnd(bb->begin());


        for(; riit != riitEnd; ++riit) {
            Instruction *I = &*riit;

            CallInst *Ci = dyn_cast<CallInst>(I);
            if (!Ci || !Ci->getCalledFunction()) {
                continue;
            }

            if (Ci->getCalledFunction() == helper_hlt) {
                //XXX: use llvm::Function.setDoesNotReturn?
                m_doesNotReturn = true;
                continue;
            }

            bool marked =  Ci->getCalledFunction() == forkAndConcretize;

            if (marked) {
                if (m_return) {
                    markReturn(Ci);
                }else {
                    markCall(Ci);
                }
                modified = true;
                break;
            }
        }
    }

    return modified;
}
