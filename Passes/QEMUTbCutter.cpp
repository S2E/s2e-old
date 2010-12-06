#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "QEMUTbCutter.h"
#include "Utils.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>

//#define _DEBUG_CUTTER

using namespace llvm;

char QEMUTbCutter::ID = 0;
RegisterPass<QEMUTbCutter>
  QEMUTbCutter("qemutbcutter", "Removes instruction ranges from the translation block",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);


bool QEMUTbCutter::isInstructionMarker(CallInst *Ci)
{
    if (!Ci) {
        return false;
    }

    return Ci->getCalledFunction() == m_instructionMarker;
}

//ilist<Instruction>::reverse_iterator iit(finish);
//ilist<Instruction>::reverse_iterator iitEnd(firstInstr);

bool QEMUTbCutter::removeBefore(llvm::CallInst *finish)
{
    Function *f = finish->getParent()->getParent();

    inst_iterator firstInstrIt = inst_end(f);
    inst_iterator lastInstrIt = inst_end(f);

    BasicBlock *firstInstrBb, *lastInstrBb;
    std::vector<BasicBlock*> bbToDelete;

    //Locate the first machine instruction
    inst_iterator iit = inst_begin(f);
    inst_iterator iitEnd = inst_end(f);

    bool lookLast = false;
    while(iit != iitEnd) {
        //Look for the first marker
        if (!lookLast) {
            if (isInstructionMarker(dyn_cast<CallInst>(&*iit))) {
                firstInstrIt = iit;
                firstInstrBb = (*iit).getParent();
                lookLast = true;
            }
        }else {
            if (&*iit == finish) {
                lastInstrIt = iit;
                lastInstrBb = (*iit).getParent();
                break;
            }
        }
        ++iit;
    }
    if (iit == iitEnd) {
        return false;
    }

    //Get the list of basic blocks to eventually delete.
    //They must be empty at the end of the process
    ilist<BasicBlock>::iterator bbit(firstInstrBb);
    ilist<BasicBlock>::iterator bbitEnd(lastInstrBb);

    while(bbit != bbitEnd) {
        if (&*bbit != firstInstrBb) {
            bbToDelete.push_back(&*bbit);
        }
        ++bbit;
    }

    //Start from the instruction before finish and delete
    //everything up to and including firstInstr
    iit = lastInstrIt;
    iitEnd = firstInstrIt;

    //Do not delete the end marker, delete everything that is strictly before it
    --iit;
    --iitEnd;

    std::vector<Instruction*> toDelete;

    while (iit != iitEnd) {
        toDelete.push_back(&*iit);
        //std::cerr << "Deleting " << *iit << std::endl;
        --iit;
    }

    //Delete the instructions for real
    foreach(it, toDelete.begin(), toDelete.end()) {
        Instruction *instr = *it;
        //Some LLVM instructions may be used across machine boundaries.
        //Keep those.
        if (instr->getNumUses() == 0) {
            instr->eraseFromParent();
        }
    }

    //Delete the empty basic blocks
    foreach(it, bbToDelete.begin(), bbToDelete.end()) {
        assert((*it)->size() == 0);
        (*it)->eraseFromParent();
    }

    //Join first and last basic block, if needed
    if (lastInstrBb != firstInstrBb) {
        BranchInst::Create(lastInstrBb, firstInstrBb);
    }

    return true;
}


//Assume BBs are sorted in topological order
bool QEMUTbCutter::removeAfter(llvm::CallInst *start)
{
    Function *f = start->getParent()->getParent();
    bool stopped = false;
    std::set<BasicBlock*> visitedBlocks;
    std::vector<Instruction *> toDelete;

    ilist<BasicBlock>::reverse_iterator bbit(f->end());
    ilist<BasicBlock>::reverse_iterator bbitEnd(f->begin());


    //Delete up to start, starting from the end,
    //because we must not break use-def chains.
    while(bbit != bbitEnd) {
        BasicBlock *bb = &*bbit;
        ilist<Instruction>::reverse_iterator iit(bb->end());
        ilist<Instruction>::reverse_iterator iitEnd(bb->begin());

        while(iit != iitEnd) {
            if (&*iit == start) {
                stopped = true;
            }

            visitedBlocks.insert(bb);
            toDelete.push_back(&*iit);
            //std::cerr << "Deleting " << *iit << std::endl;
            ++iit;

            if (stopped) {
                break;
            }
        }
        if (stopped) {
            break;
        }
        ++bbit;
    }

    foreach(it, toDelete.begin(), toDelete.end()) {
        (*it)->eraseFromParent();
    }

    if (visitedBlocks.empty()) {
        //This must not happen
        return false;
    }

    //Remove completely empty basic blocks
    foreach(it, visitedBlocks.begin(), visitedBlocks.end()) {
        if ((*it)->size() == 0) {
            (*it)->eraseFromParent();
        }
    }

#if 0
    //XXX: Check this
    //We must have only one basic block left, by construction
    //of the generated code
    if (f->size() != 1) {
        std::cerr << *f;
        assert(false);
    }
#endif

    //Insert a return instruction
    ilist<BasicBlock>::reverse_iterator lastBb(f->end());

    LLVMContext &ctx = f->getParent()->getContext();
    Value *revVal = ConstantInt::get(ctx, APInt(64,  0));
    ReturnInst::Create(ctx, revVal, &*lastBb);

    return true;
}

void QEMUTbCutter::insertUnconditionalBranch(CallInst *ci)
{
    //Check that we have an instruction marker
    Function *cf = ci->getCalledFunction();
    assert(cf);
    const char *Fn = cf->getNameStr().c_str();
    assert(strstr(Fn, "instruction_marker"));

    //Get the instruction pointer from the instruction marker
    Value *programCounter = ci->getOperand(1);

    //Inject the call marker right before the instruction marker
    Function *callMarker = cf->getParent()->getFunction("call_marker");
    assert(callMarker && "You must run QEMUTerminatorMarkerPass first");

    std::vector<Value*> CallArguments;
    CallArguments.push_back(programCounter);
    CallArguments.push_back(ConstantInt::getTrue(cf->getParent()->getContext()));
    CallInst *marker = CallInst::Create(callMarker, CallArguments.begin(), CallArguments.end());
    marker->insertBefore(ci);
}

bool QEMUTbCutter::runOnFunction(llvm::Function &F)
{
    bool res;
#ifdef _DEBUG_CUTTER
    std::cerr << "------------" << std::endl;
    std::cerr << "Before" << std::endl;
    std::cerr << F;
    std::cerr << "Marker: " << *m_marker << std::endl;
    std::cerr << "Cut before: " << m_cutBefore << std::endl;
#endif

    assert(&F == m_marker->getParent()->getParent());

    m_instructionMarker = F.getParent()->getFunction("instruction_marker");

    if (m_cutBefore) {
        res = removeBefore(m_marker);
    }else {
        //Insert and unconditional branch call marker to the next BB.
        insertUnconditionalBranch(m_marker);
        res = removeAfter(m_marker);
        if (!res) {
            return res;
        }
    }

#ifdef _DEBUG_CUTTER
    std::cerr << "After" << std::endl;
    std::cerr << F;
#endif

    return res;
}
