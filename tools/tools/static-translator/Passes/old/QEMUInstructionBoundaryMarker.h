#ifndef _INSTR_BOUND_MARK_PASS_H_

#define _INSTR_BOUND_MARK_PASS_H_

#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>

#include <map>

/**
 *  This pass inserts a marker in the LLVM bitcode between blocks of
 *  LLVM instructions belonging to the same original x86 machine instruction.
 */
struct QEMUInstructionBoundaryMarker : public llvm::FunctionPass {
    static char ID;
    QEMUInstructionBoundaryMarker() : FunctionPass((intptr_t)&ID) {
        m_instructionMarker = NULL;
        m_analyze = false;
    }

    QEMUInstructionBoundaryMarker(bool analyzeOnly) : FunctionPass((intptr_t)&ID) {
        m_instructionMarker = NULL;
        m_analyze = analyzeOnly;
    }

public:
    typedef std::map<uint64_t, llvm::CallInst*> Markers;

private:
    static std::string TAG;
    llvm::Function *m_instructionMarker;
    Markers m_markers;
    bool m_analyze;

    void initInstructionMarker(llvm::Module *module);
    void markBoundary(llvm::CallInst *Ci);
    void updateMarker(llvm::CallInst *Ci);
    void moveAllocInstruction(llvm::AllocaInst *inst);
    bool duplicatePrefixInstructions(llvm::Function &F);
public:
    virtual bool runOnFunction(llvm::Function &F);

    const Markers &getMarkers() const {
        return m_markers;
    }

};


#endif
