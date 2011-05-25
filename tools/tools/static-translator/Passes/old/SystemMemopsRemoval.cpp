extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/AliasSetTracker.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "SystemMemopsRemoval.h"
#include "lib/Utils/Utils.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>
#include <stack>

//#define _DEBUG_CUTTER

using namespace llvm;

char SystemMemopsRemoval::ID = 0;
RegisterPass<SystemMemopsRemoval>
  SystemMemopsRemoval("SystemMemopsRemoval", "Remove instructions that access outside CPU's gp registers",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);



void SystemMemopsRemoval::getAnalysisUsage(AnalysisUsage &AU) const
{
    //AU.addRequired<AliasAnalysisEvaluator>();
}

bool SystemMemopsRemoval::getEnvOffsetFromLoadStore(Instruction *instr, uint64_t &offset)
{
    std::stack<Instruction *> tovisit;
    tovisit.push(instr);

    while(!tovisit.empty()) {
        Instruction *ci = tovisit.top();
        tovisit.pop();

        //std::cout << "EXPL " << *ci << std::endl;

        if (ci->getOpcode() == Instruction::Add) {
            //We found the place where the offset is added
            ConstantInt *cste = dyn_cast<ConstantInt>(ci->getOperand(0));
            if (!cste) {
                cste = dyn_cast<ConstantInt>(ci->getOperand(1));
                assert(cste && "Generated code is broken. We expect the add to add a constant offset.");
            }
            offset = *cste->getValue().getRawData();
            return true;
        }
        for (unsigned i=0; i<ci->getNumOperands(); ++i) {
            Instruction *ni = dyn_cast<Instruction>(ci->getOperand(i));
            //XXX: This TOTALLY UNRELIABLE. Implement proper alias analysis....
            if (!ni || dyn_cast<LoadInst>(ni) || dyn_cast<CallInst>(ni)) {
                continue;
            }
            tovisit.push(ni);
        }
    }
    return false;
}

bool SystemMemopsRemoval::getEnvOffset(Instruction *instr, uint64_t &offset)
{
    LoadInst *li = dyn_cast<LoadInst>(instr);

    if (li) {
        //std::cout << "-----" << std::endl;
        if (!getEnvOffsetFromLoadStore(li, offset)) {
            return false;
        }
        //std::cout << "LD OFFSET: 0x" << std::hex << offset << std::endl;
        return true;
    }

    StoreInst *si = dyn_cast<StoreInst>(instr);
    if (si) {
        //std::cout << "-----" << std::endl;
        if (!getEnvOffsetFromLoadStore(si, offset)) {
            return false;
        }
        //std::cout << "ST OFFSET: 0x" << std::hex << offset << std::endl;
        return true;
    }
    return false;
}

bool SystemMemopsRemoval::isHardCodedAddress(Instruction *instr) {
    StoreInst *si = dyn_cast<StoreInst>(instr);
    if (!si) {
        return false;
    }

    Constant *pi = dyn_cast<Constant>(si->getPointerOperand());
    if (pi) {
        return true;
    }
    return false;
}

bool SystemMemopsRemoval::runOnFunction(llvm::Function &F)
{
    bool res = false;
    std::set<Instruction *> toDelete;

    foreach(iit, inst_begin(F), inst_end(F)) {
        uint64_t offset = 0;
        if (getEnvOffset(&*iit, offset)) {
            if (offset >= offsetof(CPUState, eip)) {
                //std::cout << "Deleting offset " << *iit << std::endl;
                toDelete.insert(&*iit);
            }
        }else if(isHardCodedAddress(&*iit)) {
            //std::cout << "Deleting hard-coded " << *iit << std::endl;
            toDelete.insert(&*iit);
        }

    }

    //Delete all the instructions that access other places than
    //the gp registers

    //First, deal with stores, that don't have uses
    foreach(iit, toDelete.begin(), toDelete.end()) {
        StoreInst *si = dyn_cast<StoreInst>(*iit);
        if (si) {
            si->eraseFromParent();
            continue;
        }

        LoadInst *li = dyn_cast<LoadInst>(*iit);
        assert(li);

        std::stack<Instruction*> toDelete;
        toDelete.push(li);
        while(!toDelete.empty()) {
            Instruction *top = toDelete.top();
            toDelete.pop();
            for (unsigned i=0; i<top->getNumOperands(); ++i) {
                if (Instruction *ni = dyn_cast<Instruction>(top->getOperand(i))) {
                    toDelete.push(ni);
                }
            }
            if (top->getNumUses() == 0) {
                top->eraseFromParent();
            }
        }
    }

    //std::cout << F;

    return res;
}
