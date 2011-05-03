#include <llvm/Function.h>
#include <llvm/BasicBlock.h>

#include "ConstantExtractor.h"
#include "Utils.h"

#include <set>

using namespace llvm;

char ConstantExtractor::ID = 0;
RegisterPass<ConstantExtractor>
  ConstantExtractor("ConstantExtractor", "Extracts all constants from a function",
  true /* Only looks at CFG */,
  true /* Analysis Pass */);


bool ConstantExtractor::runOnFunction(llvm::Function &F)
{
    m_constants.clear();
    foreach(bbit, F.begin(), F.end()) {
        const BasicBlock &bb = *bbit;
        foreach(iit, bb.begin(), bb.end()) {
            const Instruction &instr = *iit;

            //Look at all operands and take the constant ints
            for (unsigned i=0; i<instr.getNumOperands(); ++i) {
                if (ConstantInt *cste = dyn_cast<ConstantInt>(instr.getOperand(i))) {
                    const uint64_t* c = cste->getValue().getRawData();
                    m_constants.insert(*c);
                }
            }
        }
    }

    return false;
}
