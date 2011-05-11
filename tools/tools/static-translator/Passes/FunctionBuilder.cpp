#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/ModuleProvider.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/target/TargetData.h>

#include "lib/Utils/Log.h"
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

std::string FunctionBuilder::TAG = "FunctionBuilder";

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

    ReturnInst::Create(M.getContext(), ConstantInt::get(M.getContext(), APInt(64,  0)), BB);

    return F;
}

//calledBbs maps the address of a basic block to the call site
void FunctionBuilder::getCalledBbsAndInstructionBoundaries(
        Module &M, Function *f,
        Instructions &boundaries,
        MultiInstructions &calledBbs
     )
{

    Function *instrMarker = M.getFunction("instruction_marker");

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
            if (!(boundaries.find(pc) == boundaries.end())) {
                LOGERROR() << "Duplicate pc: 0x" << std::hex << pc << " in bb " << ci->getParent()->getNameStr() << std::endl;
                assert(false);
            }
            boundaries.insert(std::make_pair(pc, ci));
        }else {
            FunctionAddressMap::iterator fcnAddr = m_basicBlocks.find(ci->getCalledFunction());
            if (fcnAddr != m_basicBlocks.end()) {
                uint64_t pc = (*fcnAddr).second;
                if (!(calledBbs.find(pc) == calledBbs.end())) {
                    //A program counter may appear twice if there are two places that try
                    //to jump to a block that was not yet inlined.
                    LOGERROR() << "Duplicate target pc: 0x" << std::hex << pc << std::endl;
                }
                calledBbs.insert(std::make_pair(pc, ci));
            }
        }
    }
}

void FunctionBuilder::patchJumps(Instructions &boundaries,
                MultiInstructions &branchTargets)
{
    foreach(it, branchTargets.begin(), branchTargets.end()) {
        CallInst *ci = dyn_cast<CallInst>((*it).second);
        uintptr_t calledPc = (*it).first;
        if (boundaries.find(calledPc) == boundaries.end()) {
            //The called basic block was not inlined yet
            continue;
        }

        //Create the branch target
        //XXX: May create redundant chains of branches

        Instruction *splitTargetAt = boundaries[calledPc];
        BasicBlock *newBB = splitTargetAt->getParent()->splitBasicBlock(splitTargetAt);

#if 0
        std::cout << "Ci=" << *ci << std::endl;
        std::cout << "Splitting at " << *splitTargetAt << std::endl;
        std::cout << "New BB is " << *newBB;
#endif

        //Discard all instructions following the call
        BasicBlock *callBB = ci->getParent();
        while(&callBB->back() != ci) {
      //      std::cout << "Erasing " << callBB->back() << std::endl;
            callBB->back().eraseFromParent();
        }

        //Replace the call with a branch
        ci->eraseFromParent();
        BranchInst *bi = BranchInst::Create(newBB, callBB);
        //std::cout << "Created internal branch " << *bi << std::endl;
    }
}



bool FunctionBuilder::runOnModule(llvm::Module &M)
{
    bool modified = false;
    Function *F = createFunction(M);
    m_function = F;

    ForcedInliner inliner;

    inliner.addFunction(F);
    inliner.setPrefix("tcg-llvm-tb-");
    inliner.setInlineOnceEachFunction(true);

    std::map<uintptr_t, Instruction*> programCounters;
    std::set<CallInst*> CalledBBs;

    ExistingModuleProvider *MP = new ExistingModuleProvider(m_function->getParent());
    TargetData *TD = new TargetData(m_function->getParent());
    FunctionPassManager FcnPasses(MP);
    FcnPasses.add(TD);

    FcnPasses.add(createVerifierPass());
   /* FcnPasses.add(createCFGSimplificationPass());
    FcnPasses.add(createDeadStoreEliminationPass());
    FcnPasses.add(createGVNPass());
    FcnPasses.add(createAggressiveDCEPass());
*/


    ///XXX: This is O(n^2), as we rescan all instructions at
    //at each iteration. Find a better way of keeping track of new
    //call instructions and markers.
    do {
        inliner.inlineFunctions();
        //std::cout << *F << std::flush;

        Instructions boundaries;
        MultiInstructions branchTargets;
        getCalledBbsAndInstructionBoundaries(M, F, boundaries, branchTargets);

        //Nothing more to inline, done with construction
        if (branchTargets.size() == 0) {
            break;
        }

        patchJumps(boundaries, branchTargets);

        modified = true;
    }while(true);

    LOGDEBUG() << *F << std::flush;
    FcnPasses.run(*F);

    return modified;
}
