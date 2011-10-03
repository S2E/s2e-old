extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

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
#include <llvm/Target/TargetData.h>

#include "lib/Utils/Log.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "lib/X86Translator/ForcedInliner.h"
#include "lib/Utils/Utils.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>
#include <sstream>

#include "FunctionBuilder.h"

using namespace llvm;

namespace s2etools {

char FunctionBuilder::ID = 0;
#if 0
RegisterPass<FunctionBuilder>
  FunctionBuilder("FunctionBuilder", "Merges independent basic block functions into LLVM functions",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);
#endif

LogKey FunctionBuilder::TAG = LogKey("FunctionBuilder");

void FunctionBuilder::computeBasicBlockBoundaries()
{
    m_basicBlockBoundaries.insert(m_entryPoint);

    foreach(it, m_functionInstructions.begin(), m_functionInstructions.end()) {
        TranslatedBlocksMap::const_iterator tbit = m_allInstructions.find(*it);
        assert(tbit != m_allInstructions.end() && "Incomplete list of basic blocks");
        TranslatedBlock *tb = (*tbit).second;

        ConstantInt *fb = NULL, *dest = NULL;

        switch(tb->getType()) {
            //Normal instruction, no basic blocks to create
            case BB_DEFAULT:
                break;

            //Fallback and destination are in the same function,
            //add their targets to the list of basic block starts
            case BB_JMP:
                dest = dyn_cast<ConstantInt>(tb->getDestination());
                break;

            //XXX: we should resolve statically all possible targets
            //here
            case BB_JMP_IND:
                LOGERROR("RevGen does not support indirect branches yet..." << std::endl);
                //dest = dyn_cast<ConstantInt>(tb->getDestination());
                break;

            case BB_COND_JMP:
            case BB_REP:
                dest = dyn_cast<ConstantInt>(tb->getDestination());
                fb = dyn_cast<ConstantInt>(tb->getFallback());
                break;

            case BB_COND_JMP_IND:
                fb = dyn_cast<ConstantInt>(tb->getFallback());
                break;
            //Don't need to create a basic block for the rest
            default:
                break;
        }

        if (fb) {
            m_basicBlockBoundaries.insert(fb->getZExtValue());
        }
        if (dest) {
            m_basicBlockBoundaries.insert(dest->getZExtValue());
        }
    }
}

void FunctionBuilder::createFunction(Module &M)
{
    TranslatedBlocksMap::const_iterator it = m_allInstructions.find(m_entryPoint);
    TranslatedBlock *tb = (*it).second;
    assert(tb && "Entry point not present in the list of all instructions");

    std::string fn = TbPreprocessor::getFunctionName(tb->getAddress());
    M.getOrInsertFunction(fn, tb->getFunction()->getFunctionType());
    m_function = M.getFunction(fn);
}

void FunctionBuilder::callInstruction(BasicBlock *bb, Function *f)
{
    std::vector<Value*> CallArguments;
    CallArguments.push_back(&*m_function->arg_begin());
    CallInst::Create(f, CallArguments.begin(), CallArguments.end(), "", bb);
}

void FunctionBuilder::buildPcGep(Module &M, BasicBlock *bb)
{
    Value *env = &*bb->getParent()->arg_begin();

    SmallVector<Value*, 2>gepElements;
    gepElements.push_back(ConstantInt::get(M.getContext(), APInt(32,  0)));
    gepElements.push_back(ConstantInt::get(M.getContext(), APInt(32,  5)));

    m_pcptr = GetElementPtrInst::Create(env, gepElements.begin(), gepElements.end(), "", bb);
}

void FunctionBuilder::buildConditionalBranch(Module &M, BasicBlocksMap &bbmap, BasicBlock *bb, TranslatedBlock *tb)
{
    ETranslatedBlockType type = tb->getType();
    assert(type == BB_COND_JMP || type == BB_REP);

    Instruction *loadpc = new LoadInst(m_pcptr);

    //LOGDEBUG("loadpc:" << *loadpc << std::flush << std::endl);

    //XXX: THIS WAS REQUIRED BEFORE INTRODUCING X64 SUPPORT IN THE TRANSLATOR!!!
    //XXX: Should probably check whether to do the zero extension (if bit-width smaller)
    //ZExtInst *truncInst = new ZExtInst(loadpc, IntegerType::get(M.getContext(), 64));

    loadpc->insertAfter(&bb->back());
    //truncInst->insertAfter(loadpc);

    ConstantInt *destInt = dyn_cast<ConstantInt>(tb->getDestination());
    ConstantInt *fbInt = dyn_cast<ConstantInt>(tb->getFallback());
    assert(destInt && fbInt && "TbPreprocessor screwed up");

    //Retrieve basic blocks for destination and fallback
    BasicBlocksMap::iterator dit = bbmap.find(destInt->getZExtValue());
    BasicBlocksMap::iterator fit = bbmap.find(fbInt->getZExtValue());
    assert(dit != bbmap.end() && fit != bbmap.end() && "FunctionBuilder is broken");

    BasicBlock *destBb = (*dit).second;
    BasicBlock *fbBb = (*fit).second;

    //Generate compare instruction
    CmpInst *cmpInst =  new ICmpInst(*bb, ICmpInst::ICMP_EQ, loadpc, destInt, "");
    //cmpInst->insertAfter(loadpc);

    //Generate branch instruction
    BranchInst::Create(destBb, fbBb, cmpInst, bb);
}

void FunctionBuilder::buildIndirectConditionalBranch(Module &M, BasicBlocksMap &bbmap, BasicBlock *bb, TranslatedBlock *tb)
{
    ETranslatedBlockType type = tb->getType();
    assert(type == BB_COND_JMP_IND);

    Instruction *loadpc = new LoadInst(m_pcptr);
    ZExtInst *truncInst = new ZExtInst(loadpc, IntegerType::get(M.getContext(), 64));

    loadpc->insertAfter(&bb->back());
    truncInst->insertAfter(loadpc);

    //Retrieve basic blocks for fallback only. Destination is unknown.
    ConstantInt *fbInt = dyn_cast<ConstantInt>(tb->getFallback());
    assert(fbInt && "TbPreprocessor screwed up");

    BasicBlocksMap::iterator fit = bbmap.find(fbInt->getZExtValue());
    assert(fit != bbmap.end() && "FunctionBuilder is broken");

    BasicBlock *fbBb = (*fit).second;

    //Generate compare instruction
    CmpInst *cmpInst = new ICmpInst(*bb, ICmpInst::ICMP_EQ, truncInst, fbInt);
    //cmpInst->insertAfter(loadpc);

    //Create a new basic block with a jump marker
    BasicBlock *destBb = BasicBlock::Create(M.getContext(), "", bb->getParent(), NULL);
    insertJumpMarker(M, destBb, loadpc);
    ReturnInst::Create(M.getContext(), destBb);

    //Generate branch instruction. fbBb and destBb are swapped.
    BranchInst::Create(fbBb, destBb, cmpInst, bb);
}

void FunctionBuilder::buildUnconditionalBranch(Module &M, BasicBlocksMap &bbmap, BasicBlock *bb, TranslatedBlock *tb)
{
    ETranslatedBlockType type = tb->getType();
    assert(type == BB_JMP || type == BB_JMP_IND);

    if (type == BB_JMP) {
        //Retrieve basic blocks for destination.
        ConstantInt *destInt = dyn_cast<ConstantInt>(tb->getDestination());
        assert(destInt && destInt && "TbPreprocessor screwed up");

        BasicBlocksMap::iterator dit = bbmap.find(destInt->getZExtValue());
        assert(dit != bbmap.end() && "FunctionBuilder is broken");

        BasicBlock *destBb = (*dit).second;
        //Generate branch instruction
        BranchInst::Create(destBb, bb);
        return;
    }else if (type == BB_JMP_IND) {
        Instruction *loadpc = new LoadInst(m_pcptr);
        //ZExtInst *truncInst = new ZExtInst(loadpc, IntegerType::get(M.getContext(), 64));

        loadpc->insertAfter(&bb->back());
        //truncInst->insertAfter(loadpc);
        insertJumpMarker(M, bb, loadpc);
        ReturnInst::Create(M.getContext(), ConstantInt::get(M.getContext(), APInt(64,  0)), bb);
        return;
    }
    assert(false);
}

void FunctionBuilder::insertJumpMarker(Module &M, BasicBlock *bb, Value *target)
{
    Function *jumpMarker = M.getFunction(TbPreprocessor::getJumpMarker());
    assert(jumpMarker && "TbPreprocessor must be run first!");

    std::vector<Value*> CallArguments;
    CallArguments.push_back(&*m_function->arg_begin());
    CallArguments.push_back(target);
    CallInst *marker = CallInst::Create(jumpMarker, CallArguments.begin(), CallArguments.end());
    marker->insertAfter(&bb->back());
}

void FunctionBuilder::buildFunction(Module &M)
{
    BasicBlocksMap bbMap;
    AddressVector sortedAddresses;

    //DenseSet is not sorted
    foreach(it, m_functionInstructions.begin(), m_functionInstructions.end()) {
        sortedAddresses.push_back(*it);
    }
    std::sort(sortedAddresses.begin(), sortedAddresses.end());

    BasicBlock *curbb = NULL;

    //Create a new set of LLVM basic blocks
    foreach(it, m_basicBlockBoundaries.begin(), m_basicBlockBoundaries.end()) {
        uint64_t addr = *it;
        std::stringstream ss;
        ss << "bb_" << std::hex << addr;
        BasicBlock *llvmBb = BasicBlock::Create(M.getContext(), ss.str(), m_function, NULL);
        bbMap[addr] = llvmBb;
    }

    //Fix the entry block. It must come first in the LLVM function
    BasicBlock *entryBb = bbMap[m_entryPoint];
    assert(entryBb);
    curbb = &*m_function->begin();
    if (curbb != entryBb) {
        entryBb->moveBefore(curbb);
    }

    buildPcGep(M, entryBb);

    BasicBlock *prevBb = NULL;

    foreach(it, sortedAddresses.begin(), sortedAddresses.end()) {
        uint64_t addr = *it;
        if (m_basicBlockBoundaries.count(addr)) {
            curbb = bbMap[addr];
            assert(curbb);

            if (prevBb) {
                BranchInst::Create(curbb, prevBb);
                prevBb = NULL;
            }
        }
        assert(curbb);

        TranslatedBlocksMap::const_iterator tbit = m_allInstructions.find(*it);
        assert(tbit != m_allInstructions.end() && "Incomplete list of basic blocks");
        TranslatedBlock *tb = (*tbit).second;

        //Insert a call to the translated instruction
        callInstruction(curbb, tb->getFunction());

        //Build the terminators
        switch(tb->getType()) {
            case BB_REP:
            case BB_COND_JMP:
                buildConditionalBranch(M, bbMap, curbb, tb);
                prevBb = NULL;
                break;

            case BB_COND_JMP_IND:
                buildIndirectConditionalBranch(M, bbMap, curbb, tb);
                prevBb = NULL;
                break;

            case BB_JMP:
            case BB_JMP_IND:
                buildUnconditionalBranch(M, bbMap, curbb, tb);
                prevBb = NULL;
                break;

            case BB_RET:
                ReturnInst::Create(M.getContext(), ConstantInt::get(M.getContext(), APInt(64,  0)), curbb);
                prevBb = NULL;
                break;

            default:
                prevBb = curbb;
                break;
        }
    }
}


bool FunctionBuilder::runOnModule(llvm::Module &M)
{
    createFunction(M);
    computeBasicBlockBoundaries();
    buildFunction(M);
    //LOGDEBUG(*m_function << std::flush);
    return true;
}

}
