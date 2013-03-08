/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

extern "C" {
#include "config.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "cpus.h"
#include "helper.h"

#include "tcg-llvm.h"
#include "cpu.h"


extern CPUArchState *env;

#if S2E_RAM_OBJECT_BITS > TARGET_PAGE_BITS
#pragma message ( "S2E_RAM_OBJECT_BITS: " STRING(S2E_RAM_OBJECT_BITS) )
#pragma message ( "TARGET_PAGE_BITS: " STRING(TARGET_PAGE_BITS) )
#error S2E_RAM_OBJECTS should be smaller (or equal to) TARGET_PAGE_BITS
#endif

}

#include "S2EExecutionState.h"
#include <s2e/s2e_config.h>
#include <s2e/S2EDeviceState.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Plugin.h>
#include <s2e/Utils.h>

#include <klee/Context.h>
#include <klee/Memory.h>
#include <klee/Solver.h>
#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include <llvm/Support/CommandLine.h>

#include <iomanip>
#include <sstream>

//XXX: The idea is to avoid function calls
//#define small_memcpy(dest, source, count) asm volatile ("cld; rep movsb"::"S"(source), "D"(dest), "c" (count):"flags", "memory")
#define small_memcpy __builtin_memcpy

namespace klee {
extern llvm::cl::opt<bool> DebugLogStateMerge;
}

namespace {
CPUTLBEntry s_cputlb_empty_entry = { -1, -1, -1, -1 };
}

extern llvm::cl::opt<bool> PrintModeSwitch;
extern llvm::cl::opt<bool> PrintForkingStatus;
extern llvm::cl::opt<bool> ConcolicMode;
extern llvm::cl::opt<bool> VerboseStateDeletion;
extern llvm::cl::opt<bool> DebugConstraints;

namespace s2e {

using namespace klee;

MemoryObject *S2EExecutionState::m_cpuRegistersState = NULL;
MemoryObject *S2EExecutionState::m_cpuSystemState = NULL;
MemoryObject *S2EExecutionState::m_dirtyMask = NULL;

unsigned S2EExecutionState::s_lastSymbolicId = 0;

S2EExecutionState::S2EExecutionState(klee::KFunction *kf) :
        klee::ExecutionState(kf), m_stateID(g_s2e->fetchAndIncrementStateId()),
        m_symbexEnabled(true), m_startSymbexAtPC((uint64_t) -1),
        m_active(true), m_zombie(false), m_yielded(false), m_runningConcrete(true),
        m_cpuRegistersObject(NULL), m_cpuSystemObject(NULL),
        m_deviceState(this),
        m_qemuIcount(0),
        m_lastS2ETb(NULL),
        m_lastMergeICount((uint64_t)-1),
        m_needFinalizeTBExec(false), m_nextSymbVarId(0), m_runningExceptionEmulationCode(false)
{
    //XXX: make this a struct, not a pointer...
    m_timersState = new TimersState;
    m_dirtyMaskObject = NULL;
}

S2EExecutionState::~S2EExecutionState()
{
    assert(m_lastS2ETb == NULL);

    PluginStateMap::iterator it;

    if (VerboseStateDeletion) {
        g_s2e->getDebugStream() << "Deleting state " << m_stateID << " " << this << '\n';
    }

    //print_stacktrace();

    for(it = m_PluginState.begin(); it != m_PluginState.end(); ++it) {
        delete it->second;
    }

    g_s2e->refreshPlugins();

    //XXX: This cannot be done, as device states may refer to each other
    //delete m_deviceState;

    delete m_timersState;
}


void S2EExecutionState::enableSymbolicExecution()
{
    if (m_symbexEnabled) {
        return;
    }

    m_symbexEnabled = true;

    g_s2e->getMessagesStream(this) << "Enabled symbex"
            << " at pc = " << (void*) getPc()
            << " and pid = " << hexval(getPid()) << '\n';
}

void S2EExecutionState::disableSymbolicExecution()
{
    if (!m_symbexEnabled) {
        return;
    }

    m_symbexEnabled = false;

    g_s2e->getMessagesStream(this) << "Disabled symbex"
            << " at pc = " << (void*) getPc()
            << " and pid = " << hexval(getPid()) << '\n';
}

void S2EExecutionState::enableForking()
{
    if (!forkDisabled) {
        return;
    }

    forkDisabled = false;

    if (PrintForkingStatus) {
        g_s2e->getMessagesStream(this) << "Enabled forking"
                << " at pc = " << (void*) getPc()
                << " and pid = " << hexval(getPid()) << '\n';
    }
}

void S2EExecutionState::disableForking()
{
    if (forkDisabled) {
        return;
    }

    forkDisabled = true;

    if (PrintForkingStatus) {
        g_s2e->getMessagesStream(this) << "Disabled forking"
                << " at pc = " << (void*) getPc()
                << " and pid = " << hexval(getPid()) << '\n';
    }
}


void S2EExecutionState::addressSpaceChange(const klee::MemoryObject *mo,
                        const klee::ObjectState *oldState,
                        klee::ObjectState *newState)
{
#ifdef S2E_ENABLE_S2E_TLB
    if(mo->size == S2E_RAM_OBJECT_SIZE && oldState) {
        assert(m_cpuSystemState && m_cpuSystemObject);


        CPUArchState* cpu;
        cpu = m_active ?
                (CPUArchState*)(m_cpuSystemState->address
                              - CPU_CONC_LIMIT) :
                (CPUArchState*)(m_cpuSystemObject->getConcreteStore(true)
                              - CPU_CONC_LIMIT);


#ifdef S2E_DEBUG_TLBCACHE
        g_s2e->getDebugStream(this) << std::dec << "Replacing " << oldState << " by " << newState <<  "\n";
        g_s2e->getDebugStream(this) << "tlb map size=" << m_tlbMap.size() << '\n';
#endif

        TlbMap::iterator it = m_tlbMap.find(const_cast<ObjectState*>(oldState));
        bool found = false;
        if (it != m_tlbMap.end()) {
            found = true;
            ObjectStateTlbReferences vec = (*it).second;
            unsigned size = vec.size();
            assert(size > 0);
            for (unsigned i = 0; i < size; ++i) {
                const TlbCoordinates &coords = vec[i];
#ifdef S2E_DEBUG_TLBCACHE
                g_s2e->getDebugStream() << "  mmu_idx=" << coords.first<<
                                           " index=" << coords.second << "\n";
#endif
                S2ETLBEntry *entry = &cpu->s2e_tlb_table[coords.first][coords.second];
                assert(entry->objectState == (void*) oldState);
                assert(newState);
                entry->objectState = newState;

                if(!mo->isSharedConcrete) {
                    entry->addend =
                            (entry->addend & ~1)
                            - (uintptr_t) oldState->getConcreteStore(true)
                            + (uintptr_t) newState->getConcreteStore(true);
                    if(addressSpace.isOwnedByUs(newState))
                        entry->addend |= 1;
                }
            }

            m_tlbMap[newState] = vec;
            m_tlbMap.erase(const_cast<ObjectState*>(oldState));
        }

#ifdef S2E_DEBUG_TLBCACHE
        for(unsigned i=0; i<NB_MMU_MODES; ++i) {
            for(unsigned j=0; j<CPU_S2E_TLB_SIZE; ++j) {
                if (cpu->s2e_tlb_table[i][j].objectState == oldState) {
                    assert(found);
                }
                assert(cpu->s2e_tlb_table[i][j].objectState != oldState);
            }
        }
#endif
    }
#endif

    if (mo == m_cpuRegistersState) {
        //It may happen that an execution state is copied in other places
        //than fork, in which case clone() is not called and the state
        //is left with stale references to memory objects. We patch these
        //objects here.
        m_cpuRegistersObject = newState;
    } else if (mo == m_cpuSystemState) {
        m_cpuSystemObject = newState;
    } else {
        ObjectPair op = m_memcache.get(mo->address);
        if (op.first) {
            op.second = newState;
            m_memcache.put(mo->address, op);
        }
    }
}

ExecutionState* S2EExecutionState::clone()
{
    // When cloning, all ObjectState becomes not owned by neither of states
    // This means that we must clean owned-by-us flag in S2E TLB
    assert(m_active && m_cpuSystemState);
#ifdef S2E_ENABLE_S2E_TLB
    CPUArchState* cpu = (CPUArchState*)(m_cpuSystemState->address
                          - CPU_CONC_LIMIT);


    foreach2(it, m_tlbMap.begin(), m_tlbMap.end()) {
        ObjectStateTlbReferences &vec = (*it).second;
        unsigned size = vec.size();
        for (unsigned i = 0; i < size; ++i) {
            const TlbCoordinates &coords = vec[i];
            S2ETLBEntry *entry = &cpu->s2e_tlb_table[coords.first][coords.second];
            ObjectState* os = static_cast<ObjectState*>(entry->objectState);
            if(os && !os->getObject()->isSharedConcrete) {
                entry->addend &= ~1;
            }
        }
    }
#endif

    S2EExecutionState *ret = new S2EExecutionState(*this);
    ret->addressSpace.state = ret;
    ret->m_deviceState.setExecutionState(ret);

    if(m_lastS2ETb)
        m_lastS2ETb->refCount += 1;

    ret->m_stateID = g_s2e->fetchAndIncrementStateId();

    ret->m_timersState = new TimersState;
    *ret->m_timersState = *m_timersState;

    // Clone the plugins
    PluginStateMap::iterator it;
    ret->m_PluginState.clear();
    for(it = m_PluginState.begin(); it != m_PluginState.end(); ++it) {
        ret->m_PluginState.insert(std::make_pair((*it).first, (*it).second->clone()));
    }

    // This objects are not in TLB and won't cause any changes to it
    ret->m_cpuRegistersObject = ret->addressSpace.getWriteable(
                            m_cpuRegistersState, m_cpuRegistersObject);
    ret->m_cpuSystemObject = ret->addressSpace.getWriteable(
                            m_cpuSystemState, m_cpuSystemObject);

    m_cpuRegistersObject = addressSpace.getWriteable(
                            m_cpuRegistersState, m_cpuRegistersObject);
    m_cpuSystemObject = addressSpace.getWriteable(
                            m_cpuSystemState, m_cpuSystemObject);

    ret->m_dirtyMaskObject = ret->addressSpace.getWriteable(
            m_dirtyMask, m_dirtyMaskObject);

    m_dirtyMaskObject = addressSpace.getWriteable(
            m_dirtyMask, m_dirtyMaskObject);

    return ret;
}

ref<Expr> S2EExecutionState::readCpuRegister(unsigned offset,
                                             Expr::Width width) const
{
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset + Expr::getMinBytesForWidth(width) <= CPU_CONC_LIMIT);

    if(!m_runningConcrete || !m_cpuRegistersObject->isConcrete(offset, width)) {
        return m_cpuRegistersObject->read(offset, width);
    } else {
        /* XXX: should we check getSymbolicRegisterMask ? */
        uint64_t ret = 0;
        small_memcpy((void*) &ret, (void*) (m_cpuRegistersState->address + offset),
                       Expr::getMinBytesForWidth(width));
        return ConstantExpr::create(ret, width);
    }
}

void S2EExecutionState::writeCpuRegister(unsigned offset,
                                         klee::ref<klee::Expr> value)
{
    unsigned width = value->getWidth();
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset + Expr::getMinBytesForWidth(width) <= CPU_CONC_LIMIT);


    if(!m_runningConcrete || !m_cpuRegistersObject->isConcrete(offset, width)) {
        m_cpuRegistersObject->write(offset, value);

    } else {
        /* XXX: should we check getSymbolicRegisterMask ? */
        /* XXX: why don't we allow writing symbolic values here ??? */
        assert(isa<ConstantExpr>(value) &&
               "Can not write symbolic values to registers while executing"
               " in concrete mode. TODO: fix it by s2e_longjmping to main loop");
        ConstantExpr* ce = cast<ConstantExpr>(value);
        uint64_t v = ce->getZExtValue(64);
        small_memcpy((void*) (m_cpuRegistersState->address + offset), (void*) &v,
                    Expr::getMinBytesForWidth(ce->getWidth()));
    }
}

void S2EExecutionState::writeCpuRegisterSymbolic(unsigned offset,
                                         klee::ref<klee::Expr> value)
{
    unsigned width = value->getWidth();
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset + Expr::getMinBytesForWidth(width) <= CPU_CONC_LIMIT);

    m_cpuRegistersObject->write(offset, value);
}

bool S2EExecutionState::readCpuRegisterConcrete(unsigned offset,
                                                void* buf, unsigned size)
{
    assert(size <= 8);
    ref<Expr> expr = readCpuRegister(offset, size*8);
    if(!isa<ConstantExpr>(expr))
        return false;
    uint64_t value = cast<ConstantExpr>(expr)->getZExtValue();
    small_memcpy(buf, &value, size);
    return true;
}

void S2EExecutionState::writeCpuRegisterConcrete(unsigned offset,
                                                 const void* buf, unsigned size)
{
    uint64_t value = 0;
    small_memcpy(&value, buf, size);
    writeCpuRegister(offset, ConstantExpr::create(value, size*8));
}

/*
 * Read bytes from this state given a KLEE address.
 * Optionally store the result in the result array
 * Optionally concretize the result and store it
 * Optionally store a permanent constraint on the value if concretized
 *
 * kleeAddressExpr:  the Klee "address" of the memory object
 * sizeInBytes:  the number of bytes to read.  If this is too large
 *               read the maximum amount possible from one MemoryObject
 * result: optional parameter (can be NULL) to store the result
 * requireConcrete: if true, fail if the memory is not concrete
 * concretize: if true, concretize the memory but don't necessarily
 *             add a permanent constraint (i.e. get an example)
 * addConstraint: permanently concretize the memory
 */
void S2EExecutionState::kleeReadMemory(ref<Expr> kleeAddressExpr,
                                       uint64_t sizeInBytes,
                                       std::vector<ref<Expr> > *result,
                                       bool requireConcrete,
                                       bool concretize,
                                       bool addConstraint) {
    ObjectPair op;
    kleeAddressExpr = g_s2e->getExecutor()->toUnique(*this, kleeAddressExpr);
    ref<klee::ConstantExpr> address = cast<klee::ConstantExpr>(kleeAddressExpr);
    if (!addressSpace.resolveOne(address, op))
        assert(0 && "kleeReadMemory: out of bounds / multiple resolution unhandled");

    const MemoryObject *mo = op.first;
    const ObjectState *os = op.second;

    assert (requireConcrete ||
            (!requireConcrete && concretize && addConstraint) ||
            (!requireConcrete && concretize && !addConstraint) ||
            (!requireConcrete && !concretize));
    
    // Read an array of bytes
    unsigned i;
    sizeInBytes = sizeInBytes >= mo->size ? mo->size : sizeInBytes;
    for (i = 0; i < sizeInBytes; i++) {
        ref<Expr> cur = os->read8(i);
        if (requireConcrete) {
            // Here, we demand concrete results
            cur = g_s2e->getExecutor()->toUnique(*this, cur);
            assert(isa<klee::ConstantExpr>(cur) &&
                   "kleeReadMemory: hit symbolic char but expected concrete data");
            if (result) {
                result->push_back(cast<klee::ConstantExpr>(cur));
            }
        } else {
            if (concretize && addConstraint) {
                // Add constraint if necessary
                cur = g_s2e->getExecutor()->toConstant(*this, cur, "kleeReadMemory");
            } else if (concretize && !addConstraint) {
                // Otherwise just get an example
                cur = g_s2e->getExecutor()->toConstantSilent(*this, cur);
            } else {
                assert (false && "Expected reasonable parameters in kleeReadMemory");
            }

            if (result) {
                result->push_back(cur);
            }
        }
    }

    if (result) {
        assert (result->size() >= 1 && "Expected array to have results");
    }
}

/*
 * Write bytes to a given KLEE address.
 * Assumes that only one memory object is involved and that
 * the write does not span memory objects.
 * 
 * kleeAddressExpr:  the Klee "address" of the memory object
 * bytes:  The bytes to write at that location.
 */
void S2EExecutionState::kleeWriteMemory(ref<Expr> kleeAddressExpr, /* Address */
                                        std::vector<ref<Expr> > &bytes) {
    ObjectPair op;
    kleeAddressExpr = g_s2e->getExecutor()->toUnique(*this, kleeAddressExpr);
    ref<klee::ConstantExpr> address = cast<klee::ConstantExpr>(kleeAddressExpr);
    if (!addressSpace.resolveOne(address, op))
        assert(0 && "kleeReadMemory: out of bounds / multiple resolution unhandled");

    const MemoryObject *mo = op.first;
    const ObjectState *os = op.second;
    ObjectState *wos = addressSpace.getWriteable(mo, os);
    assert(bytes.size() <= os->size && "Too many bytes supplied to kleeWriteMemory");

    // Write an array of possibly-symbolic bytes
    unsigned i;
    for (i = 0; i < bytes.size(); i++) {
        wos->write(i, bytes[i]);
    }
}

void S2EExecutionState::makeSymbolic(std::vector< ref<Expr> > &args,
                                     bool makeConcolic) {
    assert(args.size() == 3);

    // KLEE address of variable
    ref<klee::ConstantExpr> kleeAddress = cast<klee::ConstantExpr>(args[0]);
    
    // Size in bytes
    uint64_t sizeInBytes = cast<klee::ConstantExpr>(args[1])->getZExtValue();

    // address of label and label string itself
    ref<klee::Expr> labelKleeAddress = args[2];
    std::vector<klee::ref<klee::Expr> > result;
    kleeReadMemory(labelKleeAddress, 31, &result, true, false, false);
    char *strBuf = new char[32];
    assert(result.size() <= 31 && "Expected fewer bytes??  See kleeReadMemory");
    unsigned i;
    for (i = 0; i < result.size(); i++) {
        strBuf[i] = cast<klee::ConstantExpr>(result[i])->getZExtValue(8);
    }
    strBuf[i] = 0;
    std::string labelStr(strBuf);
    delete [] strBuf;

    // Now insert the symbolic/concolic data for this state
    std::vector<ref<Expr> > existingData;
    std::vector<uint8_t> concreteData;
    std::vector<ref<Expr> > symb;

    if (makeConcolic) {
        kleeReadMemory(labelKleeAddress, sizeInBytes, &existingData, false, true, true);
        for (unsigned i = 0; i < sizeInBytes; ++i) {
            concreteData.push_back(cast<klee::ConstantExpr>(existingData[i])->getZExtValue(8));
        }
        symb = createConcolicArray(labelStr, sizeInBytes, concreteData);
    } else {
        symb = createSymbolicArray(labelStr, sizeInBytes);
    }

    kleeWriteMemory(kleeAddress, symb);
}

uint64_t S2EExecutionState::readCpuState(unsigned offset,
                                         unsigned width) const
{
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset >= CPU_CONC_LIMIT);
    assert(offset + Expr::getMinBytesForWidth(width) <= sizeof(CPUArchState));

    const uint8_t* address;
    if(m_active) {
        address = (uint8_t*) m_cpuSystemState->address - CPU_CONC_LIMIT;
    } else {
        address = m_cpuSystemObject->getConcreteStore(); assert(address);
        address -= CPU_CONC_LIMIT;
    }

    uint64_t ret = 0;
    small_memcpy((void*) &ret, address + offset, Expr::getMinBytesForWidth(width));

    if(width == 1)
        ret &= 1;

    return ret;
}

void S2EExecutionState::writeCpuState(unsigned offset, uint64_t value,
                                      unsigned width)
{

	assert((width == 1 || (width&7) == 0) && width <= 64);
	assert(offset >= CPU_CONC_LIMIT);
	assert(offset + Expr::getMinBytesForWidth(width) <= sizeof(CPUArchState));

    uint8_t* address;
    if(m_active) {
        address = (uint8_t*) m_cpuSystemState->address - CPU_CONC_LIMIT;
    } else {
        address = m_cpuSystemObject->getConcreteStore(); assert(address);
        address -= CPU_CONC_LIMIT;
    }

    if(width == 1)
        value &= 1;
    small_memcpy(address + offset, (void*) &value, Expr::getMinBytesForWidth(width));
}

bool S2EExecutionState::isRamRegistered(uint64_t hostAddress)
{
    ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
    return op.first != NULL && op.first->isUserSpecified;
}


bool S2EExecutionState::isRamSharedConcrete(uint64_t hostAddress)
{
    ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
    assert(op.first);
    return op.first->isSharedConcrete;
}


//Get the program counter in the current state.
//Allows plugins to retrieve it in a hardware-independent manner.
uint64_t S2EExecutionState::getPc() const
{
    return readCpuState(CPU_OFFSET(PROG_COUNTER), 8 * CPU_REG_SIZE);
}

uint64_t S2EExecutionState::getFlags()
{
#ifdef TARGET_I386
    /* restore flags in standard format */
    WR_cpu(env, cc_src, cpu_cc_compute_all(env, CC_OP));
    WR_cpu(env, cc_op, CC_OP_EFLAGS);
    return cpu_get_eflags(env);
#elif TARGET_ARM
    // TODO: avoid duplication with cpsr_read()
    uint8_t cpsr_symbolic = getSymbolicRegistersMask() & 0x0f;
    if (cpsr_symbolic == 0) {
        // Fast Path
        return cpsr_read(reinterpret_cast<CPUArchState *>(m_cpuRegistersState->address));
    } else {
        // Check the symbolic bits
        CPUArchState * s = reinterpret_cast<CPUArchState *>(m_cpuRegistersState->address);

        // CF
        target_ulong CF = 0;
        if (cpsr_symbolic & 0x01) {
            //TODO
        } else {
            CF = ((s->CF) << 29);
        }

        // VF
        target_ulong VF = 0;
        if (cpsr_symbolic & 0x02) {
            //TODO
        } else {
            VF = ((s->VF & 0x80000000) >> 3);
        }

        // NF
        target_ulong NF = 0;
        if (cpsr_symbolic & 0x04) {
            //TODO
        } else {
            NF = ((s->NF) & 0x80000000);
        }

        // ZF
        target_ulong ZF = 0;
        if (cpsr_symbolic & 0x08) {
            //TODO
        } else {
            ZF = (s->ZF == 0);
        }

        // These bit are always concrete
        target_ulong QF, thumb, condex1, condex2, GE;
        QF = (env->QF << 27);
        thumb = (env->thumb << 5);
        condex1 = ((env->condexec_bits & 3) << 25);
        condex2 = ((env->condexec_bits & 0xfc) << 8);
        GE = (env->GE << 16);

        // Re-assemble the cpsr
        return env->uncached_cpsr | NF | (ZF << 30) |
            CF | VF | QF
            | thumb | condex1
            | condex2
            | GE;
    }
#else
    return 0;
#endif
}

void S2EExecutionState::setPc(uint64_t pc)
{
    writeCpuState(CPU_OFFSET(PROG_COUNTER), pc, CPU_REG_SIZE * 8);
}

void S2EExecutionState::setSp(uint64_t sp)
{
#ifdef TARGET_ARM
    writeCpuRegisterConcrete(CPU_OFFSET(regs[13]), &sp, CPU_REG_SIZE);
#elif defined(TARGET_I386)
    writeCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]), &sp, CPU_REG_SIZE);
#endif
}

uint64_t S2EExecutionState::getSp() const
{
#ifdef TARGET_ARM
    ref<Expr> e = readCpuRegister(CPU_OFFSET(regs[13]), 8 * CPU_REG_SIZE);
#elif defined(TARGET_I386)
    ref<Expr> e = readCpuRegister(CPU_OFFSET(regs[R_ESP]), 8 * CPU_REG_SIZE);
#endif
    return cast<ConstantExpr>(e)->getZExtValue(64);
}

//This function must be called just after the machine call instruction
//was executed.
bool S2EExecutionState::bypassFunction(unsigned paramCount)
{
    bool ok = false;
    uint64_t retAddr;
    target_ulong newSp = 0;
    ok = getReturnAddress(&retAddr);

#if defined(TARGET_I386)
    newSp = getSp();
#ifdef TARGET_X86_64
    if (env->hflags & HF_CS64_MASK) {
    // First six parameters in x86_64 are passed in registers, with the rest on
    // the stack
        newSp += (paramCount > 6) ? (paramCount - 5) * CPU_REG_SIZE : CPU_REG_SIZE;
    } else
#else
    newSp += (paramCount + 1) * sizeof (uint32_t);
#endif

#elif defined(TARGET_ARM)
    if (retAddr & 1) {
        // return-to-Thumb case, this is actually retAddr+1
        retAddr &= ~1;
    }
    // First four parameters in ARM are passed in registers, with the rest on
    // the stack
    newSp = getSp();
    newSp += (paramCount > 4) ? (paramCount - 3) * CPU_REG_SIZE : CPU_REG_SIZE;
#else
    assert(false && "Not implemented for this architecture");
#endif
    if (ok) {
        setSp(newSp);
        setPc(retAddr);
    }
    return ok;

}

//May be called right after the machine call instruction
bool S2EExecutionState::getReturnAddress(uint64_t *retAddr)
{
    bool ok = false;
    target_ulong t_retAddr = 0;
    target_ulong size = 0;
    *retAddr = 0;

#ifdef TARGET_I386
    size = sizeof (uint32_t);
#ifdef TARGET_X86_64
    if (env->hflags & HF_CS64_MASK) size = CPU_REG_SIZE;
#endif /* TARGET_X86_64 */
    ok = readMemoryConcrete(getSp(), &t_retAddr, size);

#elif defined(TARGET_ARM)
    size = CPU_REG_SIZE;
    ok = readCpuRegisterConcrete(CPU_OFFSET(regs[14]), &t_retAddr, size);
    // Beware: in return-to-Thumb case, you got retaddr+1
#else
    assert(false && "Not implemented on this architecture");
#endif

    if (!ok) {
        g_s2e->getDebugStream() << "Could not get the return address" << '\n';
    } else {
        *retAddr = static_cast<uint64_t>(t_retAddr);
    }
    return ok;
}

void S2EExecutionState::dumpStack(unsigned count)
{
    dumpStack(getSp());
}

void S2EExecutionState::dumpStack(unsigned count, uint64_t sp)
{
    std::stringstream os;

    os << "Dumping stack @0x" << std::hex << sp << '\n';

    for (unsigned i=0; i<count; ++i) {
        klee::ref<klee::Expr> val = readMemory(sp + i * sizeof(uint32_t), klee::Expr::Int32);
        klee::ConstantExpr *ce = dyn_cast<klee::ConstantExpr>(val);
        if (ce) {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << " 0x" << std::setw(sizeof(uint32_t)*2) << std::setfill('0') << val;
            os << std::setfill(' ');
        }else {
            os << std::hex << "0x" << sp + i * sizeof(uint32_t) << val;
        }
        os << '\n';
    }

     g_s2e->getDebugStream();
}


uint64_t S2EExecutionState::getTotalInstructionCount()
{
    if (!m_cpuSystemState) {
        return 0;
    }
    return readCpuState(CPU_OFFSET(s2e_icount), 8*sizeof(uint64_t));
}


TranslationBlock *S2EExecutionState::getTb() const
{
    return (TranslationBlock*)
            readCpuState(CPU_OFFSET(s2e_current_tb), 8*sizeof(void*));
}

uint64_t S2EExecutionState::getPid() const
{
#ifdef TARGET_ARM
	//TODO: write pid somewhere in the cpu state
	return (uint64_t) -1;
#elif defined(TARGET_I386)
	return readCpuState(offsetof(CPUX86State, cr[3]), 8*sizeof(target_ulong));
#endif

}

uint64_t S2EExecutionState::getSymbolicRegistersMask() const
{
    const ObjectState* os = m_cpuRegistersObject;
    if(os->isAllConcrete())
        return 0;

    uint64_t mask = 0;

#ifdef TARGET_I386
    uint64_t offset = 0;
    /* XXX: x86-specific */
    for (int i = 0; i < CPU_NB_REGS; ++i) { /* regs */
        if (!os->isConcrete(offset, sizeof(*env->regs) << 3)) {
            mask |= (1 << (i+5));
        }
        offset += sizeof (*env->regs);
    }

    if (!os->isConcrete(offsetof(CPUX86State, cc_op),
                        sizeof(env->cc_op) << 3)) {
        mask |= _M_CC_OP;
    }
    if (!os->isConcrete(offsetof(CPUX86State, cc_src),
                        sizeof(env->cc_src) << 3)) {
        mask |= _M_CC_SRC;
    }
    if (!os->isConcrete(offsetof(CPUX86State, cc_dst),
                        sizeof(env->cc_dst) << 3)) {
        mask |= _M_CC_DST;
    }
    if (!os->isConcrete(offsetof(CPUX86State, cc_tmp),
                        sizeof(env->cc_tmp) << 3)) {
        mask |= _M_CC_TMP;
    }
#elif defined(TARGET_ARM)
    if(!os->isConcrete( 29*4, 4*8)) // CF
        mask |= (1 << 1);
    if(!os->isConcrete( 30*4, 4*8)) // VF
        mask |= (1 << 2);
    if(!os->isConcrete(31*4, 4*8)) // NF
        mask |= (1 << 3);
    if(!os->isConcrete(32*4, 4*8)) // ZF
        mask |= (1 << 4);
    for(int i = 0; i < 15; ++i) { /* regs */
            if(!os->isConcrete((33+i)*4, 4*8))
                mask |= (1 << (i+5));
    }
    if(!os->isConcrete(0, 4*8)) // spsr
        mask |= (1 << 20);
    for(int i = 0; i < 6; ++i) { /* banked_spsr */
            if(!os->isConcrete((1+i)*4, 4*8))
                mask |= (1 << (i+21));
    }
    for(int i = 0; i < 6; ++i) { /* banked r13 */
            if(!os->isConcrete((7+i)*4, 4*8))
                mask |= (1 << (i+27));
    }
    for(int i = 0; i < 6; ++i) { /* banked r14 */
            if(!os->isConcrete((13+i)*4, 4*8))
                mask |= (1 << (i+33));
    }
    for(int i = 0; i < 5; ++i) { /* usr_regs */
            if(!os->isConcrete((19+i)*4, 4*8))
                mask |= (1 << (i+39));
    }
    for(int i = 0; i < 5; ++i) { /* fiq_regs */
            if(!os->isConcrete((24+i)*4, 4*8))
                mask |= (1 << (i+44));
    }
#else
    assert(false & "Update Hardcoded masking of symbolic fields of CPUArchState for your target.");
#error "Target architecture not supported"
#endif
    return mask;
}

bool S2EExecutionState::readMemoryConcrete(uint64_t address, void *buf,
                                   uint64_t size, AddressType addressType)
{
    uint8_t *d = (uint8_t*)buf;
    while (size>0) {
        ref<Expr> v = readMemory(address, Expr::Int8, addressType);
        if (v.isNull() || !isa<ConstantExpr>(v)) {
            return false;
        }
        *d = (uint8_t)cast<ConstantExpr>(v)->getZExtValue(8);
        size--;
        d++;
        address++;
    }
    return true;
}

bool S2EExecutionState::writeMemoryConcrete(uint64_t address, void *buf,
                                   uint64_t size, AddressType addressType)
{
    uint8_t *d = (uint8_t*)buf;
    while (size>0) {
        klee::ref<klee::ConstantExpr> val = klee::ConstantExpr::create(*d, klee::Expr::Int8);
        bool b = writeMemory(address, val,  addressType);
        if (!b) {
            return false;
        }
        size--;
        d++;
        address++;
    }
    return true;
}

uint64_t S2EExecutionState::getPhysicalAddress(uint64_t virtualAddress) const
{
    assert(m_active && "Can not use getPhysicalAddress when the state"
                       " is not active (TODO: fix it)");
    target_phys_addr_t physicalAddress =
        cpu_get_phys_page_debug(env, virtualAddress & TARGET_PAGE_MASK);
    if(physicalAddress == (target_phys_addr_t) -1)
        return (uint64_t) -1;

    return physicalAddress | (virtualAddress & ~TARGET_PAGE_MASK);
}

uint64_t S2EExecutionState::getHostAddress(uint64_t address,
                                           AddressType addressType) const
{
    if(addressType != HostAddress) {
        uint64_t hostAddress = address & TARGET_PAGE_MASK;
        if(addressType == VirtualAddress) {
            hostAddress = getPhysicalAddress(hostAddress);
            if(hostAddress == (uint64_t) -1)
                return (uint64_t) -1;
        }

        /* We can not use qemu_get_ram_ptr directly. Mapping of IO memory
           can be modified after memory registration and qemu_get_ram_ptr will
           return incorrect values in such cases */
        hostAddress = s2e_get_host_address(hostAddress);
        if(!hostAddress)
            return (uint64_t) -1;

        return hostAddress | (address & ~TARGET_PAGE_MASK);

    } else {
        return address;
    }
}

bool S2EExecutionState::readString(uint64_t address, std::string &s, unsigned maxLen)
{
    s = "";
    do {
        uint8_t c;
        SREADR(this, address, c);

        if (c) {
            s = s + (char)c;
        }else {
            return true;
        }
        address++;
        maxLen--;
    }while(maxLen != 0);
    return true;
}

bool S2EExecutionState::readUnicodeString(uint64_t address, std::string &s, unsigned maxLen)
{
    s = "";
    do {
        uint16_t c;
        SREADR(this, address, c);

        if (c) {
            s = s + (char)c;
        }else {
            return true;
        }

        address+=2;
        maxLen--;
    }while(maxLen != 0);
    return true;
}

ref<Expr> S2EExecutionState::readMemory(uint64_t address,
                            Expr::Width width, AddressType addressType) const
{
    assert(width == 1 || (width & 7) == 0);
    uint64_t size = width / 8;

    uint64_t pageOffset = address & ~S2E_RAM_OBJECT_MASK;
    if(pageOffset + size <= S2E_RAM_OBJECT_SIZE) {
        /* Fast path: read belongs to one MemoryObject */
        uint64_t hostAddress = getHostAddress(address, addressType);
        if(hostAddress == (uint64_t) -1)
            return ref<Expr>(0);

        ObjectPair op = m_memcache.get(hostAddress & S2E_RAM_OBJECT_MASK);
        if (!op.first) {
            op = addressSpace.findObject(hostAddress & S2E_RAM_OBJECT_MASK);
            m_memcache.put(hostAddress & S2E_RAM_OBJECT_MASK, op);
        }

        assert(op.first && op.first->isUserSpecified
               && op.first->size == S2E_RAM_OBJECT_SIZE);

        return op.second->read(pageOffset, width);
    } else {
        /* Access spawns multiple MemoryObject's (TODO: could optimize it) */
        ref<Expr> res(0);
        for(unsigned i = 0; i != size; ++i) {
            unsigned idx = klee::Context::get().isLittleEndian() ?
                           i : (size - i - 1);
            ref<Expr> byte = readMemory8(address + idx, addressType);
            if(byte.isNull()) return ref<Expr>(0);
            res = idx ? ConcatExpr::create(byte, res) : byte;
        }
        return res;
    }
}

ref<Expr> S2EExecutionState::readMemory8(uint64_t address,
                                         AddressType addressType) const
{
    uint64_t hostAddress = getHostAddress(address, addressType);
    if(hostAddress == (uint64_t) -1)
        return ref<Expr>(0);

    ObjectPair op = addressSpace.findObject(hostAddress & S2E_RAM_OBJECT_MASK);

    assert(op.first && op.first->isUserSpecified
           && op.first->size == S2E_RAM_OBJECT_SIZE);

    return op.second->read8(hostAddress & ~S2E_RAM_OBJECT_MASK);
}

bool S2EExecutionState::readMemoryConcrete8(uint64_t address,
                                            uint8_t *result,
                                            AddressType addressType,
                                            bool addConstraint)
{
    ref<Expr> expr = readMemory8(address, addressType);
    if(!expr.isNull()) {
        if(addConstraint) { /* concretize */
            expr = g_s2e->getExecutor()->toConstant(*this, expr, "readMemoryConcrete8");
        } else { /* example */
            expr = g_s2e->getExecutor()->toConstantSilent(*this, expr);
        }

        if (result) {
            ConstantExpr *ce = dyn_cast<ConstantExpr>(expr);
            if (!ce) {
                return false;
            }

            *result = ce->getZExtValue();
        }

        return writeMemory(address, expr);
    }

    return false;
}


bool S2EExecutionState::writeMemory(uint64_t address,
                                    ref<Expr> value,
                                    AddressType addressType)
{
    Expr::Width width = value->getWidth();
    assert(width == 1 || (width & 7) == 0);
    ConstantExpr *constantExpr = dyn_cast<ConstantExpr>(value);
    if(constantExpr && width <= 64) {
        // Concrete write of supported width
        uint64_t val = constantExpr->getZExtValue();
        switch (width) {
            case Expr::Bool:
            case Expr::Int8:  return writeMemory8 (address, val, addressType);
            case Expr::Int16: return writeMemory16(address, val, addressType);
            case Expr::Int32: return writeMemory32(address, val, addressType);
            case Expr::Int64: return writeMemory64(address, val, addressType);
            default: assert(0);
        }
        return false;

    } else if(width == Expr::Bool) {
        // Boolean write is a special case
        return writeMemory8(address, ZExtExpr::create(value, Expr::Int8),
                            addressType);

    } else if((address & ~S2E_RAM_OBJECT_MASK) + (width / 8) <= S2E_RAM_OBJECT_SIZE) {
        // All bytes belong to a single MemoryObject

        uint64_t hostAddress = getHostAddress(address, addressType);
        if(hostAddress == (uint64_t) -1)
            return false;

        ObjectPair op = addressSpace.findObject(hostAddress & S2E_RAM_OBJECT_MASK);

        assert(op.first && op.first->isUserSpecified
               && op.first->size == S2E_RAM_OBJECT_SIZE);

        ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
        wos->write(hostAddress & ~S2E_RAM_OBJECT_MASK, value);
    } else {
        // Slowest case (TODO: could optimize it)
        unsigned numBytes = width / 8;
        for(unsigned i = 0; i != numBytes; ++i) {
            unsigned idx = Context::get().isLittleEndian() ?
                           i : (numBytes - i - 1);
            if(!writeMemory8(address + idx,
                    ExtractExpr::create(value, 8*i, Expr::Int8), addressType)) {
                return false;
            }
        }
    }
    return true;
}

bool S2EExecutionState::writeMemory8(uint64_t address,
                                     ref<Expr> value, AddressType addressType)
{
    assert(value->getWidth() == 8);

    uint64_t hostAddress = getHostAddress(address, addressType);
    if(hostAddress == (uint64_t) -1)
        return false;

    ObjectPair op = addressSpace.findObject(hostAddress & S2E_RAM_OBJECT_MASK);

    assert(op.first && op.first->isUserSpecified
           && op.first->size == S2E_RAM_OBJECT_SIZE);

    ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
    wos->write(hostAddress & ~S2E_RAM_OBJECT_MASK, value);
    return true;
}

bool S2EExecutionState::writeMemory(uint64_t address,
                    uint8_t* buf, Expr::Width width, AddressType addressType)
{
    assert((width & 7) == 0);
    uint64_t size = width / 8;

    uint64_t pageOffset = address & ~S2E_RAM_OBJECT_MASK;
    if(pageOffset + size <= S2E_RAM_OBJECT_SIZE) {
        /* Fast path: write belongs to one MemoryObject */

        uint64_t hostAddress = getHostAddress(address, addressType);
        if(hostAddress == (uint64_t) -1)
            return false;

        ObjectPair op = addressSpace.findObject(hostAddress & S2E_RAM_OBJECT_MASK);

        assert(op.first && op.first->isUserSpecified
               && op.first->size == S2E_RAM_OBJECT_SIZE);

        ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
        for(uint64_t i = 0; i < width / 8; ++i)
            wos->write8(pageOffset + i, buf[i]);

    } else {
        /* Access spawns multiple MemoryObject's */
        uint64_t size1 = S2E_RAM_OBJECT_SIZE - pageOffset;
        if(!writeMemory(address, buf, size1, addressType))
            return false;
        if(!writeMemory(address + size1, buf + size1, size - size1, addressType))
            return false;
    }
    return true;
}

bool S2EExecutionState::writeMemory8(uint64_t address,
                                     uint8_t value, AddressType addressType)
{
    return writeMemory(address, &value, 8, addressType);
}

bool S2EExecutionState::writeMemory16(uint64_t address,
                                     uint16_t value, AddressType addressType)
{
    return writeMemory(address, (uint8_t*) &value, 16, addressType);
}

bool S2EExecutionState::writeMemory32(uint64_t address,
                                      uint32_t value, AddressType addressType)
{
    return writeMemory(address, (uint8_t*) &value, 32, addressType);
}

bool S2EExecutionState::writeMemory64(uint64_t address,
                                     uint64_t value, AddressType addressType)
{
    return writeMemory(address, (uint8_t*) &value, 64, addressType);
}


void S2EExecutionState::readRamConcreteCheck(uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    assert(m_active && m_runningConcrete);
    uint64_t page_offset = hostAddress & ~S2E_RAM_OBJECT_MASK;
    if(page_offset + size <= S2E_RAM_OBJECT_SIZE) {
        /* Single-object access */

        uint64_t page_addr = hostAddress & S2E_RAM_OBJECT_MASK;

        //ObjectPair op = addressSpace.findObject(page_addr);
        ObjectPair op = m_memcache.get(page_addr);
        if (!op.first) {
            op = addressSpace.findObject(page_addr);
            m_memcache.put(page_addr, op);
        }


        assert(op.first && op.first->isUserSpecified &&
               op.first->address == page_addr &&
               op.first->size == S2E_RAM_OBJECT_SIZE);

        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                if (PrintModeSwitch) {
                    g_s2e->getMessagesStream()
                            << "Switching to KLEE executor at pc = "
                            << hexval(getPc()) << '\n';
                }
                m_startSymbexAtPC = getPc();
                // XXX: what about regs_to_env ?
                s2e_longjmp(env->jmp_env, 1);
            }
        }
    } else {
        /* Access spans multiple MemoryObject's */
        uint64_t size1 = S2E_RAM_OBJECT_SIZE - page_offset;
        readRamConcreteCheck(hostAddress, buf, size1);
        readRamConcreteCheck(hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutionState::readRamConcrete(uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    assert(m_active);
    uint64_t page_offset = hostAddress & ~S2E_RAM_OBJECT_MASK;
    if(page_offset + size <= S2E_RAM_OBJECT_SIZE) {
        /* Single-object access */

        uint64_t page_addr = hostAddress & S2E_RAM_OBJECT_MASK;

        //ObjectPair op = addressSpace.findObject(page_addr);
        ObjectPair op = m_memcache.get(page_addr);
        if (!op.first) {
            op = addressSpace.findObject(page_addr);
            m_memcache.put(page_addr, op);
        }


        assert(op.first && op.first->isUserSpecified &&
               op.first->address == page_addr &&
               op.first->size == S2E_RAM_OBJECT_SIZE);

        ObjectState *wos = NULL;
        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                if(!wos) {
                    op.second = wos = addressSpace.getWriteable(
                                                    op.first, op.second);
                }
                buf[i] = g_s2e->getExecutor()->toConstant(*this, wos->read8(page_offset+i),
                       "memory access from concrete code")->getZExtValue(8);
                wos->write8(page_offset+i, buf[i]);
            }
        }
    } else {
        /* Access spans multiple MemoryObject's */
        uint64_t size1 = S2E_RAM_OBJECT_SIZE - page_offset;
        readRamConcrete(hostAddress, buf, size1);
        readRamConcrete(hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutionState::writeRamConcrete(uint64_t hostAddress, const uint8_t* buf, uint64_t size)
{
    assert(m_active);
    uint64_t page_offset = hostAddress & ~S2E_RAM_OBJECT_MASK;
    if(page_offset + size <= S2E_RAM_OBJECT_SIZE) {
        /* Single-object access */

        uint64_t page_addr = hostAddress & S2E_RAM_OBJECT_MASK;


        //ObjectPair op = addressSpace.findObject(page_addr);
        ObjectPair op = m_memcache.get(page_addr);
        if (!op.first) {
            op = addressSpace.findObject(page_addr);
            m_memcache.put(page_addr, op);
        }

        assert(op.first && op.first->isUserSpecified &&
               op.first->address == page_addr &&
               op.first->size == S2E_RAM_OBJECT_SIZE);

        ObjectState* wos =
                addressSpace.getWriteable(op.first, op.second);
        for(uint64_t i=0; i<size; ++i) {
            wos->write8(page_offset+i, buf[i]);
        }

    } else {
        /* Access spans multiple MemoryObject's */
        uint64_t size1 = S2E_RAM_OBJECT_SIZE - page_offset;
        writeRamConcrete(hostAddress, buf, size1);
        writeRamConcrete(hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutionState::readRegisterConcrete(
        CPUArchState *cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    assert(m_active);
    assert(((uint64_t)cpuState) == m_cpuRegistersState->address);
    assert(offset + size <= CPU_CONC_LIMIT);

    if(!m_runningConcrete ||
            !m_cpuRegistersObject->isConcrete(offset, size*8)) {
        ObjectState* wos = m_cpuRegistersObject;

        for(unsigned i = 0; i < size; ++i) {
            if(!wos->readConcrete8(offset+i, buf+i)) {
                const char* reg;
                switch(offset) {
#ifdef TARGET_I386
                    case offsetof(CPUX86State, regs[R_EAX]): reg = "eax"; break;
                    case offsetof(CPUX86State, regs[R_ECX]): reg = "ecx"; break;
                    case offsetof(CPUX86State, regs[R_EDX]): reg = "edx"; break;
                    case offsetof(CPUX86State, regs[R_EBX]): reg = "ebx"; break;
                    case offsetof(CPUX86State, regs[R_ESP]): reg = "esp"; break;
                    case offsetof(CPUX86State, regs[R_EBP]): reg = "ebp"; break;
                    case offsetof(CPUX86State, regs[R_ESI]): reg = "esi"; break;
                    case offsetof(CPUX86State, regs[R_EDI]): reg = "edi"; break;

                    case offsetof(CPUX86State, cc_src): reg = "cc_src"; break;
                    case offsetof(CPUX86State, cc_dst): reg = "cc_dst"; break;
                    case offsetof(CPUX86State, cc_op): reg = "cc_op"; break;
                    case offsetof(CPUX86State, cc_tmp): reg = "cc_tmp"; break;
#elif defined(TARGET_ARM)
                    case 116: reg = "CF"; break;
                    case 120: reg = "VF"; break;
                    case 124: reg = "NF"; break;
                    case 128: reg = "ZF"; break;
                    case 132: reg = "r0"; break;
                    case 136: reg = "r1"; break;
                    case 140: reg = "r2"; break;
                    case 144: reg = "r3"; break;
                    case 148: reg = "r4"; break;
                    case 152: reg = "r5"; break;
                    case 156: reg = "r6"; break;
                    case 160: reg = "r7"; break;
                    case 164: reg = "r8"; break;
                    case 168: reg = "r9"; break;
                    case 172: reg = "r10"; break;
                    case 176: reg = "r11"; break;
                    case 180: reg = "r12"; break;
                    case 184: reg = "r13"; break;
                    case 188: reg = "r14"; break;
                    case 192: reg = "r15"; break;
#else
#error "Target architecture not supported"
#endif
                    default: reg = "unknown"; break;
                }
                std::string reason = std::string("access to ") + reg +
                                     " register from QEMU helper";
                buf[i] = g_s2e->getExecutor()->toConstant(*this, wos->read8(offset+i),
                                    reason.c_str())->getZExtValue(8);
                wos->write8(offset+i, buf[i]);
            }
        }
    } else {
        //XXX: check if the size is always small enough
        small_memcpy(buf, ((uint8_t*)cpuState)+offset, size);
    }

#if defined(S2E_TRACE_EFLAGS) && defined(TARGET_I386)
    if (offsetof(CPUX86State, cc_src) == offset) {
        m_s2e->getDebugStream() <<  std::hex << getPc() <<
                "read conc cc_src " << (*(uint32_t*)((uint8_t*)buf)) << '\n';
    }
#endif
}

void S2EExecutionState::writeRegisterConcrete(CPUArchState *cpuState,
                                              unsigned offset, const uint8_t* buf, unsigned size)
{
    assert(m_active);
    assert(((uint64_t)cpuState) == m_cpuRegistersState->address);
    assert(offset + size <= CPU_CONC_LIMIT);

    if(!m_runningConcrete ||
            !m_cpuRegistersObject->isConcrete(offset, size*8)) {
        ObjectState* wos = m_cpuRegistersObject;
        for(unsigned i = 0; i < size; ++i)
            wos->write8(offset+i, buf[i]);
    } else {
        assert(m_cpuRegistersObject->isConcrete(offset, size*8));
        small_memcpy(((uint8_t*)cpuState)+offset, buf, size);
    }

#if defined(S2E_TRACE_EFLAGS) && defined(TARGET_I386)
    if (offsetof(CPUX86State, cc_src) == offset) {
        m_s2e->getDebugStream() <<  std::hex << getPc() <<
                "write conc cc_src " << (*(uint32_t*)((uint8_t*)buf)) << '\n';
    }
#endif

}


std::string S2EExecutionState::getUniqueVarName(const std::string &name)
{
    std::stringstream ss;
    ss << "v" << (m_nextSymbVarId++) << "_";

    for (unsigned i=0; i<name.size(); ++i) {
        if (isspace(name[i])) {
            ss  << '_';
        }else {
            ss << name[i];
        }
    }

    ss << "_" << s_lastSymbolicId;
    ++s_lastSymbolicId;
    return ss.str();
}

ref<Expr> S2EExecutionState::createConcolicValue(
          const std::string& name, Expr::Width width,
          std::vector<unsigned char> &buffer)
{
    std::string sname = getUniqueVarName(name);

    unsigned bytes = Expr::getMinBytesForWidth(width);
    unsigned bufferSize = buffer.size();

    assert((bufferSize == bytes || bufferSize == 0)  &&
           "Concrete buffer must either have the same size as the expression or be empty");

    const Array *array = new Array(sname, bytes);

    MemoryObject *mo = new MemoryObject(0, bytes, false, false, false, NULL);
    mo->setName(sname);

    symbolics.push_back(std::make_pair(mo, array));

    if (bufferSize == bytes) {
        if (ConcolicMode) {
            concolics.add(array, buffer);
        } else {
            g_s2e->getWarningsStream(this)
                    << "Concolic mode disabled: ignoring concrete assignments for " << name << '\n';

        }
    }

    return  Expr::createTempRead(array, width);
}


ref<Expr> S2EExecutionState::createSymbolicValue(
            const std::string& name, Expr::Width width)
{

    std::vector<unsigned char> concreteValues;

    if (ConcolicMode) {
        unsigned bytes = Expr::getMinBytesForWidth(width);
        for (unsigned i = 0; i < bytes; ++i) {
            concreteValues.push_back(0);
        }
    }

    return createConcolicValue(name, width, concreteValues);
}

std::vector<ref<Expr> > S2EExecutionState::createConcolicArray(
            const std::string& name,
            unsigned size,
            std::vector<unsigned char> &concreteBuffer)
{
    assert(concreteBuffer.size() == size || concreteBuffer.size() == 0);

    std::string sname = getUniqueVarName(name);
    const Array *array = new Array(sname, size);

    UpdateList ul(array, 0);

    std::vector<ref<Expr> > result;
    result.reserve(size);

    for(unsigned i = 0; i < size; ++i) {
        result.push_back(ReadExpr::create(ul,
                    ConstantExpr::alloc(i,Expr::Int32)));
    }

    //Add it to the set of symbolic expressions, to be able to generate
    //test cases later.
    //Dummy memory object
    MemoryObject *mo = new MemoryObject(0, size, false, false, false, NULL);
    mo->setName(sname);

    symbolics.push_back(std::make_pair(mo, array));

    if (concreteBuffer.size() == size) {
        if (ConcolicMode) {
            concolics.add(array, concreteBuffer);
        } else {
            g_s2e->getWarningsStream(this)
                    << "Concolic mode disabled: ignoring concrete assignments for " << name << '\n';

        }
    }

    return result;
}

std::vector<ref<Expr> > S2EExecutionState::createSymbolicArray(
            const std::string& name, unsigned size)
{
    std::vector<unsigned char> concreteBuffer;

    if (ConcolicMode) {
        for (unsigned i = 0; i < size; ++i) {
            concreteBuffer.push_back(0);
        }
    }

    return createConcolicArray(name, size, concreteBuffer);
}

//Must be called right after the machine call instruction is executed.
//This function will reexecute the call but in symbolic mode
//XXX: remove circular references with executor?
void S2EExecutionState::undoCallAndJumpToSymbolic()
{
    if (needToJumpToSymbolic()) {
        //Undo the call
        target_ulong size = sizeof (uint32_t);
#ifdef TARGET_X86_64
        if (env->hflags & HF_CS64_MASK) size = CPU_REG_SIZE;
#endif /* TARGET_X86_64 */
        assert(getTb()->pcOfLastInstr);
        setSp(getSp() + size);
        setPc(getTb()->pcOfLastInstr);
        jumpToSymbolicCpp();
    }
}

void S2EExecutionState::jumpToSymbolicCpp()
{
    if (!isRunningConcrete()) {
        return;
    }
    m_toRunSymbolically.insert(std::make_pair(getPc(), getPid()));
    m_startSymbexAtPC = getPc();
    // XXX: what about regs_to_env ?
    throw CpuExitException();
}

void S2EExecutionState::jumpToSymbolic()
{
    assert(isActive() && isRunningConcrete());

    m_startSymbexAtPC = getPc();
    // XXX: what about regs_to_env ?
    s2e_longjmp(env->jmp_env, 1);
}

bool S2EExecutionState::needToJumpToSymbolic() const
{
    return  isRunningConcrete();
}

void S2EExecutionState::dumpCpuState(llvm::raw_ostream &os) const
{
    os << "[State " << m_stateID << "] CPU dump" << '\n';
#ifdef TARGET_I386
    os << "EAX=" << readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32) << '\n';
    os << "EBX=" << readCpuRegister(offsetof(CPUX86State, regs[R_EBX]), klee::Expr::Int32) << '\n';
    os << "ECX=" << readCpuRegister(offsetof(CPUX86State, regs[R_ECX]), klee::Expr::Int32) << '\n';
    os << "EDX=" << readCpuRegister(offsetof(CPUX86State, regs[R_EDX]), klee::Expr::Int32) << '\n';
    os << "ESI=" << readCpuRegister(offsetof(CPUX86State, regs[R_ESI]), klee::Expr::Int32) << '\n';
    os << "EDI=" << readCpuRegister(offsetof(CPUX86State, regs[R_EDI]), klee::Expr::Int32) << '\n';
    os << "EBP=" << readCpuRegister(offsetof(CPUX86State, regs[R_EBP]), klee::Expr::Int32) << '\n';
    os << "ESP=" << readCpuRegister(offsetof(CPUX86State, regs[R_ESP]), klee::Expr::Int32) << '\n';
    os << "EIP=" << readCpuState(offsetof(CPUX86State, eip), 32) << '\n';
    os << "CR2=" << readCpuState(offsetof(CPUX86State, cr[2]), 32) << '\n';
#elif defined(TARGET_ARM)
    os << "R0=" << readCpuRegister(offsetof(CPUARMState, regs[0]), klee::Expr::Int32) << "\n";
    os << "R1=" << readCpuRegister(offsetof(CPUARMState, regs[1]), klee::Expr::Int32) << "\n";
    os << "R2=" << readCpuRegister(offsetof(CPUARMState, regs[2]), klee::Expr::Int32) << "\n";
    os << "R3=" << readCpuRegister(offsetof(CPUARMState, regs[3]), klee::Expr::Int32) << "\n";
    os << "R4=" << readCpuRegister(offsetof(CPUARMState, regs[4]), klee::Expr::Int32) << "\n";
    os << "R5=" << readCpuRegister(offsetof(CPUARMState, regs[5]), klee::Expr::Int32) << "\n";
    os << "R6=" << readCpuRegister(offsetof(CPUARMState, regs[6]), klee::Expr::Int32) << "\n";
    os << "R7=" << readCpuRegister(offsetof(CPUARMState, regs[7]), klee::Expr::Int32) << "\n";
    os << "R8=" << readCpuRegister(offsetof(CPUARMState, regs[8]), klee::Expr::Int32) << "\n";
    os << "R9=" << readCpuRegister(offsetof(CPUARMState, regs[9]), klee::Expr::Int32) << "\n";
    os << "R10=" << readCpuRegister(offsetof(CPUARMState, regs[10]), klee::Expr::Int32) << "\n";
    os << "R11=" << readCpuRegister(offsetof(CPUARMState, regs[11]), klee::Expr::Int32) << "\n";
    os << "R12=" << readCpuRegister(offsetof(CPUARMState, regs[12]), klee::Expr::Int32) << "\n";
    os << "R13=" << readCpuRegister(offsetof(CPUARMState, regs[13]), klee::Expr::Int32) << "\n";
    os << "R14=" << readCpuRegister(offsetof(CPUARMState, regs[14]), klee::Expr::Int32) << "\n";
    os << "R15=" << readCpuRegister(offsetof(CPUARMState, regs[15]), klee::Expr::Int32) << "\n";
#else
#error "Target architecture not supported"
#endif
}

bool S2EExecutionState::merge(const ExecutionState &_b)
{
    assert(dynamic_cast<const S2EExecutionState*>(&_b));
    const S2EExecutionState& b = static_cast<const S2EExecutionState&>(_b);

    assert(!m_active && !b.m_active);

    llvm::raw_ostream& s = g_s2e->getMessagesStream(this);

    if(DebugLogStateMerge)
        s << "Attempting merge with state " << b.getID() << '\n';

    if(pc != b.pc) {
        if(DebugLogStateMerge)
            s << "merge failed: different pc" << '\n';
        return false;
    }

    // XXX is it even possible for these to differ? does it matter? probably
    // implies difference in object states?
    if(symbolics != b.symbolics) {
        if(DebugLogStateMerge)
            s << "merge failed: different symbolics" << '\n';
        return false;
    }

    {
        std::vector<StackFrame>::const_iterator itA = stack.begin();
        std::vector<StackFrame>::const_iterator itB = b.stack.begin();
        while (itA!=stack.end() && itB!=b.stack.end()) {
            // XXX vaargs?
            if(itA->caller!=itB->caller || itA->kf!=itB->kf) {
                if(DebugLogStateMerge)
                    s << "merge failed: different callstacks" << '\n';
            }
          ++itA;
          ++itB;
        }
        if(itA!=stack.end() || itB!=b.stack.end()) {
            if(DebugLogStateMerge)
                s << "merge failed: different callstacks" << '\n';
            return false;
        }
    }

    std::set< ref<Expr> > aConstraints(constraints.begin(), constraints.end());
    std::set< ref<Expr> > bConstraints(b.constraints.begin(),
                                       b.constraints.end());
    std::set< ref<Expr> > commonConstraints, aSuffix, bSuffix;
    std::set_intersection(aConstraints.begin(), aConstraints.end(),
                          bConstraints.begin(), bConstraints.end(),
                          std::inserter(commonConstraints, commonConstraints.begin()));
    std::set_difference(aConstraints.begin(), aConstraints.end(),
                        commonConstraints.begin(), commonConstraints.end(),
                        std::inserter(aSuffix, aSuffix.end()));
    std::set_difference(bConstraints.begin(), bConstraints.end(),
                        commonConstraints.begin(), commonConstraints.end(),
                        std::inserter(bSuffix, bSuffix.end()));
    if(DebugLogStateMerge) {
        s << "\tconstraint prefix: [";
        for(std::set< ref<Expr> >::iterator it = commonConstraints.begin(),
                        ie = commonConstraints.end(); it != ie; ++it)
            s << *it << ", ";
        s << "]\n";
        s << "\tA suffix: [";
        for(std::set< ref<Expr> >::iterator it = aSuffix.begin(),
                        ie = aSuffix.end(); it != ie; ++it)
            s << *it << ", ";
        s << "]\n";
        s << "\tB suffix: [";
        for(std::set< ref<Expr> >::iterator it = bSuffix.begin(),
                        ie = bSuffix.end(); it != ie; ++it)
        s << *it << ", ";
        s << "]" << '\n';
    }

    /* Check CPUArchState */
    {
        uint8_t* cpuStateA = m_cpuSystemObject->getConcreteStore() - CPU_CONC_LIMIT;
        uint8_t* cpuStateB = b.m_cpuSystemObject->getConcreteStore() - CPU_CONC_LIMIT;
        if(memcmp(cpuStateA + CPU_CONC_LIMIT, cpuStateB + CPU_CONC_LIMIT,
                  CPU_OFFSET(current_tb) - CPU_CONC_LIMIT)) {
            if(DebugLogStateMerge)
                s << "merge failed: different concrete cpu state" << "\n";
            return false;
        }
    }

    // We cannot merge if addresses would resolve differently in the
    // states. This means:
    //
    // 1. Any objects created since the branch in either object must
    // have been free'd.
    //
    // 2. We cannot have free'd any pre-existing object in one state
    // and not the other

    //if(DebugLogStateMerge) {
    //    s << "\tchecking object states\n";
    //    s << "A: " << addressSpace.objects << "\n";
    //    s << "B: " << b.addressSpace.objects << "\n";
    //}

    std::set<const MemoryObject*> mutated;
    MemoryMap::iterator ai = addressSpace.objects.begin();
    MemoryMap::iterator bi = b.addressSpace.objects.begin();
    MemoryMap::iterator ae = addressSpace.objects.end();
    MemoryMap::iterator be = b.addressSpace.objects.end();
    for(; ai!=ae && bi!=be; ++ai, ++bi) {
        if (ai->first != bi->first) {
            if (DebugLogStateMerge) {
                if (ai->first < bi->first) {
                    s << "\t\tB misses binding for: " << ai->first->id << "\n";
                } else {
                    s << "\t\tA misses binding for: " << bi->first->id << "\n";
                }
            }
            if(DebugLogStateMerge)
                s << "merge failed: different callstacks" << '\n';
            return false;
        }
        if(ai->second != bi->second && !ai->first->isValueIgnored &&
                    ai->first != m_cpuSystemState && ai->first != m_dirtyMask) {
            const MemoryObject *mo = ai->first;
            if(DebugLogStateMerge)
                s << "\t\tmutated: " << mo->id << " (" << mo->name << ")\n";
            if(mo->isSharedConcrete) {
                if(DebugLogStateMerge)
                    s << "merge failed: different shared-concrete objects "
                      << '\n';
                return false;
            }
            mutated.insert(mo);
        }
    }
    if(ai!=ae || bi!=be) {
        if(DebugLogStateMerge)
            s << "merge failed: different address maps" << '\n';
        return false;
    }

    // Create state predicates
    ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
    ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
    for(std::set< ref<Expr> >::iterator it = aSuffix.begin(),
                 ie = aSuffix.end(); it != ie; ++it)
        inA = AndExpr::create(inA, *it);
    for(std::set< ref<Expr> >::iterator it = bSuffix.begin(),
                 ie = bSuffix.end(); it != ie; ++it)
        inB = AndExpr::create(inB, *it);

    // XXX should we have a preference as to which predicate to use?
    // it seems like it can make a difference, even though logically
    // they must contradict each other and so inA => !inB

    // merge LLVM stacks

    int selectCountStack = 0, selectCountMem = 0;

    std::vector<StackFrame>::iterator itA = stack.begin();
    std::vector<StackFrame>::const_iterator itB = b.stack.begin();
    for(; itA!=stack.end(); ++itA, ++itB) {
        StackFrame &af = *itA;
        const StackFrame &bf = *itB;
        for(unsigned i=0; i<af.kf->numRegisters; i++) {
            ref<Expr> &av = af.locals[i].value;
            const ref<Expr> &bv = bf.locals[i].value;
            if(av.isNull() || bv.isNull()) {
                // if one is null then by implication (we are at same pc)
                // we cannot reuse this local, so just ignore
            } else {
                if(av != bv) {
                    av = SelectExpr::create(inA, av, bv);
                    selectCountStack += 1;
                }
            }
        }
    }

    if(DebugLogStateMerge)
        s << "\t\tcreated " << selectCountStack << " select expressions on the stack\n";

    for(std::set<const MemoryObject*>::iterator it = mutated.begin(),
                    ie = mutated.end(); it != ie; ++it) {
        const MemoryObject *mo = *it;
        const ObjectState *os = addressSpace.findObject(mo);
        const ObjectState *otherOS = b.addressSpace.findObject(mo);
        assert(os && !os->readOnly &&
               "objects mutated but not writable in merging state");
        assert(otherOS);

        ObjectState *wos = addressSpace.getWriteable(mo, os);
        for (unsigned i=0; i<mo->size; i++) {
            ref<Expr> av = wos->read8(i);
            ref<Expr> bv = otherOS->read8(i);
            if(av != bv) {
                wos->write(i, SelectExpr::create(inA, av, bv));
                selectCountMem += 1;
            }
        }
    }

    if(DebugLogStateMerge)
        s << "\t\tcreated " << selectCountMem << " select expressions in memory\n";

    constraints = ConstraintManager();
    for(std::set< ref<Expr> >::iterator it = commonConstraints.begin(),
                ie = commonConstraints.end(); it != ie; ++it)
        constraints.addConstraint(*it);

    constraints.addConstraint(OrExpr::create(inA, inB));

    // Merge dirty mask by clearing bits that differ. Clearning bits in
    // dirty mask can only affect performance but not correcntess.
    // NOTE: this requires flushing TLB
    {
        const ObjectState* os = addressSpace.findObject(m_dirtyMask);
        ObjectState* wos = addressSpace.getWriteable(m_dirtyMask, os);
        uint8_t* dirtyMaskA = wos->getConcreteStore();
        const uint8_t* dirtyMaskB = b.addressSpace.findObject(m_dirtyMask)->getConcreteStore();

        for(unsigned i = 0; i < m_dirtyMask->size; ++i) {
            if(dirtyMaskA[i] != dirtyMaskB[i])
                dirtyMaskA[i] = 0;
        }
    }

    // Flush TLB
    {
        CPUArchState * cpu;
        cpu = (CPUArchState *) (m_cpuSystemObject->getConcreteStore() - CPU_CONC_LIMIT);
        cpu->current_tb = NULL;

        for (int mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++) {
            for(int i = 0; i < CPU_TLB_SIZE; i++)
                cpu->tlb_table[mmu_idx][i] = s_cputlb_empty_entry;
            for(int i = 0; i < CPU_S2E_TLB_SIZE; i++)
                cpu->s2e_tlb_table[mmu_idx][i].objectState = 0;
        }

        memset (cpu->tb_jmp_cache, 0, TB_JMP_CACHE_SIZE * sizeof (void *));
    }

    return true;
}

CPUArchState *S2EExecutionState::getConcreteCpuState() const
{
    CPUArchState * cpu;
    cpu = (CPUArchState *) (m_cpuSystemState->address - CPU_CONC_LIMIT);
    return cpu;
}


//Fast function to read bytes from physical (uses caching)
void S2EExecutionState::dmaRead(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    while(size > 0) {
        uint64_t hostPage = hostAddress & S2E_RAM_OBJECT_MASK;
        uint64_t length = (hostPage + S2E_RAM_OBJECT_SIZE) - hostAddress;
        if (length > size) {
            length = size;
        }

        ObjectPair op = m_memcache.get(hostPage);
        if (!op.first) {
            op = addressSpace.findObject(hostPage);
            m_memcache.put(hostAddress, op);
        }
        assert(op.first && op.second && op.first->address == hostPage);
        ObjectState *os = const_cast<ObjectState*>(op.second);
        uint8_t *concreteStore;

        unsigned offset = hostAddress & (S2E_RAM_OBJECT_SIZE-1);

        if (op.first->isSharedConcrete) {
            concreteStore = (uint8_t*)op.first->address;
            memcpy(buf, concreteStore + offset, length);
        } else {
            concreteStore = os->getConcreteStore(true);
            for (unsigned i=0; i<length; ++i) {
                if (_s2e_check_concrete(os, offset+i, 1)) {
                    buf[i] = concreteStore[offset+i];
                }else {
                    readRamConcrete(hostAddress+i, &buf[i], sizeof(buf[i]));
                }
            }
        }

        buf+=length;
        hostAddress+=length;
        size -= length;
    }
}

void S2EExecutionState::dmaWrite(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    while(size > 0) {
        uint64_t hostPage = hostAddress & S2E_RAM_OBJECT_MASK;
        uint64_t length = (hostPage + S2E_RAM_OBJECT_SIZE) - hostAddress;
        if (length > size) {
            length = size;
        }


        ObjectPair op = m_memcache.get(hostPage);
        if (!op.first) {
            op = addressSpace.findObject(hostPage);
            m_memcache.put(hostAddress, op);
        }

        assert(op.first && op.second && op.first->address == hostPage);
        ObjectState *os = addressSpace.getWriteable(op.first, op.second);
        uint8_t *concreteStore;

        unsigned offset = hostAddress & (S2E_RAM_OBJECT_SIZE-1);

        if (op.first->isSharedConcrete) {
            concreteStore = (uint8_t*)op.first->address;
            memcpy(concreteStore + offset, buf, length);
        } else {
            concreteStore = os->getConcreteStore(true);

            for (unsigned i=0; i<length; ++i) {
                if (_s2e_check_concrete(os, offset+i, 1)) {
                    concreteStore[offset+i] = buf[i];
                }else {
                    writeRamConcrete(hostAddress+i, &buf[i], sizeof(buf[i]));
                }
            }
        }
        buf+=length;
        hostAddress+=length;
        size -= length;
    }
}


void S2EExecutionState::flushTlbCache()
{
#ifdef S2E_DEBUG_TLBCACHE
    g_s2e->getDebugStream(this) << "Flushing TLB cache\n";
#endif
    m_tlbMap.clear();
}

void S2EExecutionState::flushTlbCachePage(klee::ObjectState *objectState, int mmu_idx, int index)
{
    if (!objectState) {
        return;
    }

    bool found = false;
    TlbMap::iterator tlbIt = m_tlbMap.find(objectState);
    assert(tlbIt != m_tlbMap.end());

    ObjectStateTlbReferences &vec = (*tlbIt).second;
    foreach2(vit, vec.begin(), vec.end()) {
        if ((*vit).first == (unsigned)mmu_idx && (*vit).second == (unsigned)index) {
            vec.erase(vit);
            found = true;
            break;
        }
    }

    assert(found && "Invalid cache!");

    if (vec.empty()) {
#ifdef S2E_DEBUG_TLBCACHE
        g_s2e->getDebugStream(this) << "Erasing cache entry for " <<
                                       (*tlbIt).first << "\n";
#endif
        m_tlbMap.erase(tlbIt);
    }
}

void S2EExecutionState::updateTlbEntry(CPUArchState* env,
                          int mmu_idx, uint64_t virtAddr, uint64_t hostAddr)
{
#ifdef S2E_ENABLE_S2E_TLB
    assert( (hostAddr & ~TARGET_PAGE_MASK) == 0 );
    assert( (virtAddr & ~TARGET_PAGE_MASK) == 0 );

    ObjectPair *ops = m_memcache.getArray(hostAddr);

    unsigned int index = (virtAddr >> S2E_RAM_OBJECT_BITS) & (CPU_S2E_TLB_SIZE - 1);
    for(int i = 0; i < CPU_S2E_TLB_SIZE / CPU_TLB_SIZE; ++i) {
        S2ETLBEntry* entry = &env->s2e_tlb_table[mmu_idx][index];
        ObjectState *oldObjectState = static_cast<ObjectState *>(entry->objectState);

        ObjectPair op;

        if (!ops || !(op = ops[i]).first) {
            op = m_memcache.get(hostAddr);
            if (!op.first) {
                op = addressSpace.findObject(hostAddr);
            }
        }
        assert(op.first && op.second && op.second->getObject() == op.first && op.first->address == hostAddr);

        klee::ObjectState *ros = const_cast<ObjectState*>(op.second);

        if(op.first->isSharedConcrete) {
            entry->objectState = const_cast<klee::ObjectState*>(op.second);
            entry->addend = (hostAddr - virtAddr) | 1;
        } else {
            // XXX: for now we always ensure that all pages in TLB are writable
            klee::ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
            entry->objectState = wos;
            entry->addend = ((uintptr_t) wos->getConcreteStore(true) - virtAddr) | 1;
        }

        op = ObjectPair(op.first, (const ObjectState*)entry->objectState);

        if (!ops) {
            m_memcache.put(hostAddr, op);
            ops = m_memcache.getArray(hostAddr & TARGET_PAGE_MASK);
        }
        ops[i] = op;


        /* Store the new mapping in the cache */
#ifdef S2E_DEBUG_TLBCACHE
        g_s2e->getDebugStream() << std::dec << "Storing " << op.second << " (" << mmu_idx << ',' << index << ")\n";
#endif
        if (oldObjectState != ros) {
            flushTlbCachePage(oldObjectState, mmu_idx, index);
            m_tlbMap[const_cast<ObjectState *>(op.second)].push_back(TlbCoordinates(mmu_idx, index));
        }

        index += 1;
        hostAddr += S2E_RAM_OBJECT_SIZE;
        virtAddr += S2E_RAM_OBJECT_SIZE;
    }
#endif
}


uint8_t S2EExecutionState::readDirtyMask(uint64_t host_address)
{
    uint8_t val=0;
    host_address -= m_dirtyMask->address;
    m_dirtyMaskObject->readConcrete8(host_address, &val);
    return val;
}

void S2EExecutionState::writeDirtyMask(uint64_t host_address, uint8_t val)
{
    host_address -= m_dirtyMask->address;
    m_dirtyMaskObject->write8(host_address, val);
}

void S2EExecutionState::addConstraint(klee::ref<klee::Expr> e)
{
    if (DebugConstraints) {
        if (ConcolicMode) {
            klee::ref<klee::Expr> ce = concolics.evaluate(e);
            assert(ce->isTrue() && "Expression must be true here");
        }

        //Check that the added constraint is consistent with
        //the existing path constraints
        bool truth;
        Solver *solver = g_s2e->getExecutor()->getSolver();
        Query query(constraints,e);
        //bool res = solver->mayBeTrue(query, mayBeTrue);
        bool res = solver->mustBeTrue(query.negateExpr(), truth);
        if (!res || truth) {
           g_s2e->getWarningsStream() << "State has invalid constraints" << '\n';
           exit(-1);
           //g_s2e->getExecutor()->terminateStateEarly(*this, "State has invalid constraint set");
        }
        assert(res && !truth  &&  "state has invalid constraint set");
    }

    constraints.addConstraint(e);
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

extern "C" {

S2EExecutionState* g_s2e_state = NULL;

void s2e_dump_state()
{
    g_s2e_state->dumpCpuState(g_s2e->getDebugStream());
}

uint8_t s2e_read_dirty_mask(uint64_t host_address)
{
    return g_s2e_state->readDirtyMask(host_address);
}

void s2e_write_dirty_mask(uint64_t host_address, uint8_t val)
{
    return g_s2e_state->writeDirtyMask(host_address, val);
}


int s2e_is_ram_registered(S2E *s2e, S2EExecutionState *state,
                               uint64_t host_address)
{
    return state->isRamRegistered(host_address);
}

int s2e_is_ram_shared_concrete(S2E *s2e, S2EExecutionState *state,
                               uint64_t host_address)
{
    return state->isRamSharedConcrete(host_address);
}

void s2e_read_ram_concrete_check(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    assert(state->isRunningConcrete());
    if(state->isSymbolicExecutionEnabled())
        state->readRamConcreteCheck(host_address, buf, size);
    else
        state->readRamConcrete(host_address, buf, size);
}

void s2e_read_ram_concrete(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, void* buf, uint64_t size)
{
    state->readRamConcrete(host_address, (uint8_t*)buf, size);
}

void s2e_write_ram_concrete(S2E *s2e, S2EExecutionState *state,
                    uint64_t host_address, const uint8_t* buf, uint64_t size)
{
    state->writeRamConcrete(host_address, buf, size);
}

void s2e_read_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUArchState* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    /** XXX: use CPUArchState */
    state->readRegisterConcrete(cpuState, offset, buf, size);
}

void s2e_write_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUArchState* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    /** XXX: use CPUArchState */
    state->writeRegisterConcrete(cpuState, offset, buf, size);
}

int s2e_is_zombie(S2EExecutionState* state)
{
    return state->isZombie();
}

int s2e_is_speculative(S2EExecutionState *state)
{
    return state->isSpeculative();
}

int s2e_is_yielded(S2EExecutionState *state)
{
    return state->isYielded();
}

int s2e_is_runnable(S2EExecutionState *state)
{
    return !s2e_is_zombie(state) && !s2e_is_speculative(state) && !s2e_is_yielded(state);
}

} // extern "C"
