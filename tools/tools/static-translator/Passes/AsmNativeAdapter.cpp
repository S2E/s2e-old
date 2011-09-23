#include <llvm/Function.h>
#include <llvm/Target/TargetData.h>

#include "lib/X86Translator/CpuStatePatcher.h"
#include "AsmNativeAdapter.h"
#include "CallingConvention.h"

#include <sstream>

using namespace llvm;

namespace s2etools {


char AsmNativeAdapter::ID = 0;
LogKey AsmNativeAdapter::TAG = LogKey("AsmNativeAdapter");


void AsmNativeAdapter::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<TargetData>();
    AU.addRequired<CallingConvention>();
}

Function* AsmNativeAdapter::createNativeWrapper(Module &M,
                                           Function *deinlinedFunction,
                                           Function *nativeFunction)
{
    //Create the function type
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(PointerType::getUnqual(m_cpuStateType));

    foreach(it, deinlinedFunction->arg_begin(), deinlinedFunction->arg_end()) {
        Argument &arg = *it;
        paramTypes.push_back(arg.getType());
    }

    FunctionType *wrapperFunctionType = FunctionType::get(
            deinlinedFunction->getReturnType(), paramTypes, false);

    //Create the function
    std::stringstream ss;
    ss << deinlinedFunction->getNameStr() << "_wrapper";

    Function *wrapperFunction = dyn_cast<Function>(M.getOrInsertFunction(ss.str(), wrapperFunctionType));
    assert(wrapperFunction);

    LOGDEBUG(*wrapperFunction << std::endl << std::flush);

    //Create the only basic block in the wrapper function
    BasicBlock *BB = BasicBlock::Create(M.getContext(), "", wrapperFunction, NULL);

    //Create an array of integers for the stack
    const IntegerType *stackElementType = IntegerType::get(M.getContext(), m_targetData->getPointerSizeInBits());

    //The stack holds the original parameters + the return value + 8 free slots for the callee
    //XXX: wtf hard-coded constants
    unsigned stackSize = wrapperFunction->getArgumentList().size() + 1 + 8;
    Value *stackElementCount = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), APInt(32, stackSize));
    Value *stackLastElement = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), APInt(32, stackSize-1));
    AllocaInst *stackAlloc = new AllocaInst(stackElementType,  stackElementCount, "", BB);

    //Push all the parameters
    //XXX: Assume here they are pushed from right to left!
    std::vector<Value*> nativeParameters;
    int skipfirst = true;
    foreach(it, wrapperFunction->arg_begin(), wrapperFunction->arg_end()) {
        if (skipfirst) {
            skipfirst = false;
            continue;
        }
        nativeParameters.push_back(&*it);
    }

    //Check whether to pass a hidden parameter that will hold
    //a complex return type
    const Type *returnType = wrapperFunctionType->getReturnType();
    Value *returnedValue = NULL;

    //Initialize the stack pointer
    Value *cpuStateParam = CpuStatePatcher::getCpuStateParam(*wrapperFunction);

    GetElementPtrInst *stackPtr = CpuStatePatcher::getStackPointer(M, cpuStateParam);
    stackPtr->insertAfter(&(BB->back()));
    GetElementPtrInst *stackTop = GetElementPtrInst::Create(stackAlloc, stackLastElement, "", BB);
    PtrToIntInst *stackTopAsInt = new PtrToIntInst(stackTop, stackElementType, "", BB);
    new StoreInst(stackTopAsInt, stackPtr, BB);


    //Generate the right type of wrapper depending on the current calling convention
    m_callingConvention->generateGuestCall(cpuStateParam, nativeFunction, BB, nativeParameters, stackTopAsInt);


    returnedValue = m_callingConvention->extractReturnValues(cpuStateParam, returnType, BB);
    if (returnedValue) {
        ReturnInst::Create(M.getContext(), returnedValue, BB);
    }else {
        ReturnInst::Create(M.getContext(), BB);
    }

    LOGDEBUG(*wrapperFunction << std::flush);

    wrapperFunction->setLinkage(Function::InternalLinkage);

    return wrapperFunction;
}

bool AsmNativeAdapter::replaceDeinlinedFunction(llvm::Module &M,
                              llvm::Function *deinlinedFunction,
                              llvm::Function *nativeWrapper)
{
    unsigned callSitesCount = 0;
    std::vector<Instruction*> toErase;


    foreach(uit, deinlinedFunction->use_begin(), deinlinedFunction->use_end()) {
        CallInst *callSite = dyn_cast<CallInst>(*uit);
        if (!callSite) {
            continue;
        }
        ++callSitesCount;

        Function *caller = callSite->getParent()->getParent();
        CpuStateAllocs::iterator it = m_cpuStateAllocs.find(caller);
        AllocaInst *cpuState = NULL;
        if (it == m_cpuStateAllocs.end()) {
            //Create an alloca instruction for the CPU state
            cpuState = new AllocaInst(m_cpuStateType, "");
            cpuState->insertBefore(&*caller->getEntryBlock().begin());
            m_cpuStateAllocs[caller] = cpuState;
        }else {
            cpuState = (*it).second;
        }

        SmallVector<Value *, 5> args;
        args.push_back(cpuState);
        for (unsigned argIdx = 1; argIdx < callSite->getNumOperands(); ++argIdx) {
            args.push_back(callSite->getOperand(argIdx));
        }
        CallInst *callToWrapper = CallInst::Create(nativeWrapper, args.begin(), args.end(), "");
        callToWrapper->insertBefore(callSite);
        callSite->replaceAllUsesWith(callToWrapper);
        toErase.push_back(callSite);
    }

    foreach(it, toErase.begin(), toErase.end()) {
        (*it)->eraseFromParent();
    }

    if (!callSitesCount) {
        LOGERROR(deinlinedFunction->getNameStr() << " not called from anywhere" << std::endl);
        return false;
    }

    if (callSitesCount > 1) {
        LOGERROR(deinlinedFunction->getNameStr() << " called multiple times" << std::endl);
        return false;
    }

    return true;
}

bool AsmNativeAdapter::runOnModule(Module &M)
{
    m_targetData = &getAnalysis<TargetData>();
    m_callingConvention = &getAnalysis<CallingConvention>();
    assert(m_targetData && "Must have TargetData");

    assert(m_targetData->getPointerSize() == sizeof(uint64_t) && "We only support 64-bit targets for now");

    m_cpuStateType = CpuStatePatcher::getCpuStateType(M);

    foreach(it, m_functionMap.begin(), m_functionMap.end()) {
        Function *deinlined = (*it).first;
        Function *native = (*it).second;
        Function *nativeWrapper = createNativeWrapper(M, deinlined, native);
        replaceDeinlinedFunction(M, deinlined, nativeWrapper);
    }

    return true;
}

}
