#ifndef REVGEN_ALIAS_ANALYSIS_H

#define REVGEN_ALIAS_ANALYSIS_H

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/ADT/DenseSet.h>

#include "lib/Utils/Log.h"
#include "CallingConvention.h"

namespace s2etools {

class RevgenAlias : public llvm::FunctionPass, public llvm::AliasAnalysis {
    static LogKey TAG;
    typedef llvm::DenseSet<llvm::Function*> Functions;

    Functions m_memoryAccessors;
    llvm::Function *m_callMarker;
    llvm::Function *m_instructionMarker;
    const llvm::PointerType* m_cpuStateType;

    CallingConvention *m_callingConvention;

    void initializeMemoryAccessors(llvm::Module &M);
    void initializeCallingConvention(llvm::Module &M);
public:
    static char ID;

    RevgenAlias();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual llvm::AliasAnalysis::AliasResult
            alias(const llvm::Value *V1, unsigned V1Size,
                                     const llvm::Value *V2, unsigned V2Size);

    virtual ModRefResult getModRefInfo(llvm::CallSite CS, llvm::Value *P, unsigned Size);

    virtual ModRefResult getModRefInfo(llvm::CallSite CS1, llvm::CallSite CS2) {
        return AliasAnalysis::getModRefInfo(CS1, CS2);
    }

    bool runOnFunction(llvm::Function &F);
};

}

#endif
