#include <iostream>
#include "Passes/QEMUInstructionBoundaryMarker.h"
#include "CBasicBlock.h"

namespace s2etools {
namespace translator {

CBasicBlock::CBasicBlock(llvm::Function *f, uint64_t va, unsigned size)
{
    m_address = va;
    m_size = size;
    m_function = f;

    markInstructionBoundaries();
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

}
}
