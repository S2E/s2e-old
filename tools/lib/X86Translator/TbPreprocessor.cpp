#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "lib/Utils/Utils.h"
#include "lib/Utils/Log.h"
#include "TbPreprocessor.h"
#include "CpuStatePatcher.h"
#include <iostream>

#include <set>
#include <map>
#include <vector>
#include <queue>


using namespace llvm;

namespace s2etools {

char TbPreprocessor::ID = 0;
RegisterPass<TbPreprocessor>
  TbPreprocessor("TbPreprocessor", "Inserts a terminator marker at the end of the block",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

std::string TbPreprocessor::s_instructionMarker = "instruction_marker";
std::string TbPreprocessor::s_jumpMarker = "jump_marker";
std::string TbPreprocessor::s_callMarker = "call_marker";
std::string TbPreprocessor::s_returnMarker = "return_marker";
std::string TbPreprocessor::s_functionPrefix = "function_";

std::string TbPreprocessor::s_ld[4] = {"__ldb_mmu", "__ldw_mmu", "__ldl_mmu", "__ldq_mmu"};
std::string TbPreprocessor::s_st[4] = {"__stb_mmu", "__stw_mmu", "__stl_mmu", "__stq_mmu"};

LogKey TbPreprocessor::TAG = LogKey("TbPreprocessor");

bool TbPreprocessor::isReconstructedFunction(const llvm::Function &f)
{
    return f.getName().startswith(getFunctionPrefix());
}

//XXX: 32-bit assumptions
CallInst* TbPreprocessor::getMemoryLoadFromIndirectCall(CallInst *marker)
{
    ZExtInst *v = dyn_cast<ZExtInst>(TbPreprocessor::getCallTarget(marker));
    if (!v) {
        return NULL;
    }

    CallInst *cs = dyn_cast<CallInst>(v->getOperand(0));
    if (!cs) {
        return NULL;
    }

    if (cs->getCalledFunction()->getName().equals("__ldl_mmu")) {
        return cs;
    }
    return NULL;
}

LoadInst *TbPreprocessor::getRegisterLoadFromIndirectCall(CallInst *marker)
{
    ZExtInst *v = dyn_cast<ZExtInst>(TbPreprocessor::getCallTarget(marker));
    if (!v) {
        //When the translator supports 64-bits registers, there is no intermediate zext.
        LoadInst *cs = dyn_cast<LoadInst>(TbPreprocessor::getCallTarget(marker));
        return cs;
    }

    LoadInst *cs = dyn_cast<LoadInst>(v->getOperand(0));
    if (!cs) {
        return NULL;
    }
    return cs;
}

void TbPreprocessor::initMarkers(llvm::Module *module)
{
    if (m_callMarker || m_returnMarker) {
        return;
    }

    m_envType = dyn_cast<StructType>(module->getTypeByName(CpuStatePatcher::getCpuStateTypeName()));
    assert(m_envType && "Type struct.CPUX86State not defined.");

    FunctionType *type;
    std::vector<const Type *> paramTypes;

    //**********************************
    //1. Deal with call markers
    //First parameter: pointer to the CPU state, for indirect calls
    paramTypes.push_back(PointerType::getUnqual(m_envType));

    //Second parameter is the program counter of the function to call
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));

    //The marker does not return anything
    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_callMarker = dyn_cast<Function>(module->getOrInsertFunction(getCallMarker(), type));

    //**********************************
    //2. Handle the return marker
    paramTypes.clear();
    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_returnMarker = dyn_cast<Function>(module->getOrInsertFunction(getReturnMarker(), type));

    //**********************************
    //2a. Handle the jump marker
    //This is only used for indirect jumps
    paramTypes.clear();

    //First parameter: pointer to the CPU state, for indirect jumps
    paramTypes.push_back(PointerType::getUnqual(m_envType));

    //Second parameter is the program counter of the basic block to which we jump
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));

    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_jumpMarker = dyn_cast<Function>(module->getOrInsertFunction(getJumpMarker(), type));
    m_jumpMarker->setDoesNotReturn(true);

    //**********************************
    //3. Handle the instruction marker
    //Patch all returns with 1 if branch taken, 0 if fallback.
    paramTypes.clear();
    paramTypes.push_back(Type::getInt64Ty(module->getContext()));
    type = FunctionType::get(Type::getVoidTy(module->getContext()), paramTypes, false);
    m_instructionMarker = dyn_cast<Function>(module->getOrInsertFunction(getInstructionMarker(), type));
    m_instructionMarker->setOnlyReadsMemory(true);

    //**********************************
    //tcg_llvm_fork_and_concretize is a special marker placed by the translator
    //upon all assignments to the program counter
    m_forkAndConcretize = module->getFunction("tcg_llvm_fork_and_concretize");
    assert(m_forkAndConcretize);
}

/**
 *  Inserts an instruction marker at the start of the LLVM function.
 */
void TbPreprocessor::markInstructionStart(Function &f)
{
    std::vector<Value*> CallArguments;
    Value *vpc = ConstantInt::get(f.getParent()->getContext(), APInt(64,  m_tb->getAddress()));
    CallArguments.push_back(vpc);
    CallInst *marker = CallInst::Create(m_instructionMarker, CallArguments.begin(), CallArguments.end());
    marker->insertBefore(f.getEntryBlock().begin());
}

/**
 *  Returns the last assignment to program counter
 */
Value *TbPreprocessor::getTargetPc(BasicBlock &bb, CallInst **marker)
{
    ilist<Instruction>::reverse_iterator riit(bb.end());
    ilist<Instruction>::reverse_iterator riitEnd(bb.begin());

    while (riit != riitEnd) {
        CallInst *ci = dyn_cast<CallInst>(&*riit);
        ++riit;
        if (!ci) {
            continue;
        }
        if (ci->getCalledFunction() == m_forkAndConcretize) {
            if (marker) {
                *marker = ci;
            }
            return ci->getOperand(1);
        }
    }
    return NULL;
}

/**
 * Assumes that the final pc assignments are in distinct basic blocks
 */
bool TbPreprocessor::findFinalPcAssignments(Function &f, Value **destination, Value **fallback,
                                            CallInst **destMarker, CallInst **fbMarker)
{    
    *destination = NULL;
    *fallback = NULL;
    *fbMarker = NULL;
    *destMarker = NULL;

    Instructions returnInstructions;
    findReturnInstructions(f, returnInstructions);
    assert(returnInstructions.size() >= 1);

    foreach(iit, returnInstructions.begin(), returnInstructions.end()) {
        BasicBlock *bb = (*iit)->getParent();
        CallInst *marker;
        Value *v = getTargetPc(*bb, &marker);
        assert(v && "Could not find tcg_llvm_fork_and_concretize in basic block");

        //Figure out if this is a fallback
        if (ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
            uint64_t vpc = ci->getZExtValue();
            if (vpc == m_tb->getAddress() + m_tb->getSize()) {
                assert(!*fallback && "Can't have multiple fallback blocks");
                *fallback = v;
                *fbMarker = marker;
            }else {
                assert(!*destination && "Can't have multiple destination blocks");
                *destination = v;
                *destMarker = marker;
            }
        }else {
            //An indirect call cannot have a fallback
            assert(!*destination && "Can't have multiple fallback blocks");
            *destination = v;
            *destMarker = marker;
        }
    }

    return *destination || *fallback;
}

CallInst *TbPreprocessor::buildCallMarker(Function &f, Value *v)
{
    std::vector<Value*> CallArguments;
    CallArguments.push_back(f.arg_begin());
    CallArguments.push_back(v);
    CallInst *marker = CallInst::Create(m_callMarker, CallArguments.begin(), CallArguments.end());
    return marker;
}

CallInst *TbPreprocessor::buildReturnMarker()
{
    std::vector<Value*> CallArguments;
    CallInst *marker = CallInst::Create(m_returnMarker, CallArguments.begin(), CallArguments.end());
    return marker;
}

/**
 * XXX: also extract info about indirect calls
 */
void TbPreprocessor::markCall(Function &f)
{
    assert(m_tb->getType() == BB_CALL || m_tb->getType() == BB_CALL_IND);
    assert(f.size() == 1 && "Call instruction cannot have more than one basic block in it");

    Value *destination, *fallback;
    CallInst *destMarker, *fbMarker;
    if (!findFinalPcAssignments(f, &destination, &fallback, &destMarker, &fbMarker)) {
        LOGERROR("No program counter assignments in the LLVM function" << f << std::endl);
        assert(false);
    }

    assert(destination && destMarker && !fallback && !fbMarker);

    //Create the call_marker instruction
    CallInst *marker = buildCallMarker(f, destination);
    marker->insertAfter(destMarker);
    destMarker->replaceAllUsesWith(destination);
    destMarker->eraseFromParent();

    m_tb->setDestination(destination);

    //The fallback is the instruction right after the call
    m_tb->setFallback(ConstantInt::get(f.getParent()->getContext(),
                                   APInt(64,  m_tb->getAddress() + m_tb->getSize())));
    return;
}

void TbPreprocessor::markReturn(Function &f)
{
    assert(m_tb->getType() == BB_RET);
    assert(f.size() == 1 && "Return instruction cannot have more than one basic bloc in it");


    CallInst *marker;
    Value *programCounter = getTargetPc(f.getEntryBlock(), &marker);
    assert(programCounter && "Something is broken");
    assert(!isa<ConstantInt>(programCounter) && "Return instruction cannot go to a constant address");

    CallInst *ret = buildReturnMarker();
    ret->insertBefore(marker);

    marker->replaceAllUsesWith(programCounter);
    marker->eraseFromParent();
}

void TbPreprocessor::findReturnInstructions(Function &F, Instructions &Result)
{
  foreach(bbit, F.begin(), F.end()) {
      BasicBlock &bb = *bbit;
      TerminatorInst  *term = bb.getTerminator();
      if (term->getOpcode() == Instruction::Ret) {
          Result.push_back(term);
      }
  }
}

void TbPreprocessor::extractJumpInfo(Function &f)
{
    Value *destination=NULL, *fallback=NULL;
    CallInst *destMarker, *fbMarker;
    if (!findFinalPcAssignments(f, &destination, &fallback, &destMarker, &fbMarker)) {
        LOGERROR("No program counter assignments in the LLVM function" << f << std::endl);
        assert(false);
    }

    //These are only consistency checks
    ETranslatedBlockType type = m_tb->getType();
    if (type == BB_REP || type == BB_COND_JMP) {
        assert(destination && fallback && destMarker && fbMarker);
        ConstantInt *destInt = dyn_cast<ConstantInt>(destination);
        ConstantInt *fbInt = dyn_cast<ConstantInt>(fallback);
        assert(destInt && fbInt);
    }else if (type == BB_COND_JMP_IND) {
        assert(destination && fallback && destMarker && fbMarker);
        ConstantInt *fbInt = dyn_cast<ConstantInt>(fallback);
        assert(fbInt && "Fallback must be constant for indirect jumps");
    }else if (type == BB_JMP) {
        assert(destination && !fallback && destMarker && !fbMarker);
        ConstantInt *destInt = dyn_cast<ConstantInt>(destination);
        assert(destInt && "Direct jump must have constant destination address");
    }else if (type == BB_JMP_IND) {
        assert(destination && !fallback && destMarker && !fbMarker);
        ConstantInt *destInt = dyn_cast<ConstantInt>(destination);
        assert(!destInt && "Indirect jumps must have non-constant destination address");
    }

    m_tb->setDestination(destination);
    m_tb->setFallback(fallback);
}

void TbPreprocessor::removeForkAndConcretize(llvm::Function &F) {
    Function::use_iterator uit = m_forkAndConcretize->use_begin();

    while(uit != m_forkAndConcretize->use_end()) {
        CallInst *ci = dyn_cast<CallInst>(*uit);
        ++uit;

        if (!ci || ci->getParent()->getParent() != &F) {
            continue;
        }
        ci->replaceAllUsesWith(ci->getOperand(1));
        ci->eraseFromParent();
    }
}

bool TbPreprocessor::runOnFunction(llvm::Function &F)
{
    initMarkers(F.getParent());

    markInstructionStart(F);

    switch(m_tb->getType()) {
        case BB_DEFAULT:
            m_tb->setFallback(ConstantInt::get(F.getParent()->getContext(),
                                           APInt(64,  m_tb->getAddress() + m_tb->getSize())));
            break;

        case BB_RET:
            markReturn(F);
            break;

        case BB_CALL:
        case BB_CALL_IND:
            markCall(F);
            break;

        case BB_REP:
        case BB_COND_JMP:
        case BB_COND_JMP_IND:
        case BB_JMP_IND:
        case BB_JMP:
            extractJumpInfo(F);
            break;

        default:
            assert(false && "Unknown block type");
    }

    removeForkAndConcretize(F);
    return true;
}

}
