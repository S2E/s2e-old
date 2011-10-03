#include <llvm/Target/TargetData.h>
#include <llvm/Analysis/AliasAnalysis.h>

#include "lib/Utils/Log.h"
#include "lib/Utils/Utils.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "CallBuilder.h"
#include "TargetBinary.h"
#include "RevgenAlias.h"
#include "CallingConvention.h"

using namespace llvm;

namespace s2etools {

// Register this pass...
static RegisterPass<CallBuilder> X("callbuilder", "Rebuilds function calls");


char CallBuilder::ID = 0;
LogKey CallBuilder::TAG = LogKey("CallBuilder");

void CallBuilder::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.setPreservesCFG();
    AU.addRequired<MemoryDependenceAnalysis>();
    AU.addRequired<TargetBinary>();
}

bool CallBuilder::processLocalCall(CallInst *marker, Function *f)
{
    SmallVector<Value*,1> CallArguments;
    CallArguments.push_back(marker->getOperand(1));
    CallInst *newCall = CallInst::Create(f, CallArguments.begin(), CallArguments.end());
    newCall->insertBefore(marker);
    marker->eraseFromParent();
    return true;
}

bool CallBuilder::resolveImport(uint64_t address, std::string &functionName)
{
    uint32_t target =  m_binary->readAddressFromImportTable(address);
    if (!target) {
        LOGDEBUG("Could not read import entry 0x" << std::hex << address << std::endl);
        return false;
    }

    const Imports &imports = m_binary->getImports();
    Imports::const_iterator iit = imports.find(target);
    if (iit == imports.end()) {
        LOGDEBUG("No imported function at 0x" << std::hex << target << std::endl);
        return false;
    }

    LOGDEBUG("Found function " << (*iit).second.first << "!" <<
            (*iit).second.second << std::endl);

    functionName = (*iit).second.second;
    return true;
}


StoreInst *CallBuilder::getRegisterDefinition(LoadInst *load)
{
    std::set<StoreInst*> definitions;
    std::vector<LoadInst*> toVisit;
    std::set<Instruction*> visited;
    SmallVectorImpl<MemoryDependenceAnalysis::NonLocalDepEntry> Dependencies(2);

    LOGDEBUG("Looking for definition of " << *load << std::endl);

    toVisit.push_back(load);

    while(!toVisit.empty()) {
        load = &*toVisit.back();
        toVisit.pop_back();

        if (visited.count(load)) {
            continue;
        }
        visited.insert(load);

        MemDepResult Dependency = m_memDepAnalysis->getDependency(load);
        if (Dependency.isNonLocal()) {
            Dependencies.clear();
            m_memDepAnalysis->getNonLocalPointerDependency(load->getPointerOperand(), true,
                                                          load->getParent(),
                                                          Dependencies);
        }else {
            Dependencies.push_back(std::make_pair(Dependency.getInst()->getParent(), Dependency));
        }

        //foreach(it, Dependencies.begin(), Dependencies.end()) {
        for (unsigned i=0; i<Dependencies.size(); ++i) {
            MemoryDependenceAnalysis::NonLocalDepEntry &dentry = Dependencies[i];
            MemDepResult Result = dentry.second;
            LOGDEBUG("Dep: " << Result.isClobber() << " - " << Result.isDef()<< " - " << Result.isNonLocal() << std::endl << std::flush);
            LOGDEBUG("Dep: " << dentry.first->getNameStr() << " - " << *Result.getInst() << std::endl);
            if (Result.isDef() != true) {
                LOGDEBUG("   Dependency is not a definition " << std::endl);
                return false;
            }
            if (LoadInst *loadInst = dyn_cast<LoadInst>(Result.getInst())) {
                toVisit.push_back(loadInst);
            }else {
                StoreInst *regDef = dyn_cast<StoreInst>(Result.getInst());
                definitions.insert(regDef);
            }
        }
    }

    if (definitions.size() != 1) {
        //XXX: Check that all definitions are the same values and reachable from all paths
        return NULL;
    }

    //XXX: Check that the size of the load/store matches


    return *definitions.begin();
}

/**
 * mov reg, immediate   ;Normal function call, NOT HANDLED YET
 * call reg
 *
 * mov reg, immediate   ;Import
 * call [reg]
 *
 * mov reg, [immediate] ;XXXX this is not handled
 * call [reg]
 *
 * mov reg, [immediate] ;Import
 * call reg
 */
bool CallBuilder::processIndirectCall(CallInst *marker)
{
    ConstantInt *cste = NULL;
    Value *loadAddress;
    LoadInst *regLoad = NULL;
    unsigned numIndirections = 0;

    CallInst *memLoad = TbPreprocessor::getMemoryLoadFromIndirectCall(marker);
    if (memLoad) {
        //Process call [xxx]
        ++numIndirections;
        loadAddress = TbPreprocessor::getAddressFromMemoryOp(memLoad);
        assert(loadAddress && "Something is broken");
    }else {
        //Process call xxx
        //Can be only a register. Constant immediates have been processed elsewhere.
        loadAddress = TbPreprocessor::getRegisterLoadFromIndirectCall(marker);
        LOGDEBUG("Indirect call: " << *marker << std::endl << std::flush);
        assert(loadAddress && "Something is broken");
    }

    if ((cste = dyn_cast<ConstantInt>(loadAddress))) {
        //The address comes from some section in the executable
    }else {
        //Process call reg / call [reg]
        regLoad = dyn_cast<LoadInst>(loadAddress);
        if (!regLoad) {
            return false;
        }

        LOGDEBUG("Call to value stored in register " << *regLoad << std::endl << std::flush);

        //Find the definition of the register
        StoreInst *regDef = getRegisterDefinition(regLoad);
        if (!regDef) {
            //No definition, multiple different definitions, etc...
            return false;
        }

        //Check that the definition is a constant value
        LOGDEBUG("Found the store: " << *regDef << std::endl);
        cste = dyn_cast<ConstantInt>(regDef->getOperand(0));
        if (!cste) {
            //See if it is a memory load from constant address
            CallInst *load = dyn_cast<CallInst>(regDef->getOperand(0));
            if (!load) {
                return false;
            }
            loadAddress = TbPreprocessor::getAddressFromMemoryOp(load);
            cste = dyn_cast<ConstantInt>(loadAddress);
            ++numIndirections;
        }
    }

    if (!cste || numIndirections != 1) {
        //XXX: handle normal function calls
        return false;
    }

    //Now we have a constant address for the call
    uint64_t address = cste->getZExtValue();
    std::string functionName;
    if (!resolveImport(address, functionName)) {
        return false;
    }

    return true;
}



bool CallBuilder::processCallMarker(Module &M, CallInst *marker)
{
    Value *target = TbPreprocessor::getCallTarget(marker);
    if (ConstantInt *cste = dyn_cast<ConstantInt>(target)) {
        //Verify if it is an internal function call
        Function *f = TbPreprocessor::getFunction(M, cste);
        if (f) {
            return processLocalCall(marker, f);
        }

        //Not an internal function call
        LOGERROR("Could not determine call target for address 0x"
                << std::hex << cste->getZExtValue() << " for call " <<
                *marker << std::endl);
        return false;
    }else {
        return processIndirectCall(marker);
    }
    return false;
}


bool CallBuilder::runOnModule(llvm::Module &M)
{
    unsigned unprocessedCallsCount = 0;
    unsigned processedCallsCount = 0;

    TargetBinary *targetBinary = &getAnalysis<TargetBinary>();
    m_binary = targetBinary->getBinary();

    Function *callMarker = TbPreprocessor::getCallMarker(M);
    assert(callMarker);

    //Gather all the call sites and assign them to functions
    std::map<Function*, DenseSet<CallInst *> > callSites;
    foreach(it, callMarker->use_begin(), callMarker->use_end()) {
        if (CallInst *ci = dyn_cast<CallInst>(*it)) {
            Function *f = ci->getParent()->getParent();
            if (TbPreprocessor::isReconstructedFunction(*f)) {
                callSites[f].insert(ci);
            }
        }
    }

    //Create a function pass manager.
    //We cannot directly specify MemoryDependenciesAnalysis as a prerequesite
    //of CallBuilder because the former is a FunctionPass and the latter a
    //ModulePass. MemoryDependenciesAnalysis needs TargetData and cannot take
    //the one in the PassManager for CallSiteBuilder. TargetData is needed to
    //compute the size of various types.
    FunctionPassManager fpm(targetBinary->getTranslator()->getModuleProvider());
    fpm.add(new TargetData(&M));
    fpm.add(new TargetBinary(m_binary, targetBinary->getTranslator()));
    fpm.add(new CallingConvention());
    fpm.add(new RevgenAlias());
    m_memDepAnalysis = new MemoryDependenceAnalysis();
    fpm.add(m_memDepAnalysis);

    //Process each call site, grouped by function
    foreach(fit, callSites.begin(), callSites.end()) {
        Function *F = (*fit).first;
        const DenseSet<CallInst*> &cs = (*fit).second;

        fpm.run(*F);

        LOGDEBUG("Processing " << *F << std::endl << std::flush);

        foreach(cit, cs.begin(), cs.end()) {
            CallInst *ci = *cit;
            if (!processCallMarker(M, ci)) {
                LOGDEBUG("Could not process " << *ci <<
                        " in function " << ci->getParent()->getParent()->getNameStr()
                        << " bblock " << ci->getParent()->getNameStr()
                        <<std::endl);
                ++unprocessedCallsCount;
            }else {
                ++processedCallsCount;
            }
        }
    }

    if (unprocessedCallsCount > 0) {
        LOGINFO("Processed " << std::dec << processedCallsCount << " calls" << std::endl);
        LOGERROR("Could not process " << std::dec << unprocessedCallsCount
                << " calls" << std::endl);
    }

    return processedCallsCount > 0;
}

}
