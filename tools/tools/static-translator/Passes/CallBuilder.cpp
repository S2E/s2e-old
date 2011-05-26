#include "lib/Utils/Log.h"
#include "lib/Utils/Utils.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "CallBuilder.h"

using namespace llvm;

namespace s2etools {

char CallBuilder::ID = 0;
LogKey CallBuilder::TAG = LogKey("CallBuilder");

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
        LOGDEBUG() << "Could not read import entry 0x" << std::hex << address << std::endl;
        return false;
    }

    const Imports &imports = m_binary->getImports();
    Imports::const_iterator iit = imports.find(target);
    if (iit == imports.end()) {
        LOGDEBUG() << "No imported function at 0x" << std::hex << target << std::endl;
        return false;
    }

    LOGDEBUG() << "Found function " << (*iit).second.first << "!" <<
            (*iit).second.second << std::endl;

    functionName = (*iit).second.second;
    return true;
}


/**
 * call [ds:immediate_value] ;Usually for import tables
 * call register ;Figure out whether the register is constant
 */
bool CallBuilder::processIndirectCall(CallInst *marker)
{
    CallInst *memLoad = TbPreprocessor::getMemoryLoadFromIndirectCall(marker);
    if (!memLoad) {
        return false;
    }

    Value *v = TbPreprocessor::getAddressFromMemoryOp(memLoad);
    assert(v && "Something is broken");

    if (ConstantInt *cste = dyn_cast<ConstantInt>(v)) {
        uint64_t address = cste->getZExtValue();
        std::string functionName;
        if (!resolveImport(address, functionName)) {
            return false;
        }
        return true;
    }else {
        //Check if this is a register access
    }
    return false;
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
        LOGERROR() << "Could not determine call target for address 0x"
                << std::hex << cste->getZExtValue() << " for call " <<
                *marker << std::endl;
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

    Function *callMarker = TbPreprocessor::getCallMarker(M);
    assert(callMarker);

    Value::use_iterator it = callMarker->use_begin();
    while(it != callMarker->use_end()) {
        CallInst *ci = dyn_cast<CallInst>(*it);
        ++it;   //Increment now, iterator can be invalidated later
        if (!ci) {
            continue;
        }


        if (!processCallMarker(M, ci)) {
            LOGDEBUG() << "Could not process " << *ci <<
                    " in function " << ci->getParent()->getParent()->getNameStr()
                    <<std::endl;
            ++unprocessedCallsCount;
        }

        ++processedCallsCount;
    }

    if (unprocessedCallsCount > 0) {
        LOGINFO() << "Processed " << std::dec << processedCallsCount << " calls" << std::endl;
        LOGERROR() << "Could not process " << std::dec << unprocessedCallsCount
                << " calls" << std::endl;
    }

    return processedCallsCount > 0;
}

}
