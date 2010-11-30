#include <iostream>
#include "Passes/QEMUInstructionBoundaryMarker.h"
#include "Passes/QEMUCallMarker.h"
#include "CBasicBlock.h"

namespace s2etools {
namespace translator {

CBasicBlock::CBasicBlock(llvm::Function *f, uint64_t va, unsigned size, EBasicBlockType type)
{
    m_address = va;
    m_size = size;
    m_function = f;
    m_type = type;

    markInstructionBoundaries();

    if (m_type == BB_CALL || m_type == BB_CALL_IND) {
        markCallInstruction();
    }
}

CBasicBlock::~CBasicBlock()
{

}

void CBasicBlock::markInstructionBoundaries()
{
    QEMUInstructionBoundaryMarker marker;
    if (!marker.runOnFunction(*m_function)) {
        std::cerr << "Basic block at address 0x" << std::hex << m_address <<
                " has no instruction markers. This is bad." << std::endl;
    }
}

void CBasicBlock::markCallInstruction()
{
    assert (m_type == BB_CALL || m_type == BB_CALL_IND);
    QEMUCallMarker callMarker;
    if (!callMarker.runOnFunction(*m_function)) {
        std::cerr << "Basic block at address 0x" << std::hex << m_address <<
                " has no call markers. This is bad." << std::endl;
    }
}

void CBasicBlock::computeSuccessors()
{
    switch(m_type) {
        //The BB has no terminator, return the address that follows it
        case BB_DEFAULT:
            m_successors.push_back(m_address + m_size);
            break;

        case BB_CALL:
            //Simplest case: successor
            m_successors.push_back(m_address + m_size);

            //Run analysis pass to get the call target
            break;

    default:
            assert(false && "Not implemented");

    }
}

}
}
