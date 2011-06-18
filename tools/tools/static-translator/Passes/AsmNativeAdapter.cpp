#include <llvm/Function.h>
#include <llvm/target/TargetData.h>

#include "lib/X86Translator/CpuStatePatcher.h"
#include "AsmNativeAdapter.h"

#include <sstream>

using namespace llvm;

namespace s2etools {


char AsmNativeAdapter::ID = 0;
LogKey AsmNativeAdapter::TAG = LogKey("AsmNativeAdapter");


void AsmNativeAdapter::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<TargetData>();
}

void AsmNativeAdapter::createNativeWrapper(Module &M,
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

    //Create the only basic block in the wrapper function
    BasicBlock *BB = BasicBlock::Create(M.getContext(), "", wrapperFunction, NULL);

    //Create an array of integers for the stack
    const IntegerType *stackElementType = IntegerType::get(M.getContext(), m_targetData->getPointerSizeInBits());

    //The stack holds the original parameters + the return value + 8 free slots for the callee
    unsigned stackSize = wrapperFunction->getArgumentList().size() + 1 + 8;
    Value *stackElementCount = ConstantInt::get(stackElementType,
                                                APInt(m_targetData->getPointerSizeInBits(),
                                                      stackSize));
    AllocaInst *stackAlloc = new AllocaInst(stackElementType,  stackElementCount, "", BB);

    //Store the original parameters on the stack
    unsigned argIdx = 0;
    foreach(it, wrapperFunction->arg_begin(), wrapperFunction->arg_end()) {
        Argument &arg = *it;
        if (argIdx == 0) {
            ++argIdx;
            continue;
        }

        assert(arg.getType()->isInteger() || dyn_cast<PointerType>(arg.getType()));

        //Create a get element ptr instruction to access the array
        SmallVector<Value*, 1> gepElements;
            gepElements.push_back(ConstantInt::get(M.getContext(), APInt(32,  argIdx-1)));
        GetElementPtrInst *gep = GetElementPtrInst::Create(stackAlloc, gepElements.begin(), gepElements.end(), "", BB);

        //Store the parameter in the array
        new StoreInst(&arg, gep, BB);

        ++argIdx;
    }

    //Initialize the stack pointer at call
    Value *cpuStateParam = CpuStatePatcher::getCpuStateParam(*wrapperFunction);
    GetElementPtrInst *stackPtr = CpuStatePatcher::getStackPointer(M, cpuStateParam);
    stackPtr->insertAfter(&BB->back());

    SmallVector<Value*, 1> spGepElements;
    spGepElements.push_back(ConstantInt::get(M.getContext(), APInt(32,  argIdx-1)));
    GetElementPtrInst *spGep = GetElementPtrInst::Create(stackAlloc, spGepElements.begin(), spGepElements.end(), "", BB);
    CastInst *spGepInt = CastInst::CreatePointerCast(spGep, stackElementType, "", BB);


    new StoreInst(spGepInt, stackPtr, BB);

    LOGDEBUG(*wrapperFunction << std::flush);
    LOGDEBUG(*nativeFunction << std::flush);

    //Invoke the native function
    SmallVector<Value*, 1> arguments;
    arguments.push_back(CpuStatePatcher::getCpuStateParam(*wrapperFunction));

    CallInst::Create(nativeFunction, arguments.begin(), arguments.end(), "", BB);

    //Get back the result
    GetElementPtrInst *eaxPtr = CpuStatePatcher::getResultRegister(M, cpuStateParam);
    eaxPtr->insertAfter(&BB->back());
    LoadInst *loadResult = new LoadInst(eaxPtr, "", BB);

    ReturnInst::Create(M.getContext(), loadResult, BB);

    LOGDEBUG(*wrapperFunction);
}

bool AsmNativeAdapter::runOnModule(Module &M)
{
    m_targetData = &getAnalysis<TargetData>();
    assert(m_targetData && "Must have TargetData");

    assert(m_targetData->getPointerSize() == sizeof(uint32_t) && "We only support 32-bit targets for now");

    m_cpuStateType = CpuStatePatcher::getCpuStateType(M);

    foreach(it, m_functionMap.begin(), m_functionMap.end()) {
        Function *deinlined = (*it).first;
        Function *native = (*it).second;
        createNativeWrapper(M, deinlined, native);
    }

    return true;
}

}
