#include <llvm/Function.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "SelectRemovalPass.h"
#include <iostream>

using namespace llvm;

char SelectRemovalPass::ID = 0;
RegisterPass<SelectRemovalPass> 
  X("selectremoval", "Converts Select instructions to if/then/else",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

#define foreach(_i, _b, _e) \
	  for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)




static PHINode* GenIf(Function *f, Value *trueV, Value *falseV,
  BasicBlock *entry, BasicBlock *fallback,
  Value *condition)
{
  BasicBlock* trueBb = BasicBlock::Create(f->getContext(), "", f);
  BranchInst::Create(fallback, trueBb);

  BasicBlock* falseBb = BasicBlock::Create(f->getContext(), "", f);
  BranchInst::Create(fallback, falseBb);

  
  entry->back().eraseFromParent();
  BranchInst::Create(trueBb, falseBb, condition, entry);

  PHINode *phi = PHINode::Create(trueV->getType(), "");
  phi->addIncoming(trueV, trueBb);
  phi->addIncoming(falseV, falseBb);

  return phi;
}

bool SelectRemovalPass::runOnFunction(Function &F) 
{
 // std::cout << "SelectRemovalPass: " << F.getName() << std::endl;
  bool modified=false;
  
  foreach(bbit, F.begin(), F.end()) {
   again:
   foreach(iit, bbit->begin(), bbit->end()) {
      Instruction &i = *iit;
      if (i.getOpcode() != Instruction::Select)
        continue;

      Value *condition = iit->getOperand(0);
      Value *trueV = iit->getOperand(1);
      Value *falseV = iit->getOperand(2);
  
      BasicBlock *entry = bbit;
      BasicBlock *fallback = bbit->splitBasicBlock(iit, "");
      PHINode *phi = GenIf(&F, trueV, falseV, entry, fallback, condition);
      //std::cout << phi->getParent();
      Instruction *instToReplace = fallback->begin();
      BasicBlock::iterator ii(instToReplace);
      ReplaceInstWithInst(instToReplace->getParent()->getInstList(),
      ii, phi);

      bbit = fallback;
      modified = true;
      goto again;
    }
  }
 
  return modified;
}
