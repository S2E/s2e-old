#include <iostream>
#include <sstream>

#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/DerivedTypes.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/target/TargetData.h"

#include "llvm/Transforms/Utils/Cloning.h"
#include "Passes/QEMUInstructionBoundaryMarker.h"
#include "Passes/QEMUTerminatorMarker.h"
#include "Passes/QEMUTbCutter.h"
#include "CBasicBlock.h"
#include "Utils.h"

using namespace llvm;

namespace s2etools {
namespace translator {

unsigned CBasicBlock::s_seqNum = 0;

CBasicBlock::CBasicBlock(llvm::Function *f, uint64_t va, unsigned size, EBasicBlockType type)
{
    m_address = va;
    m_size = size;
    m_function = f;
    m_type = type;

    if (m_type == BB_DEFAULT) {
        m_successors.insert(m_address + m_size);
    }

    markTerminator();
    markInstructionBoundaries();
    valid();

}

CBasicBlock::CBasicBlock(uint64_t va, unsigned size)
{
    m_address = va;
    m_size = size;
    m_function = NULL;
    m_type = BB_DEFAULT;
}

CBasicBlock::CBasicBlock()
{
    m_address = 0;
    m_size = 0;
    m_function = NULL;
    m_type = BB_DEFAULT;
}

CBasicBlock::~CBasicBlock()
{
    //TODO: delete the function
}

void CBasicBlock::markInstructionBoundaries()
{
    QEMUInstructionBoundaryMarker marker;
    if (!marker.runOnFunction(*m_function)) {
        std::cerr << "Basic block at address 0x" << std::hex << m_address <<
                " has no instruction markers. This is bad." << std::endl;
        std::cerr << m_function << std::endl;
        assert(false);
    }

    m_instructionMarkers = marker.getMarkers();    
}

void CBasicBlock::markTerminator()
{
    bool isRet = m_type == BB_RET;
    bool isInlinable = m_type == BB_JMP || m_type == BB_COND_JMP || m_type == BB_REP || m_type == BB_DEFAULT;

    QEMUTerminatorMarker terminatorMarker(isInlinable, isRet);
    if (!terminatorMarker.runOnFunction(*m_function)) {
        std::cerr << "Basic block at address 0x" << std::hex << m_address <<
                " has no terminator markers. This is bad." << std::endl;
        std::cerr << m_function << std::endl;
        assert(false);
    }

    //Call targets are not successors
    if (isInlinable) {
        QEMUTerminatorMarker::StaticTargets target = terminatorMarker.getStaticTargets();

        foreach(it, target.begin(), target.end()) {
            uint64_t tpc = *it;
            m_successors.insert(*it);
        }
    }

    if (!isRet && !(m_type == BB_JMP)) {
        m_successors.insert(m_address + m_size);
    }
}

Function* CBasicBlock::cloneFunction()
{
    Module *module = m_function->getParent();

    std::cerr << "Cloning BB 0x" << std::hex << m_address << std::endl;
    if (m_address == 0x10ebe) {
        std::cerr << *m_function;
    }

    std::stringstream fcnName;
    fcnName << "bb_" << std::hex << m_address << "_" << std::dec << s_seqNum;
    ++s_seqNum;

    module->getOrInsertFunction(fcnName.str(), m_function->getFunctionType());
    Function *destFcn = module->getFunction(fcnName.str());

    if (destFcn->begin() != destFcn->end()) {
        assert(false && "Function already exists");
    }

    DenseMap<const Value*, Value*> ValueMap;
    std::vector<ReturnInst*> Returns;

    ValueMap[m_function->arg_begin()] = destFcn->arg_begin();
    CloneFunctionInto(destFcn, m_function, ValueMap, Returns);

    assert(m_function->getParent() == destFcn->getParent());

    return destFcn;
}


/**
 *  Returns the first half of the basic block.
 *  va will belong to the second half
 */
CBasicBlock* CBasicBlock::split(uint64_t va)
{
    if (m_instructionMarkers.size() == 1) {
        //Can't split a basic block that has only one instruction
        std::cerr << "Basic block 0x"  << std::hex << m_address << " has only one instruction." << std::endl;
        return NULL;
    }
    
    if (m_instructionMarkers.find(va) == m_instructionMarkers.end()) {
        //Requested to split at an address that does not belong to us
        std::cerr << "Address 0x" << std::hex << va << " does not belong to basic block 0x"<< m_address << std::endl;
        return NULL;
    }

    unsigned newSize = va - m_address;

    CBasicBlock *ret = new CBasicBlock();
    QEMUInstructionBoundaryMarker insMarker(true);

    //Clone the LLVM function
    Function *retf = cloneFunction();
    insMarker.runOnFunction(*retf);
    ret->m_instructionMarkers = insMarker.getMarkers();

    insMarker.runOnFunction(*m_function);
    m_instructionMarkers = insMarker.getMarkers();


    QEMUTbCutter tbCutter(ret->m_instructionMarkers[va], false);
    if (!tbCutter.runOnFunction(*retf)) {
        std::cerr << "Could not cut function " << retf->getNameStr() << " at 0x" << std::hex << va << std::endl;
        delete ret;
        return NULL;
    }
    ret->m_function = retf;
    ret->m_successors.insert(m_address + newSize);
    ret->m_type = BB_DEFAULT;
    ret->m_address = m_address;
    ret->m_size = newSize;
    insMarker.runOnFunction(*retf);
    ret->m_instructionMarkers = insMarker.getMarkers();

    assert(ret->m_instructionMarkers.size() > 0);


    //Shorten the current function by removing instructions at the start
    QEMUTbCutter tbShortener(m_instructionMarkers[va], true);
    if (!tbShortener.runOnFunction(*m_function)) {
        std::cerr << "Could not cut function " << m_function->getNameStr() << " at 0x" << std::hex << va << std::endl;
        delete ret;
        return NULL;
    }
    m_address += newSize;
    m_size -= newSize;

    insMarker.runOnFunction(*m_function);
    m_instructionMarkers = insMarker.getMarkers();
    assert(m_instructionMarkers.size() > 0);

    valid();
    ret->valid();
    return ret;
}

void CBasicBlock::toString(std::ostream &os) const
{
    os << "BB Start=0x" << std::hex << m_address << " size=0x" << m_size <<
            " type=" << m_type << std::endl;
    os << *m_function;
}

/**
 * Returns the list of program counters common to both basic blocks
 */
void CBasicBlock::intersect(CBasicBlock *bb1, AddressSet &result) const
{
    AddressSet a1, a2;
    foreach(it, m_instructionMarkers.begin(), m_instructionMarkers.end()) {
        a1.insert((*it).first);
    }

    foreach(it, bb1->m_instructionMarkers.begin(), bb1->m_instructionMarkers.end()) {
        a2.insert((*it).first);
    }

    std::set_intersection(a1.begin(), a1.end(), a2.begin(), a2.end(),
                          std::inserter(result, result.begin()));
}

bool CBasicBlock::valid() const
{
    //Check that the basic block is well-formed LLVM
    ExistingModuleProvider *MP = new ExistingModuleProvider(m_function->getParent());
    TargetData *TD = new TargetData(m_function->getParent());
    FunctionPassManager FcnPasses(MP);
    FcnPasses.add(TD);

    FcnPasses.add(createVerifierPass());


    foreach(it, m_instructionMarkers.begin(), m_instructionMarkers.end()) {
        uint64_t marker = (*it).first;
        if (marker < m_address || marker >= m_address + m_size) {
            std::cerr << "BUGGY FUNCTION " << std::endl;
            toString(std::cerr);
            assert(false && "BB has marker outside its bounds");
        }
    }
    return true;
}

}
}
