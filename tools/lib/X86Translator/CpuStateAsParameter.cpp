#include <llvm/Transforms/Utils/Cloning.h>
#include "lib/X86Translator/CpuStatePatcher.h"
#include "CpuStateAsParameter.h"

using namespace llvm;

namespace s2etools {

char CpuStateAsParameter::ID = 0;
LogKey CpuStateAsParameter::TAG = LogKey("CpuStateAsParameter");

llvm::Function *CpuStateAsParameter::createFunction(llvm::Module &M, llvm::Function *original)
{
    //Create the function type
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(PointerType::getUnqual(m_cpuStateType));

    foreach(it, original->arg_begin(), original->arg_end()) {
        Argument &arg = *it;
        paramTypes.push_back(arg.getType());
    }

    FunctionType *newFunctionType = FunctionType::get(
            original->getReturnType(), paramTypes, false);

    Function *newFunction = dyn_cast<Function>(M.getOrInsertFunction(original->getNameStr()+"_pa", newFunctionType));
    assert(newFunction);
    return newFunction;
}

void CpuStateAsParameter::getValuesToPatch(llvm::Module &M, llvm::Function *F,
                                              Values &toPatch)
{
    if (toPatch.count(F)) {
        return;
    }

    if (F->arg_size() >= 1) {
        Value *p = CpuStatePatcher::getCpuStateParam(*F);
        if (p->getType() == PointerType::getUnqual(m_cpuStateType)) {
            return;
        }
    }

    toPatch.insert(F);

    foreach(uit, F->use_begin(), F->use_end()) {
        CallInst *ci = dyn_cast<CallInst>(*uit);
        if (ci) {
            getValuesToPatch(M, ci->getParent()->getParent(), toPatch);
        }else {
            toPatch.insert(*uit);
        }
    }
}

void CpuStateAsParameter::appendEnvParam(llvm::Module &M, Values &toPatch,
                                         OriginalToPatched &patchMap)
{
    foreach(it, toPatch.begin(), toPatch.end()) {
        if (Function *f = dyn_cast<Function>(*it)) {
            patchMap[f] = patchFunction(M, f);
        }
    }
}

bool CpuStateAsParameter::patchInstruction(User *user, OriginalToPatched &patchMap)
{
    if (!user) {
        return false;
    }

    return true;
}

void CpuStateAsParameter::patchCallSite(CallInst *ci, OriginalToPatched &patchMap)
{
    Function *patchedF = ci->getParent()->getParent();

    Function *patchedCallee = patchMap[ci->getCalledFunction()];
    assert(patchedCallee);

    //Create a new call instruction with the env parameter
    std::vector<Value*> params;
    params.push_back(CpuStatePatcher::getCpuStateParam(*patchedF));
    for(unsigned argIdx = 0; argIdx < ci->getNumOperands()-1; ++argIdx) {
        params.push_back(ci->getOperand(argIdx+1));
    }

    CallInst *patchedCall = CallInst::Create(patchedCallee, params.begin(), params.end(), "", ci);
    ci->replaceAllUsesWith(patchedCall);
    ci->eraseFromParent();
}

void CpuStateAsParameter::patchUses(llvm::Module &M, OriginalToPatched &patchMap)
{
    foreach(it, patchMap.begin(), patchMap.end()) {
        Function *original = (*it).first;
        Function *patched = (*it).second;

        std::vector<User*> uses(original->use_begin(), original->use_end());

        foreach(uit, uses.begin(), uses.end()) {
            LOGDEBUG("USES: " << original->getNameStr() << " - " <<  *(*uit) << std::endl << std::flush);
            if (CallInst *ci = dyn_cast<CallInst>(*uit)) {
                patchCallSite(ci, patchMap);
            }else if (ConstantExpr *cste = dyn_cast<ConstantExpr>(*uit)) {
                Constant *newCste;
                switch(cste->getOpcode()) {
                    case Instruction::PtrToInt:
                        newCste = ConstantExpr::getPtrToInt(patched, cste->getType());
                        break;
                    case Instruction::BitCast:
                        newCste = ConstantExpr::getZExtOrBitCast(patched, cste->getType());
                        break;
                default:
                        assert(false && "Don't know how to patch this");
                }

                cste->replaceAllUsesWith(newCste);
            }else {
                assert(false && "Don't know how to patch this");
            }
        }
    }

}

//Return the patched function
llvm::Function* CpuStateAsParameter::patchFunction(llvm::Module &M, llvm::Function *F)
{
    Function *newFunction = createFunction(M, F);

    DenseMap<const Value*, Value*> ValueMap;

    unsigned argIdx = 0;

    std::vector<Value*> args;
    foreach(ait, newFunction->arg_begin(), newFunction->arg_end()) {
        args.push_back(&*ait);
    }

    foreach(ait, F->arg_begin(), F->arg_end()) {
        ValueMap[&*ait] = args[argIdx+1];
        ++argIdx;
    }

    std::vector<ReturnInst*> Returns;
    CloneFunctionInto(newFunction, F, ValueMap, Returns);
    LOGDEBUG("Deleting body of " << F->getNameStr() << std::endl);
    F->deleteBody();

    return newFunction;
}

void CpuStateAsParameter::patchEnv(llvm::Module &M)
{
    std::vector<Instruction*> toErase;
    foreach(uit, m_env->use_begin(), m_env->use_end()) {
        if (LoadInst *instr = dyn_cast<LoadInst>(*uit)) {
            LOGDEBUG("Patching: " << *instr << std::endl << std::flush);
            Function *f = instr->getParent()->getParent();
            instr->replaceAllUsesWith(CpuStatePatcher::getCpuStateParam(*f));
            toErase.push_back(instr);
        }else {
            LOGERROR("Instruction not implemented: " << *(*uit) << std::endl << std::flush);
            assert(false && "Not implemented");
        }
    }

    foreach(it, toErase.begin(), toErase.end()) {
        (*it)->eraseFromParent();
    }
}

bool CpuStateAsParameter::runOnModule(llvm::Module &M)
{
    m_cpuStateType = CpuStatePatcher::getCpuStateType(M);
    m_env = M.getGlobalVariable("env");

    if (!m_env) {
        //If there are no helpers used, env may be optimized away and be missing.
        return false;
    }

    foreach(uit, m_env->use_begin(), m_env->use_end()) {
        Instruction *instr = dyn_cast<Instruction>(*uit);
        if (!instr) {
            continue;
        }

        m_functionsUsingEnv.insert(instr->getParent()->getParent());
    }

    Values valuesToPatch;
    foreach(fit, m_functionsUsingEnv.begin(), m_functionsUsingEnv.end()) {
        getValuesToPatch(M, *fit, valuesToPatch);
    }

    OriginalToPatched patchMap;
    appendEnvParam(M, valuesToPatch, patchMap);

    patchUses(M, patchMap);
    patchEnv(M);
    return true;
}

}
