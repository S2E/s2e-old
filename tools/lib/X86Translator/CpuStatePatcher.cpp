extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <sstream>
#include <llvm/TypeSymbolTable.h>

#include "lib/Utils/Utils.h"
#include "CpuStatePatcher.h"
#include "ForcedInliner.h"

using namespace llvm;

namespace s2etools {

char CpuStatePatcher::ID = 0;
LogKey CpuStatePatcher::TAG = LogKey("CpuStatePatcher");

std::string CpuStatePatcher::s_cpuStateTypeName = "struct.CPUX86State";

CpuStatePatcher::CpuStatePatcher(uint64_t instructionAddress) : FunctionPass((intptr_t)&ID) {
    m_instructionAddress = instructionAddress;
}

CpuStatePatcher::~CpuStatePatcher()
{
    delete m_targetData;
}

bool CpuStatePatcher::getRegisterIndex(const llvm::GetElementPtrInst *reg, unsigned &index)
{
    if (reg->getNumIndices() != 3) {
        return false;
    }

    ConstantInt *regArray = dyn_cast<ConstantInt>(reg->getOperand(2));
    if (!regArray || regArray->getZExtValue() != 0) {
        return false;
    }

    ConstantInt *x86reg = dyn_cast<ConstantInt>(reg->getOperand(3));
    if (!regArray || x86reg->getZExtValue() > R_EDI) {
        return false;
    }

    index = x86reg->getZExtValue();
    return true;
}

void CpuStatePatcher::getAllLoadStores(Instructions &I, llvm::Function &F) const
{
    foreach(bit, F.begin(), F.end()) {
        BasicBlock &BB = *bit;
        foreach(iit, BB.begin(), BB.end()) {
            Instruction *instr = &*iit;
            if (dyn_cast<LoadInst>(instr) || dyn_cast<StoreInst>(instr)) {
                I.push_back(instr);
            }
        }
    }
}

/**
 * Simple pattern matcher to remove the uglygeps from the code.
 * It assumes that accesses to the CPU state are aligned.
 * If not, it returns NULL and the original uglygep will remain.
 * Correctness of the deuglygepified code is guaranteed.
 */
Instruction *CpuStatePatcher::createGep(llvm::Instruction *instr,
                                        ConstantInt *coffset,
                                        bool &dropInstruction,
                                        unsigned accessSize) const
{
    dropInstruction = false;

    Module *M = instr->getParent()->getParent()->getParent();
    uint64_t offset = coffset->getZExtValue();
    SmallVector<Value *, 3> gepOffsets;

    const Type *memOpType = instr->getOperand(0)->getType();

    ConstantInt *ci = ConstantInt::get(M->getContext(), APInt(32, 0));
    gepOffsets.push_back(ci);

    const Type *ty = m_envType;
    const Type *lastType = ty;

    while(ty) {
        lastType = ty;
        unsigned idx = 0;
        if (const StructType *st = dyn_cast<StructType>(ty)) {
            const StructLayout *ly = m_targetData->getStructLayout(st);
            idx = ly->getElementContainingOffset(offset);
            unsigned noffset = ly->getElementOffset(idx);
            offset -= noffset;
            ty = ty->getContainedType(idx);

        }else if (const ArrayType *at = dyn_cast<ArrayType>(ty)) {
            const Type *elTy = at->getElementType();
            unsigned elSz = m_targetData->getTypeAllocSize(elTy);
            unsigned elCnt = at->getNumElements();
            idx = offset / elSz;
            offset = offset % elSz;
            ty = elTy;
        }else if (const IntegerType *it = dyn_cast<IntegerType>(ty)) {
            //We are done here
            unsigned elSz = m_targetData->getTypeAllocSize(it);
            if (!offset && accessSize == elSz) {
                break;
            }
            return NULL;
        }else if (const PointerType *it = dyn_cast<PointerType>(ty)) {
            //We are storing some pointer.
            //Such stuff should be discarded later anyway.
            dropInstruction = true;
            return NULL;
        }else{
            //Don't know how to handle all the cases
            //They should not happen anyway.
            return NULL;
        }

        ConstantInt *ci = ConstantInt::get(M->getContext(), APInt(32, idx));
        gepOffsets.push_back(ci);
    }

    //Create the gep here
    Value *envPtr = instr->getParent()->getParent()->arg_begin();
    return GetElementPtrInst::Create(envPtr, gepOffsets.begin(),
                              gepOffsets.end(), "");
}

void CpuStatePatcher::patchOffset(llvm::Instruction *instr) const
{
    StoreInst *storeInstr = dyn_cast<StoreInst>(instr);
    unsigned addrOperand = storeInstr ? 1 : 0;

    const Type *valueType;
    if (storeInstr) {
        valueType = instr->getOperand(0)->getType();
    }else {
        valueType = instr->getType();
    }

    unsigned accessSize = valueType->getPrimitiveSizeInBits() / 8;

    //LOGDEBUG() << *instr->getOperand(addrOperand) << std::endl << std::flush;
    //Get the offset out of the expression
    IntToPtrInst *address = dyn_cast<IntToPtrInst>(instr->getOperand(addrOperand));
    if (!address) {
        return;
    }

    assert(dyn_cast<IntegerType>(valueType) && "The translator must only load/store integers!");

    BinaryOperator *add = dyn_cast<BinaryOperator>(address->getOperand(0));
    assert(add && add->getOpcode() == BinaryOperator::Add &&
           "We must have an offset to access cpu state");

    ConstantInt *offset = dyn_cast<ConstantInt>(add->getOperand(1));
    assert(offset && "Offset must be a constant");

    //Create a gep instruction for this register access
    bool dropInstruction = false;
    Instruction *gep = createGep(instr, offset, dropInstruction, accessSize);

    if (gep) {
        gep->insertBefore(instr);
        instr->setOperand(addrOperand, gep);
    }

    //In case of stores, the assigned value must be casted to the right type
    if (storeInstr && dropInstruction) {
        instr->eraseFromParent();
    }
}

void CpuStatePatcher::transformArguments(llvm::Function *transformed,
                                         llvm::Function *original) const
{
    //At this point, the cloned function is empty.
    //We need to create an initial basic block.
    Module *M = transformed->getParent();
    BasicBlock *entryBlock = BasicBlock::Create(M->getContext(), "", transformed, NULL);

    //Create a type cast from CPUState to uint8_t** in order to
    //accomodate the existing function. These casts will be removed when
    //after reconstructing accesses to the CPUState structure.
    Value *cpuState = transformed->arg_begin();

    //%envAddr = alloca %struct.State*
    Value *envAddr = new AllocaInst(cpuState->getType(), "", entryBlock);

    //store %struct.State* %env, %struct.State** %env_addr
    StoreInst *storeAddr = new StoreInst(cpuState, envAddr, entryBlock);

    //%envAddr1 = bitcast %struct.State** %env_addr to i64*
    BitCastInst *envAddr1 = new BitCastInst(envAddr,
                                      PointerType::getUnqual(IntegerType::get(M->getContext(), 64)),
                                      "",
                                      entryBlock);

    //Create a call to the old function with the casted environment
    SmallVector<Value*,1> CallArguments;
    CallArguments.push_back(envAddr1);
    CallInst *newCall = CallInst::Create(original, CallArguments.begin(), CallArguments.end());
    newCall->insertAfter(envAddr1);

    //Terminate the basic block
    Constant *retVal = ConstantInt::get(M->getContext(), APInt(64, 0, false));
    ReturnInst::Create(M->getContext(), retVal, entryBlock);

    //LOGDEBUG() << *transformed << std::endl << std::flush;

    //Inline the old function
    ForcedInliner inliner;
    inliner.addFunction(transformed);
    inliner.inlineFunctions();
    original->deleteBody();

    //Now we have an instruction that accepts CPUState as parameter
    //instead of an opaque uint64_t* object.
}

Function *CpuStatePatcher::createTransformedFunction(llvm::Function &original) const
{
    std::stringstream ss;
    ss << "instr_" << std::hex << m_instructionAddress;

    Module *M = original.getParent();
    Function *newInstr = M->getFunction(ss.str());
    assert(!newInstr && "Can patch state only once for each instruction");

    //Fetch the CpuState type
    const Type *cpuStateType = m_envType;
    assert(cpuStateType);

    //Create the function type
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(PointerType::getUnqual(cpuStateType));

    FunctionType *type = FunctionType::get(
            original.getReturnType(), paramTypes, false);

    //Create the function
    newInstr = dyn_cast<Function>(M->getOrInsertFunction(ss.str(), type));

    return newInstr;
}

bool CpuStatePatcher::runOnFunction(llvm::Function &F)
{
    Instructions registerAccesses;

    m_envType = dyn_cast<StructType>(F.getParent()->getTypeByName(s_cpuStateTypeName));
    assert(m_envType && "Type struct.CPUX86State not defined. You must link the emulation bitcode library.");

    m_targetData = new TargetData(F.getParent());
    m_envLayout = m_targetData->getStructLayout(m_envType);

    Function *transformed = createTransformedFunction(F);
    transformArguments(transformed, &F);

    //LOGDEBUG() << *transformed;

    getAllLoadStores(registerAccesses, *transformed);
    //LOGDEBUG() << "There are " << registerAccesses.size() << " register accesses in "
    //        << F.getNameStr() << std::endl;

    foreach(it, registerAccesses.begin(), registerAccesses.end()) {
        Instruction *instr = *it;
        patchOffset(instr);
        //LOGDEBUG() << *transformed;
    }

    m_transformed = transformed;
    return true;
}

}
