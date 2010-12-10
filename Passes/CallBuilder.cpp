#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/InstIterator.h>

#include "CallBuilder.h"
#include "Utils.h"

#include <set>

using namespace llvm;

char CallBuilder::ID = 0;
RegisterPass<CallBuilder>
  CallBuilder("CallBuilder", "Rebuild all function calls",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

void CallBuilder::createMapping(llvm::Module &M)
{
    if (m_functions.size() == 0) {
        return;
    }

    //Create variable storing the number of functions
    GlobalVariable *functionCountVar = M.getGlobalVariable("__x2l_g_functionCount", false);
    assert(!functionCountVar);

    const IntegerType *functionCountVarType = Type::getInt32Ty(M.getContext());
    ConstantInt *functionCountVarInit = ConstantInt::get(M.getContext(), APInt(32,  m_functions.size()));
    functionCountVar = new GlobalVariable(M, functionCountVarType, false, llvm::GlobalVariable::ExternalLinkage,
                                          functionCountVarInit, "__x2l_g_functionCount");

    //Create an array with uint64_t ints representing native functions
    GlobalVariable *nativeArrayVar = M.getGlobalVariable("__x2l_g_functionlist_val", false);
    assert(!nativeArrayVar);

    std::vector<Constant*> nativeVector;
    foreach(it, m_functions.begin(), m_functions.end()) {
        nativeVector.push_back(ConstantInt::get(M.getContext(), APInt(64,  (*it).first)));
    }

    ArrayType *nativeArrayVarType = ArrayType::get(Type::getInt64Ty(M.getContext()), m_functions.size());
    Constant *nativeArrayVarInit = ConstantArray::get(nativeArrayVarType, nativeVector);;
    nativeArrayVar = new GlobalVariable(M, nativeArrayVarType, false, llvm::GlobalVariable::ExternalLinkage,
                                          nativeArrayVarInit, "__x2l_g_functionlist_val");

    //Create an array with pointers to the LLVM functions
    GlobalVariable *llvmArrayVar = M.getGlobalVariable("__x2l_g_functionlist_ptr", false);
    assert(!llvmArrayVar);

    std::vector<Constant*> llvmVector;
    foreach(it, m_functions.begin(), m_functions.end()) {
        llvmVector.push_back((*it).second);
    }

    const Type *functionType = (*m_functions.begin()).second->getType();
    ArrayType *llvmArrayVarType = ArrayType::get(functionType , m_functions.size());
    Constant *llvmArrayVarInit = ConstantArray::get(llvmArrayVarType, llvmVector);;
    llvmArrayVar = new GlobalVariable(M, llvmArrayVarType, false, llvm::GlobalVariable::ExternalLinkage,
                                          llvmArrayVarInit, "__x2l_g_functionlist_ptr");

}

void CallBuilder::resolveCall(llvm::Module &M, llvm::Function &F, CallInst *ci)
{
    ConstantInt *targetPcCi = dyn_cast<ConstantInt>(ci->getOperand(1));
    assert(targetPcCi);
    const uint64_t targetPc = *targetPcCi->getValue().getRawData();

    //Fetch the right function
    FunctionMap::iterator it = m_functions.find(targetPc);

    //Might be null if from dll?
    assert(it != m_functions.end());

    Function *targetFunction = (*it).second;

    std::vector<Value*> CallArguments;
    CallArguments.push_back(&*F.arg_begin());
    CallInst *targetCall = CallInst::Create(targetFunction, CallArguments.begin(), CallArguments.end());
    targetCall->insertAfter(ci);
}

bool CallBuilder::runOnModule(llvm::Module &M)
{
    bool modified = false;

    createMapping(M);

    Function *callMarker = M.getFunction("call_marker");
    assert(callMarker && "Run TerminatorMarker first");

    foreach(fit, M.begin(), M.end()) {
        Function &F = *fit;
        if (F.getNameStr().find("function_") == std::string::npos) {
            continue;
        }

        foreach(iit, inst_begin(F), inst_end(F)) {
            CallInst *ci = dyn_cast<CallInst>(&*iit);
            if (!ci || ci->getCalledFunction() != callMarker) {
                continue;
            }

            Value *isInlinable = ci->getOperand(2);
            if (isInlinable == ConstantInt::getTrue(M.getContext())) {
                //Skip jumps
                continue;
            }

            ConstantInt *targetPc = dyn_cast<ConstantInt>(ci->getOperand(1));
            if (!targetPc) {
                //Skip indirect calls
                continue;
            }

            resolveCall(M, F, ci);
            modified = true;
        }
    }

    return modified;
}
