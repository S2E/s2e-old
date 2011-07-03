extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include "lib/X86Translator/CpuStatePatcher.h"
#include "CallingConvention.h"
#include "TargetBinary.h"

#include "lib/BinaryReaders/Pe.h"
#include "lib/BinaryReaders/Macho.h"

using namespace llvm;

namespace s2etools {

char CallingConvention::ID = 0;
LogKey CallingConvention::TAG = LogKey("CallingConvention");

static RegisterPass<CallingConvention> X("callingconvention",
                                     "Provides calling convention information", false, false);

void CallingConvention::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<TargetBinary>();
    AU.addRequired<TargetData>();
}

//XXX: Assumes x86 architecture
CallingConvention::Convention CallingConvention::getConvention(const GetElementPtrInst *reg) const
{
    unsigned regNum;

    if (!CpuStatePatcher::getRegisterIndex(reg, regNum)) {
        return Unknown;
    }

    if (regNum == R_EAX || regNum == R_ECX || regNum == R_EDX) {
        return CallerSave;
    }
    return CalleeSave;
}

CallingConvention::Abi CallingConvention::getAbi() const
{
    TargetBinary *binary = &getAnalysis<TargetBinary>();
    Binary::Mode mode = binary->getBinary()->getMode();

    PeReader *pe = dynamic_cast<PeReader*>(binary);
    if (pe) {
        if (mode == Binary::BIT64) {
            return MS64;
        }
        return UNKNOWNABI;
    }else {
        if (mode == Binary::BIT64) {
            return SYSV64;
        }
        return UNKNOWNABI;
    }
}

void generateGuestCallCdecl(llvm::Value *cpuState, llvm::Function *callee,
                       llvm::BasicBlock *insertAtEnd,
                       std::vector<llvm::Value> &parameters,
                       llvm::Value *stack)
{
    assert(false && "Not implemented");
}

//XXX: does casting belong to here?
Value* CallingConvention::CastToInteger(Value *value, const IntegerType *resType)
{
    if (value->getType() == resType) {
        return value;
    }

    if (isa<PointerType>(value->getType())) {
        TargetData *targetData = &getAnalysis<TargetData>();
        assert(targetData->getPointerSizeInBits() == resType->getPrimitiveSizeInBits());
        return new PtrToIntInst(value, resType, "");
    }

    if (value->getType()->getPrimitiveSizeInBits() < resType->getPrimitiveSizeInBits()) {
        return new ZExtInst(value, resType, "");
    }else if (value->getType()->getPrimitiveSizeInBits() > resType->getPrimitiveSizeInBits()) {
        return new TruncInst(value, resType, "");
    }else {
        return value;
    }
}

//XXX: does casting belong to here?
Value* CallingConvention::CastIntegerTo(Value *value, const Type *targetType)
{
    if (const IntegerType* it = dyn_cast<IntegerType>(targetType)) {
        return CastToInteger(value, it);
    }else if (const PointerType *ptr = dyn_cast<PointerType>(targetType)) {
        TargetData *targetData = &getAnalysis<TargetData>();
        assert(targetData->getPointerSizeInBits() == value->getType()->getPrimitiveSizeInBits());
        return new IntToPtrInst(value, ptr, "");
    }else {
        assert(false && "Not implemented");
    }
}

//Push the value on the stack, updating the stack pointer
void CallingConvention::push(llvm::BasicBlock *BB, llvm::Value *cpuState, llvm::GetElementPtrInst *stackPtr,
                             llvm::Value *value)
{
    Module *M = BB->getParent()->getParent();
    TargetData *targetData = &getAnalysis<TargetData>();

    //Decrement stack pointer
    Instruction *loadStackPtr = new LoadInst(stackPtr, "", BB);
    ConstantInt *subEspAmount = ConstantInt::get(M->getContext(), APInt(targetData->getPointerSizeInBits(),
                                                                       -(int)(targetData->getPointerSize())));
    Instruction *subEsp = BinaryOperator::CreateAdd(loadStackPtr, subEspAmount, "", BB);
    new StoreInst(subEsp, stackPtr, BB);

    //Store the value
    //Zero-extend the argument
    //XXX: extension not required on some calling conventions...
    const IntegerType *stackElementType = IntegerType::get(M->getContext(), targetData->getPointerSizeInBits());
    LOGDEBUG("Type: " << *stackElementType << " - val:" << *value << std::endl << std::flush);

    value = CastToInteger(value, stackElementType);
    if (Instruction *instr = dyn_cast<Instruction>(value)) {
        instr->insertAfter(&BB->back());
    }


    const Type *ptrType = PointerType::getUnqual(stackElementType);
    IntToPtrInst *llvmStackPtr = new IntToPtrInst(subEsp, ptrType, "", BB);

    new StoreInst(value, llvmStackPtr, BB);
}

/**
 *  Microsoft x64 calling convention
 *  RCX, RDX, R8, R9: 4 params left to right
 *  Others pushed on the stack
 *  Reserve 32 bytes of spill memory on the stack before calling
 *  Do not zero-extend parameters < 64-bits
 *
 *  System V AMD64 ABI convention
 *  RDI, RSI, RDX, RCX, R8 and R9 for registers
 *  XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and XMM7: floating point
 *  For syscall, R10 instead of RCX
 *
 *  Stack layout:
 *  -------------
 *
 *  stack is the bottom of the stack
 *  The CPU state contains the current pointer to the stack top
 *
 *  XXX: XMM registers, floating point
 */
void CallingConvention::generateGuestCall64(llvm::Value *cpuState, llvm::Function *callee,
                       llvm::BasicBlock *insertAtEnd,
                       std::vector<llvm::Value*> &parameters,
                       llvm::Value *stack)
{
    assert(callee->getArgumentList().size() == 1);

    //unsigned msAbi[] = {R_ECX, R_EDX, 8, 9};
    unsigned sysvAbi[] = {R_EDI, R_ESI, R_EDX, R_ECX, 8, 9};
    std::vector<unsigned> regs;

    DenseMap<Value *, Value*> valueToMemoryLocation;

    Abi abi = getAbi();
    assert(abi != UNKNOWNABI && "Not supported ABI");
    assert(abi == SYSV64 || abi == MS64);

    if (abi == SYSV64) {
        for(unsigned i=0; i<sizeof(sysvAbi) / sizeof(sysvAbi[1]); ++i) {
            regs.push_back(sysvAbi[i]);
        }
    }else {
        assert(false && "Not implemented");
    }

    Module *M = insertAtEnd->getParent()->getParent();

    //Create an array of integers for the stack
    TargetData *targetData = &getAnalysis<TargetData>();
    const IntegerType *stackElementType = IntegerType::get(M->getContext(), targetData->getPointerSizeInBits());

    //How many parameters go to registers. We'll have to allocate stack space for them.
    unsigned maxRegParams = parameters.size() < regs.size() ? parameters.size() : regs.size();

    unsigned paramIdx = 0;
    for (paramIdx = 0; paramIdx < parameters.size() && paramIdx < regs.size(); ++paramIdx) {
        GetElementPtrInst *reg = CpuStatePatcher::getRegister(*M, cpuState, regs[paramIdx]);
        reg->insertAfter(&insertAtEnd->back());

        //Zero-extend the argument
        //XXX: standard says we shouldn't do that...
        Value *value = CastToInteger(parameters[paramIdx], stackElementType);
        if (Instruction *instr = dyn_cast<Instruction>(value)) {
            instr->insertAfter(&insertAtEnd->back());
        }

        new StoreInst(value, reg, insertAtEnd);
    }

    GetElementPtrInst *stackPtr = CpuStatePatcher::getStackPointer(*M, cpuState);
    stackPtr->insertAfter(&insertAtEnd->back());

    //Allocate stack space for spilling
    Instruction *loadStackPtr = new LoadInst(stackPtr, "", insertAtEnd);
    ConstantInt *subEspAmount = ConstantInt::get(M->getContext(), APInt(targetData->getPointerSizeInBits(),
                                                                       -(int)(maxRegParams * targetData->getPointerSize())));
    Instruction *subEsp = BinaryOperator::CreateAdd(loadStackPtr, subEspAmount, "", insertAtEnd);
    new StoreInst(subEsp, stackPtr, insertAtEnd);

    //Push the remaining parameters
    while(paramIdx < parameters.size()) {
        push(insertAtEnd, cpuState, stackPtr, parameters[paramIdx]);
        ++paramIdx;
    }

    //Push a dummy return address
    push(insertAtEnd, cpuState, stackPtr, ConstantInt::get(M->getContext(), APInt(targetData->getPointerSizeInBits(), 0xdeadbeef)));

    //Issue the call
    SmallVector<Value*, 1> nativeArgs;
    nativeArgs.push_back(cpuState);
    CallInst::Create(callee, nativeArgs.begin(), nativeArgs.end(), "", insertAtEnd);
}

void CallingConvention::generateGuestCall(llvm::Value *cpuState, llvm::Function *callee,
                       llvm::BasicBlock *insertAtEnd,
                       std::vector<llvm::Value*> &parameters,
                       llvm::Value *stack)
{
    TargetBinary *binary = &getAnalysis<TargetBinary>();
    if (binary->getBinary()->getMode() == Binary::BIT64) {
        generateGuestCall64(cpuState, callee, insertAtEnd, parameters, stack);
    }else {
        assert(false && "Not implemented");
    }
}

bool CallingConvention::returnTypeUsesRegisters(const llvm::Type *ty) const
{
    llvm::SmallVector<unsigned, 2> regs;
    return returnTypeUsesRegisters(ty, regs);
}

bool CallingConvention::returnTypeUsesRegisters(const llvm::Type *ty, llvm::SmallVector<unsigned, 2> &regs) const
{
    regs.clear();

    LOGDEBUG("Return type: " << *ty << std::endl << std::flush);

    if (ty == PointerType::getVoidTy(ty->getContext())) {
        return false;
    }

    if (isa<PointerType>(ty) || ty->isInteger()) {
        //XXX: Assume x86-64 architecture
        regs.push_back(R_EAX);
        return true;
    }

    //Aggregate return types are explicitely passed by parameter to the function.
    //They are handled just like any other parameter.
    //Nothing to do here.
    LOGDEBUG(*ty << std::endl << std::flush);
    assert(false && "Cannot happen");

    return false;
}

Value *CallingConvention::extractReturnValues(llvm::Value *cpuState,
                                            const llvm::Type *returnType,
                                            llvm::BasicBlock *BB)
{
    llvm::SmallVector<unsigned, 2> returnTypeRegs;
    bool returnTypeUsesRegs;

    returnTypeUsesRegs = returnTypeUsesRegisters(returnType, returnTypeRegs);
    if (!returnTypeUsesRegs) {
        return NULL;
    }

    Module *M = BB->getParent()->getParent();

    GetElementPtrInst *resultRegPtr = CpuStatePatcher::getResultRegister(*M, cpuState);
    resultRegPtr->insertAfter(&BB->back());
    Instruction *returnedValue = new LoadInst(resultRegPtr, "", BB);

    Value *cast = CastIntegerTo(returnedValue, returnType);
    if (cast != returnedValue) {
        dyn_cast<Instruction>(cast)->insertAfter(&BB->back());
    }
    return cast;
}

}
