#include "TargetBinary.h"

using namespace llvm;

namespace s2etools {

char TargetBinary::ID = 0;

static RegisterPass<TargetBinary> X("targetbinary",
                                     "Wrapper pass for binary interface", false, false);


}
