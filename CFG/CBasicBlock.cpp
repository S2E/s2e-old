#include <iostream>
#include "Passes/QEMUInstructionBoundaryMarker.h"
#include "Passes/QEMUTerminatorMarker.h"
#include "CBasicBlock.h"
#include "Utils.h"

namespace s2etools {
namespace translator {

CBasicBlock::CBasicBlock(llvm::Function *f, uint64_t va, unsigned size, EBasicBlockType type)
{
    m_address = va;
    m_size = size;
    m_function = f;
    m_type = type;

    if (m_type == BB_DEFAULT) {
        m_successors.insert(m_address + m_size);
    }else {
        markTerminator();
    }

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

void CBasicBlock::markTerminator()
{
    bool isRet = m_type == BB_RET;
    bool isInlinable = m_type == BB_JMP || m_type == BB_COND_JMP || m_type == BB_REP;

    QEMUTerminatorMarker terminatorMarker(isInlinable, isRet);
    if (!terminatorMarker.runOnFunction(*m_function)) {
        std::cerr << "Basic block at address 0x" << std::hex << m_address <<
                " has no terminator markers. This is bad." << std::endl;
    }

    QEMUTerminatorMarker::StaticTargets target = terminatorMarker.getStaticTargets();

    foreach(it, target.begin(), target.end()) {
        uint64_t tpc = *it;
        //Do not add successor if it points back to the same bb.
        if (!(tpc >= m_address && tpc < m_address+m_size)) {
            m_successors.insert(*it);
        }
    }

    m_successors.insert(m_address + m_size);
}


void CBasicBlock::toString(std::ostream &os) const
{
    os << *m_function;
}

}
}
