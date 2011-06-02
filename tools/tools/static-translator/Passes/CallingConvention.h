#ifndef REVGEN_CALLING_CONVENTION_H

#define REVGEN_CALLING_CONVENTION_H

#include <llvm/Pass.h>
#include <llvm/Instructions.h>

#include "lib/X86Translator/Translator.h"

namespace s2etools {
class CallingConvention : public llvm::ImmutablePass {
private:

public:
    enum Convention {
        CallerSave, CalleeSave, Unknown
    };

    static char ID;

    CallingConvention() : ImmutablePass(&ID) {
    }


    Convention getConvention(const llvm::GetElementPtrInst *reg) const;


};
}

#endif
