#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Support/InstIterator.h>


#include <Utils.h>>
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
  ConstantInt *programCounter = dyn_cast<ConstantInt>(Ci->getOperand(1));

  //The program counter must be a constant!
  if (!programCounter) {
      return;
  }

  const uint64_t* target = programCounter->getValue().getRawData();
  if(m_markers.find(*target) == m_markers.end())
  {
      //Some instructions generate multiple updates to the program counter
      //(e.g., pop ss). Make sure to keep only the first one.
      //XXX: We really should have specific markers for instruction start in the code generator
      std::vector<Value*> CallArguments;
      CallArguments.push_back(programCounter);
      CallInst *marker = CallInst::Create(m_instructionMarker, CallArguments.begin(), CallArguments.end());
      marker->insertBefore(Ci);
      m_markers[*target] = marker;
  }

  //In all cases, delete the original fork instruction
  Ci->replaceAllUsesWith(programCounter);
  Ci->eraseFromParent();

}

void QEMUInstructionBoundaryMarker::updateMarker(llvm::CallInst *Ci)
{
    ConstantInt *programCounter = dyn_cast<ConstantInt>(Ci->getOperand(1));
    assert(programCounter);

    const uint64_t* target = programCounter->getValue().getRawData();

    assert(m_markers.find(*target) == m_markers.end());
    m_markers[*target] = Ci;
}

/**
 *  Moves the alloca instruction in the prologue of the TB closer
 *  to its use site, i.e., right after the instruction marker.
 */
void QEMUInstructionBoundaryMarker::moveAllocInstruction(llvm::AllocaInst *instr)
{    
    Function *containingFunction = instr->getParent()->getParent();
    Function *instructionMarker = containingFunction->getParent()->getFunction("instruction_marker");
    std::vector<User*> users;
    users.insert(users.begin(), instr->use_begin(), instr->use_end());
    assert(users.size() > 0);

    Instruction *movedRightAfter = NULL;

    std::cerr << "ALLOCA: " << *instr << std::endl;

    foreach(useit, users.begin(), users.end()) {
        Instruction *target = dyn_cast<Instruction>(*useit);

        std::cerr << "USER: " << *target << std::endl;

        Instruction *firstInst = &*(*instr->getParent()->getParent()->begin()).begin();
        //Get the instruction boundary

        ilist<BasicBlock>::iterator iCurBb(*target->getParent());
        ilist<BasicBlock>::iterator iFirstBb(*target->getParent()->getParent()->begin());

        ilist<Instruction>::iterator iStartOfCurBb(*target->getParent()->begin());
        ilist<Instruction>::iterator iCurInstr(target);
        ilist<Instruction>::iterator iFirstFcnInstr(firstInst);


        while(iCurInstr != iFirstFcnInstr) {
            if (iCurInstr == iStartOfCurBb) {
                //Go to the previous basic block
                --iCurBb;
                iStartOfCurBb = ilist<Instruction>::iterator(*(*iCurBb).begin());
                iCurInstr = ilist<Instruction>::iterator((*iCurBb).back());
                --iCurInstr;

            }

            CallInst *ci = dyn_cast<CallInst>(&*iCurInstr);
            if (!ci || ci->getCalledFunction() != instructionMarker) {
                --iCurInstr;
                continue;
            }

            if (movedRightAfter) {
                //Check that the new use is the same instruction
                assert(movedRightAfter == ci && "The alloca is used across multiple instructions...");
                break;
            }

            movedRightAfter = ci;
            std::cerr << " MOVING AFTER " << *ci << std::endl;
            //Move the alloc right after
            ++iCurInstr;
            std::cerr << "        BEFORE " << *iCurInstr << std::endl;
            instr->moveBefore(&*iCurInstr);
            break;
        }
     }
}

/**
 *  This function renders each machine instruction independent from the others,
 *  by making sure that it only uses definitions that are inside its boundary.
 */
bool QEMUInstructionBoundaryMarker::duplicatePrefixInstructions(llvm::Function &F)
{
    Function *instructionMarker = F.getParent()->getFunction("instruction_marker");
    assert(instructionMarker);

    //First erase any useless instruction that appears before
    //the first machine instruction
    inst_iterator iit = inst_begin(F);
    inst_iterator iitEnd = inst_end(F);

    std::set<Instruction*> prologueInstructionSet;
    std::vector<Instruction*>prologueInstructionVector;

    while(iit != iitEnd) {
        CallInst *ci = dyn_cast<CallInst>(&*iit);
        if (ci && ci->getCalledFunction() == instructionMarker) {
            break;
        }
        prologueInstructionSet.insert(&*iit);
        prologueInstructionVector.push_back(&*iit);
        ++iit;
    }

    //Now copy these instructions next to their uses down
    foreach(iit, prologueInstructionVector.rbegin(), prologueInstructionVector.rend()) {
        Instruction *instr = *iit;
        User::use_iterator useit = instr->use_begin();

        //alloca instructions cannot be duplicated. Move them right before their first use.
        if (dyn_cast<AllocaInst>(instr)) {
            moveAllocInstruction(dyn_cast<AllocaInst>(instr));
            continue;
        }
        
        std::vector<User*> users;
        users.insert(users.begin(), instr->use_begin(), instr->use_end());
        foreach(useit, users.begin(), users.end()) {
            //Copy instruction right before *instr.
            Instruction *target = dyn_cast<Instruction>(*useit);
            if (prologueInstructionSet.find(target) != prologueInstructionSet.end()) {
                continue;
            }
            assert(target);
            Instruction *newInstr = instr->clone(F.getParent()->getContext());
            newInstr->insertBefore(target);
            for (unsigned i=0; i<target->getNumOperands(); ++i) {
                if (target->getOperand(i) == instr) {
                    target->setOperand(i, newInstr);
                }
            }
        }
    }


    return true;
}

bool QEMUInstructionBoundaryMarker::runOnFunction(llvm::Function &F)
{
  bool modified = false;
  std::vector<CallInst *> Forks;

  m_markers.clear();

  initInstructionMarker(F.getParent());

  Function *forkAndConcretize = F.getParent()->getFunction("tcg_llvm_fork_and_concretize");
  Function *instructionMarker = F.getParent()->getFunction("instruction_marker");

  Function::iterator bbit;
  for(bbit=F.begin(); bbit != F.end(); ++bbit) {
    BasicBlock *bb = &*bbit;

    BasicBlock::iterator iit;
    for(iit = bb->begin(); iit != bb->end(); ++iit) {
      Instruction *I = &*iit;
    
      CallInst *Ci = dyn_cast<CallInst>(I);
      Function *F;
      if (!Ci || !(F = Ci->getCalledFunction())) {
        continue;
      }

      if ((F == forkAndConcretize) && !m_analyze) {
        Forks.push_back(Ci);
      }

      if ((F == instructionMarker) && m_analyze) {
        updateMarker(Ci);
      }
    }    
  }

  if (!m_analyze) {
    std::vector<CallInst *>::iterator fit;
    for(fit = Forks.begin(); fit != Forks.end(); ++fit) {
      markBoundary(*fit);
      modified = true;
    }
  }

  duplicatePrefixInstructions(F);

  std::cout << "Marked" << F << std::endl;

  return modified;
}
