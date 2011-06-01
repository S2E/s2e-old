#include <llvm/Instructions.h>

#include "lib/X86Translator/CpuStatePatcher.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "RevgenAlias.h"

using namespace llvm;

namespace s2etools {

LogKey RevgenAlias::TAG = LogKey("RevgenAlias");
char RevgenAlias::ID = 0;
static RegisterPass<RevgenAlias>
    X("revgenaa", "Revgen Alias Analysis", false, true);

static RegisterAnalysisGroup<AliasAnalysis> Y(X);

RevgenAlias::RevgenAlias() : FunctionPass(&ID){
}

void RevgenAlias::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.setPreservesCFG();
    AU.addRequired<AliasAnalysis>();
}


AliasAnalysis::AliasResult RevgenAlias::alias(const Value *V1, unsigned V1Size,
                                 const Value *V2, unsigned V2Size) {
    // Couldn't determine a must or no-alias result.
    return AliasAnalysis::alias(V1, V1Size, V2, V2Size);
}

void RevgenAlias::initializeMemoryAccessors(Module &M)
{
    m_cpuStateType = CpuStatePatcher::getCpuStateType(M)->getPointerTo(0);
    m_memoryAccessors.insert(M.getFunction("__ldb_mmu"));
    m_memoryAccessors.insert(M.getFunction("__ldw_mmu"));
    m_memoryAccessors.insert(M.getFunction("__ldl_mmu"));
    m_memoryAccessors.insert(M.getFunction("__ldq_mmu"));

    m_memoryAccessors.insert(M.getFunction("__stb_mmu"));
    m_memoryAccessors.insert(M.getFunction("__stw_mmu"));
    m_memoryAccessors.insert(M.getFunction("__stl_mmu"));
    m_memoryAccessors.insert(M.getFunction("__stq_mmu"));

    m_memoryAccessors.insert(M.getFunction(TbPreprocessor::getInstructionMarker()));

    assert(m_memoryAccessors.size() == 9 && "Could not find all ops. Your ucode lib is probably broken");
}

AliasAnalysis::ModRefResult RevgenAlias::getModRefInfo(CallSite CS, Value *P, unsigned Size)
{
    //If we call something else than machine uops, there may be anything
    if (!m_memoryAccessors.count(CS.getCalledFunction())) {
        return AliasAnalysis::getModRefInfo(CS, P, Size);
    }

    //If P is a pointer to the CPU state, there is no way there can
    //be aliasing with a memory uop
    GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(P);
    if (!gep) {
        return AliasAnalysis::getModRefInfo(CS, P, Size);
    }

    Function *f = gep->getParent()->getParent();
    //LOGDEBUG(f->getNameStr() << std::endl);

    Value *cpuState = CpuStatePatcher::getCpuStateParam(*f);
    //LOGDEBUG(*cpuState->getType() << std::endl << std::flush);
    assert(cpuState->getType() == m_cpuStateType && "Broken translator");

    if (gep->getPointerOperand() == cpuState) {
        return NoModRef;
    }

    return AliasAnalysis::getModRefInfo(CS, P, Size);
}


bool RevgenAlias::runOnFunction(Function &F)
{
    InitializeAliasAnalysis(this);
    initializeMemoryAccessors(*F.getParent());
    return false;
}


}
