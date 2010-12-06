#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "FunctionBuilder.h"
#include "ForcedInliner.h"
#include "Utils.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>

using namespace llvm;

char FunctionBuilder::ID = 0;
RegisterPass<FunctionBuilder>
  FunctionBuilder("FunctionBuilder", "Transforms merges independent basic block functions into LLVM functions",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

Function *FunctionBuilder::createFunction(Module &M)
{
    const FunctionType *fcnType = m_entryPoint->getFunctionType();
    M.getOrInsertFunction(m_functionName, fcnType);
    Function *F = M.getFunction(m_functionName);

    //Setup the function arguments
    //First, we only have the pointer to the CPU environment
    Function::ArgumentListType &args = F->getArgumentList();
    assert(args.size() == 1);

    std::vector<Value*> CallArguments(1);
    CallArguments[0] = &*args.begin();

    BasicBlock *BB = BasicBlock::Create(M.getContext(), "", F, NULL);

    //The initial function calls the entry basic block
    CallInst::Create(m_entryPoint, CallArguments.begin(), CallArguments.end(), "", BB);

    ReturnInst::Create(M.getContext(), ConstantInt::get(M.getContext(), APInt(32,  0)), BB);

    return F;
}

void FunctionBuilder::getCalledBbsAndInstructionBoundaries(
        Module &M, Function *f,
        Instructions &boundaries,
        Instructions &calledBbs
     )
{

    Function *instrMarker = M.getFunction("instructionMarker");
    Function *callMarker = M.getFunction("callMarker");

    foreach(iit, inst_begin(f), inst_end(f)) {
        CallInst *ci = dyn_cast<CallInst>(&*iit);
        if (!ci) {
            continue;
        }

        if (ci->getCalledFunction() == instrMarker) {
            //Get the program counter
            ConstantInt *cste = dyn_cast<ConstantInt>(ci->getOperand(1));
            assert(cste);
            uint64_t pc = *cste->getValue().getRawData();
            assert(boundaries.find(pc) == boundaries.end());
            boundaries[pc] = ci;
        }else if (ci->getCalledFunction() == callMarker) {
            //Extract direct jumps
            ConstantInt *cste = dyn_cast<ConstantInt>(ci->getOperand(1));
            ConstantInt *isInlinable = dyn_cast<ConstantInt>(ci->getOperand(2));
            assert(cste && isInlinable);
            if (isInlinable == ConstantInt::getTrue(M.getContext())) {
                uint64_t pc = *cste->getValue().getRawData();
                assert(calledBbs.find(pc) == calledBbs.end());
                calledBbs[pc] = ci;
            }
        }
    }
}

void FunctionBuilder::patchJumps(Instructions &boundaries,
                Instructions &branchTargets)
{
    foreach(it, branchTargets.begin(), branchTargets.end()) {
        CallInst *ci = dyn_cast<CallInst>((*it).second);
        uintptr_t calledPc = (*it).first;
        if (boundaries.find(calledPc) == boundaries.end()) {
            //The called basic block was not inlined yet
            continue;
        }

        //Create the branch target

        std::cout << "Ci=" << *ci;
        Instruction *splitTargetAt = boundaries[calledPc];
        std::cout << "Splitting at " << *splitTargetAt;
        BasicBlock *newBB = splitTargetAt->getParent()->splitBasicBlock(splitTargetAt);
        std::cout << "New BB is " << *newBB;

        //Discard all instructions following the call
        BasicBlock *callBB = ci->getParent();
        while(&callBB->back() != ci) {
            std::cout << "Erasing " << callBB->back();
            callBB->back().eraseFromParent();
        }

        //Replace the call with a branch
        ci->eraseFromParent();
        BranchInst *bi = BranchInst::Create(newBB, callBB);
        std::cout << "Created internal branch " << *bi;
    }
}

bool FunctionBuilder::runOnModule(llvm::Module &M)
{
    bool modified = false;
    Function *F = createFunction(M);

    ForcedInliner inliner;

    inliner.addFunction(F);
    std::map<uintptr_t, Instruction*> programCounters;
    std::set<CallInst*> CalledBBs;

    ///XXX: This is O(n^2), as we rescan all instructions at
    //at each iteration. Find a better way of keeping track of new
    //call instructions and markers.
    do {
        inliner.inlineFunctions();

        Instructions boundaries;
        Instructions branchTargets;
        getCalledBbsAndInstructionBoundaries(M, F, boundaries, branchTargets);

        //Nothing more to inline, done with construction
        if (branchTargets.size() == 0) {
            break;
        }

        patchJumps(boundaries, branchTargets);
        modified = true;
    }while(true);

    return modified;
}
