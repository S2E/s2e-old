#include <llvm/Instructions.h>

#include "lib/X86Translator/CpuStatePatcher.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "RevgenAlias.h"
#include "CallingConvention.h"

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

    assert(m_memoryAccessors.size() == 8 && "Could not find all ops. Your ucode lib is probably broken");
}

AliasAnalysis::ModRefResult RevgenAlias::getModRefInfo(CallSite CS, Value *P, unsigned Size)
{
    GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(P);
    if (!gep) {
        return AliasAnalysis::getModRefInfo(CS, P, Size);
    }

    Function *f = gep->getParent()->getParent();
    //LOGDEBUG(f->getNameStr() << std::endl);

    Value *cpuState = CpuStatePatcher::getCpuStateParam(*f);
    //LOGDEBUG(*cpuState->getType() << std::endl << std::flush);
    assert(cpuState->getType() == m_cpuStateType && "Broken translator");

    if (gep->getPointerOperand() != cpuState) {
        return AliasAnalysis::getModRefInfo(CS, P, Size);
    }

    //A pointer to the CPU state can never alias memory
    if (m_memoryAccessors.count(CS.getCalledFunction())) {
        return NoModRef;
    }

    //Instruction markers are just here for decoration
    if (CS.getCalledFunction() == m_instructionMarker) {
        return NoModRef;
    }

    if (CS.getCalledFunction() == m_callMarker) {
        //Check if the register may be clobbered by the called function,
        //assuming the specified convention
        //XXX: This may give erroneous results if used on internal calls
        //XXX: Check for library call first?
        if (m_callingConvention) {
            if (m_callingConvention->getConvention(gep) == CallingConvention::CalleeSave) {
                //XXX: Use NoModRef instead???
                return Ref;
            }
        }
    }

    LOGDEBUG("Trying default AA for " << *P << std::endl);
    //Try the default alias analyzer
    return AliasAnalysis::getModRefInfo(CS, P, Size);
}

void RevgenAlias::initializeCallingConvention(Module &M)
{
    m_callingConvention = getAnalysisIfAvailable<CallingConvention>();
    if (!m_callingConvention) {
        LOGWARNING("Calling convention not specified. This may give inaccurate results.");
    }

    m_callMarker = TbPreprocessor::getCallMarker(M);
    m_instructionMarker = TbPreprocessor::getInstructionMarker(M);

}

bool RevgenAlias::runOnFunction(Function &F)
{
    InitializeAliasAnalysis(this);
    initializeMemoryAccessors(*F.getParent());
    initializeCallingConvention(*F.getParent());
    return false;
}


}
