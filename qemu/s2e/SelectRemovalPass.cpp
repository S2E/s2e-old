/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

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

  PHINode *phi = PHINode::Create(trueV->getType(), 0, "");
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
