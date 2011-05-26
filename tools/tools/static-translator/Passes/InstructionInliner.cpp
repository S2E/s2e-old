#include "lib/Utils/Utils.h"
#include "lib/X86Translator/TbPreprocessor.h"
#include "InstructionInliner.h"
#include "ForcedInliner.h"


using namespace llvm;

namespace s2etools {

char InstructionInliner::ID = 0;
LogKey InstructionInliner::TAG = LogKey("InstructionInliner");


bool InstructionInliner::runOnModule(llvm::Module &M)
{

    foreach(it, M.begin(), M.end()) {
        Function *F = &*it;
        if (!TbPreprocessor::isReconstructedFunction(*F)) {
            continue;
        }

        ForcedInliner inliner;
        inliner.addFunction(F);
        inliner.inlineFunctions();
    }
    return true;
}

}
