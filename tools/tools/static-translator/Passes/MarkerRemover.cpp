#include <llvm/Instruction.h>
#include "MarkerRemover.h"
#include "lib/X86Translator/TbPreprocessor.h"

using namespace llvm;

namespace s2etools {

LogKey MarkerRemover::TAG = LogKey("MarkerRemover");
char MarkerRemover::ID = 0;

//Removes void markers
bool MarkerRemover::removeMarker(llvm::Function *marker)
{
    assert(marker->getReturnType() == Type::getVoidTy(marker->getParent()->getContext()));

    bool modified = false;
    foreach(it, marker->use_begin(), marker->use_end()) {
        CallInst *ci = dyn_cast<CallInst>(*it);
        if (!ci) {
            continue;
        }

        ci->eraseFromParent();
        modified = true;
    }
    return modified;
}

bool MarkerRemover::transformLoads(Module &M)
{
    bool modified = false;
    for (unsigned i=0; i<4; ++i) {
        unsigned size = 1 << i;
        Function *ld = TbPreprocessor::getMemoryLoad(M, i);

        foreach(uit, ld->use_begin(), ld->use_end()) {
            CallInst *ci = dyn_cast<CallInst>(*uit);
            if (!ci) {
                continue;
            }
            Value *address = TbPreprocessor::getAddressFromMemoryOp(ci);

            const Type *accessType = IntegerType::get(M.getContext(), size * 8);
            const Type *ptrAccessType = PointerType::getUnqual(accessType);

            Value *addressPtr = new IntToPtrInst(address, ptrAccessType, "", ci);
            LoadInst *load = new LoadInst(addressPtr, "", ci);
            ci->replaceAllUsesWith(load);
            ci->eraseFromParent();
            modified = true;
        }
    }
    return modified;
}

bool MarkerRemover::transformStores(llvm::Module &M)
{
    bool modified = false;
    for (unsigned i=0; i<4; ++i) {
        unsigned size = 1 << i;
        Function *ld = TbPreprocessor::getMemoryStore(M, i);

        foreach(uit, ld->use_begin(), ld->use_end()) {
            CallInst *ci = dyn_cast<CallInst>(*uit);
            if (!ci) {
                continue;
            }
            Value *address = TbPreprocessor::getAddressFromMemoryOp(ci);
            Value *value = TbPreprocessor::getValueFromMemoryOp(ci);

            const Type *accessType = IntegerType::get(M.getContext(), size * 8);
            const Type *ptrAccessType = PointerType::getUnqual(accessType);

            Value *addressPtr = new IntToPtrInst(address, ptrAccessType, "", ci);
            new StoreInst(value, addressPtr,ci);
            ci->eraseFromParent();
            modified = true;
        }
    }
    return modified;
}

bool MarkerRemover::runOnModule(llvm::Module &M)
{
    bool modified = false;

    //Initialize markers
    m_callMarker = TbPreprocessor::getCallMarker(M);
    m_jumpMarker = TbPreprocessor::getJumpMarker(M);
    m_instructionMarker = TbPreprocessor::getInstructionMarker(M);
    m_returnMarker = TbPreprocessor::getReturnMarker(M);

    modified |= removeMarker(m_returnMarker);
    modified |= removeMarker(m_instructionMarker);

    //XXX: inline assembly removal should not have those in the resulting code
    if (!m_jumpMarker->use_empty()) {
        LOGERROR("Jump marker must not be present in the code");
    }

    if (!m_callMarker->use_empty()) {
        LOGERROR("Call marker must not be present in the code");
    }

    //Transform all memory accesses to native LLVM loads/stores
    modified |= transformLoads(M);
    modified |= transformStores(M);

    return modified;

}


}
