extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/InstrTypes.h>
#include <llvm/Support/InstIterator.h>

#include "lib/Utils/Log.h"

#include "CallBuilder.h"
#include "lib/Utils/Utils.h"

#include <set>
#include <iostream>
#include <sstream>
#include <stack>

using namespace llvm;
using namespace s2etools;
using namespace s2etools::translator;

char CallBuilder::ID = 0;
RegisterPass<CallBuilder>
  CallBuilder("CallBuilder", "Rebuild all function calls",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

std::string CallBuilder::TAG="CallBuilder";

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
    foreach(it, m_functionMap.begin(), m_functionMap.end()) {
        nativeVector.push_back(ConstantInt::get(M.getContext(), APInt(64,  (*it).first)));
    }

    ArrayType *nativeArrayVarType = ArrayType::get(Type::getInt64Ty(M.getContext()), m_functionMap.size());
    Constant *nativeArrayVarInit = ConstantArray::get(nativeArrayVarType, nativeVector);;
    nativeArrayVar = new GlobalVariable(M, nativeArrayVarType, false, llvm::GlobalVariable::ExternalLinkage,
                                          nativeArrayVarInit, "__x2l_g_functionlist_val");

    //Create an array with pointers to the LLVM functions
    GlobalVariable *llvmArrayVar = M.getGlobalVariable("__x2l_g_functionlist_ptr", false);
    assert(!llvmArrayVar);

    std::vector<Constant*> llvmVector;
    foreach(it, m_functionMap.begin(), m_functionMap.end()) {
        llvmVector.push_back((*it).second->getFunction());
    }

    const Type *functionType = (*m_functions.begin())->getFunction()->getType();
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
    FunctionMap::iterator it = m_functionMap.find(targetPc);

    //Might be null if from dll?
    assert(it != m_functionMap.end());

    Function *targetFunction = (*it).second->getFunction();

    std::vector<Value*> CallArguments;
    CallArguments.push_back(&*F.arg_begin());
    CallInst *targetCall = CallInst::Create(targetFunction, CallArguments.begin(), CallArguments.end());
    targetCall->insertAfter(ci);
}


//Get the  address in jmp [address] or jmp address
//Might be better to move this to CBasicBlock
uint64_t CallBuilder::resolveStub(llvm::Module &M, uint64_t callee)
{
    Function *callMarker = M.getFunction("call_marker");
    assert(callMarker);

    FunctionMap::iterator it = m_functionMap.find(callee);
    assert (it != m_functionMap.end());

    //If the target has one instruction that is an indirect jump,
    //we have a stub;
    CFunction *f = (*it).second;
    if (!f->isStub()) {
        return 0;
    }

    //Get the branch target
    Function *stubFunc = f->getFunction();
    LOGDEBUG() << *stubFunc;

    //Look for the call_marker instruction in it
    CallInst *jmpCall = NULL;
    foreach(iit, inst_begin(stubFunc), inst_end(stubFunc)) {
        jmpCall = dyn_cast<CallInst>(&*iit);
        if (!jmpCall || jmpCall->getCalledFunction() != callMarker) {
            continue;
        }
        LOGINFO() << "Found call: " << *jmpCall << std::endl;
        break;
    }


    ConstantInt *addressInDataSegment;
    addressInDataSegment = dyn_cast<ConstantInt>(jmpCall->getOperand(1));
    if (!addressInDataSegment) {
        //Handle the indirect jump here
        //Fetch the __ldl instruction
        //XXX: We should really have a util lib where such algorithms are centralized
        Function *ldlInstr = M.getFunction("__ldl_mmu");
        CallInst *loadInstr = NULL;
        std::stack<Instruction *> instrStack;
        instrStack.push(dyn_cast<Instruction>(jmpCall->getOperand(1)));
        while(!instrStack.empty()) {
            Instruction *instr = instrStack.top();
            instrStack.pop();;
            for(unsigned i=0; i<instr->getNumOperands(); ++i) {
                Instruction *next = dyn_cast<Instruction>(instr->getOperand(i));
                if (!next){
                    continue;
                }
                CallInst *nextCi = dyn_cast<CallInst>(next);
                if (nextCi && nextCi->getCalledFunction() == ldlInstr) {
                    loadInstr = nextCi;
                    while(!instrStack.empty())
                        instrStack.pop();
                    break;
                }
            }
        }
        assert(loadInstr && "Did not find a __ldl_mmu instruction");

        //Fetch the address operand. It must be a constant
        addressInDataSegment = dyn_cast<ConstantInt>(loadInstr->getOperand(1));
        if (!addressInDataSegment) {
            return 0;
        }

        //Look at what is stored at that address
        uint64_t addressToCheck = *addressInDataSegment->getValue().getRawData();

        //Read the address pointing to the symbol section
        uint32_t importAddress;
        bool ret = m_binary->read(addressToCheck, &importAddress, sizeof(importAddress));
        if (!ret) {
            return 0;
        }
        return importAddress;
    }else {
        //We have a relative jump.
        //Look at what is stored at that address
        uint64_t importAddress = *addressInDataSegment->getValue().getRawData();
        return importAddress;
    }

    //XXX: Compilers can put stubs in many different ways. Have to handle them all...
}

bool CallBuilder::patchCallWithLibraryFunction(llvm::Module &M, CallInst *ci, std::string &functionName)
{
    //Get the prototype of the function
    Function *libFunc = M.getFunction(functionName);
    if (!libFunc) {
        LOGERROR() << "Could not find " << functionName << " prototype in LLVM bitcode." << std::endl;
        return false;
    }

    //Load each parameter's value from the stack.
    //Assumes that eip is already pushed.
    unsigned argCount = libFunc->getArgumentList().size();
    std::vector<CallInst*> memoryLoadsFromStack;

    //Load the ESP register first
    //%2 = load i64* %0
    //%4 = add i64 %2, 16
    //%esp_ptr.i = inttoptr i64 %4 to i32*
    //%esp_v.i = load i32* %esp_ptr.i
    Value *opaque = &*ci->getParent()->getParent()->arg_begin();
    Instruction *gep = GetElementPtrInst::Create(opaque, ConstantInt::get(M.getContext(), APInt(32, 0)));
    Instruction *downCast = CastInst::CreatePointerCast(gep, PointerType::get(Type::getInt32Ty(M.getContext()), 0));
    Instruction *opaqueLoad = new LoadInst(downCast);
    ConstantInt *espValue = ConstantInt::get(M.getContext(), APInt(32, offsetof(CPUState, regs[R_ESP])));
    Instruction *addEspOffset = BinaryOperator::CreateAdd(opaqueLoad, espValue);
    Instruction *castToPtr = new IntToPtrInst(addEspOffset, PointerType::get(addEspOffset->getType(), 0));
    Instruction *esp = new LoadInst(castToPtr);

    gep->insertBefore(ci);
    downCast->insertAfter(gep);
    opaqueLoad->insertAfter(downCast);
    addEspOffset->insertAfter(opaqueLoad);
    castToPtr->insertAfter(addEspOffset);
    esp->insertAfter(castToPtr);

    Function *ldlFunc = M.getFunction("__ldl_mmu");
    assert(ldlFunc);

    Instruction *lastInserted = esp;
    for (unsigned i=0; i<argCount; ++i) {
        ConstantInt *negOffset = ConstantInt::get(M.getContext(), APInt(32, 4 * (i+1)));
        Instruction *espOffset = BinaryOperator::CreateAdd(esp, negOffset);
        //%tmp0_v.i = call i32 @__ldl_mmu(i32 %tmp2_v4.i, i32 -1)
        std::vector<Value*> CallArguments;
        CallArguments.push_back(espOffset);
        CallArguments.push_back(ConstantInt::get(M.getContext(), APInt(32, -1)));
        CallInst *ldl = CallInst::Create(ldlFunc, CallArguments.begin(), CallArguments.end());

        espOffset->insertAfter(lastInserted);
        ldl->insertAfter(espOffset);
        lastInserted = ldl;

        memoryLoadsFromStack.push_back(ldl);
    }

    //Create the actual function call
    //XXX: Deal with type casts...
    std::vector<Value*> CallArguments;
    std::vector<const Type*> LibFuncArguments;

    LOGDEBUG() << *libFunc->getType() << std::endl;

    foreach(ait, libFunc->arg_begin(), libFunc->arg_end()) {
        LibFuncArguments.push_back((*ait).getType());
    }

    unsigned i=0;
    foreach(it, memoryLoadsFromStack.begin(), memoryLoadsFromStack.end()) {
        Instruction *cast = *it;
        //Create the casts to the right types. Deal with pointers only for now.
        if (LibFuncArguments[i]->getTypeID() == Type::PointerTyID) {

            cast = new IntToPtrInst(*it, LibFuncArguments[i], "");
            cast->insertAfter(*it);
        }else {
            assert(false && "Unsupported argument type");
        }

        CallArguments.push_back(cast);
        ++i;
    }
    CallInst *functionCall = CallInst::Create(libFunc, CallArguments.begin(), CallArguments.end());
    functionCall->insertBefore(ci);

    //Must make sure to update the return value...

    LOGDEBUG() << *ci->getParent()->getParent();

    return true;
}

bool CallBuilder::resolveLibraryCall(llvm::Module &M, llvm::Function &F, CallInst *ci)
{
    uint64_t targetPc = 0;
    bool isDirect = false;

    ConstantInt *targetPcCi = dyn_cast<ConstantInt>(ci->getOperand(1));
    if (targetPcCi) {
        targetPc = *targetPcCi->getValue().getRawData();
        isDirect = true;
    }

    std::string functionName;
    std::string libName;

    if (isDirect) {
        uint64_t importAddress = resolveStub(M, targetPc);
        if (!importAddress) {
            return false;
        }

        //Get the symbol at this address
        Imports::iterator it = m_imports.find(importAddress);
        assert(it != m_imports.end());
        functionName = (*it).second.second;
        libName = (*it).second.first;
    }

    //XXX: Move mangling to a central place, as it is used in various places
    std::stringstream mangledNameS;
    mangledNameS << libName << "_" << functionName;

    std::string mangledName = mangledNameS.str();

    //We have a function name, patch the call!
    return patchCallWithLibraryFunction(M, ci, mangledName);
}

bool CallBuilder::runOnModule(llvm::Module &M)
{
    bool modified = false;

    foreach(it, m_functions.begin(), m_functions.end()) {
        m_functionMap[(*it)->getAddress()] = *it;
    }

    m_imports = m_binary->getImports();

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

            if (!resolveLibraryCall(M, F, ci)) {
                resolveCall(M, F, ci);
            }

            modified = true;
        }
    }

    return modified;
}
