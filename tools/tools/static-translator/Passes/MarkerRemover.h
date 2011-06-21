#ifndef _ASM_MARKER_REMOVER_PASS_H_

#define _MARKER_REMOVER_PASS_H_

#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/ADT/DenseSet.h>

#include <lib/Utils/Log.h>

namespace s2etools {

/**
 * Removes all marker_* calls and transforms memory
 * memory operations to native LLVM load/stores
 */
class MarkerRemover: public llvm::ModulePass {
private:
    static LogKey TAG;
    llvm::Function *m_callMarker;
    llvm::Function *m_instructionMarker;
    llvm::Function *m_returnMarker;
    llvm::Function *m_jumpMarker;

    bool removeMarker(llvm::Function *marker);
    bool transformLoads(llvm::Module &M);
    bool transformStores(llvm::Module &M);

public:
    static char ID;

    MarkerRemover() : ModulePass(&ID){

    }

    bool runOnModule(llvm::Module &M);
};

}

#endif
