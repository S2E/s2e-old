extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include "lib/X86Translator/CpuStatePatcher.h"
#include "CallingConvention.h"

using namespace llvm;

namespace s2etools {

char CallingConvention::ID = 0;

static RegisterPass<CallingConvention> X("callingconvention",
                                     "Provides calling convention information", false, false);


//XXX: Assumes x86 architecture
CallingConvention::Convention CallingConvention::getConvention(const GetElementPtrInst *reg) const
{
    unsigned regNum;

    if (!CpuStatePatcher::getRegisterIndex(reg, regNum)) {
        return Unknown;
    }

    if (regNum == R_EAX || regNum == R_ECX || regNum == R_EDX) {
        return CallerSave;
    }
    return CalleeSave;
}

}
