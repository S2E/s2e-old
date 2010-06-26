extern "C" {
#include "config.h"
#include "qemu-common.h"

extern struct CPUX86State *env;
int cpu_memory_rw_debug_se(uint64_t addr, uint8_t *buf, int len, int is_write);
//target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr);
}

#include "S2EExecutionState.h"
#include <s2e/S2EDeviceState.h>
#include <s2e/Plugin.h>

#include <klee/Context.h>
#include <klee/Memory.h>

#include <s2e/s2e_qemu.h>

namespace s2e {

using namespace klee;

int S2EExecutionState::s_lastStateID = 0;

S2EExecutionState::S2EExecutionState(klee::KFunction *kf) :
        klee::ExecutionState(kf), m_stateID(s_lastStateID++),
        m_symbexEnabled(false), m_startSymbexAtPC((uint64_t) -1),
        m_active(true), m_runningConcrete(true),
        m_cpuRegistersState(NULL), m_cpuSystemState(NULL)
{
    m_deviceState = new S2EDeviceState();
    m_cpuRegistersObject = NULL;
    m_cpuSystemObject = NULL;
    m_cpuSystemState = NULL;
    m_cpuRegistersState = NULL;
}

ExecutionState* S2EExecutionState::clone()
{
    S2EExecutionState *ret = new S2EExecutionState(*this);
    ret->m_deviceState = m_deviceState->clone();
    ret->m_stateID = s_lastStateID++;

    //Clone the plugins
    PluginStateMap::iterator it;
    for(it = ret->m_PluginState.begin(); it != ret->m_PluginState.end(); ++it) {
        (*it).second = (*it).second->clone();
    }

    return ret;
}

ref<Expr> S2EExecutionState::readCpuRegister(unsigned offset,
                                             Expr::Width width) const
{
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset + Expr::getMinBytesForWidth(width) <= CPU_OFFSET(eip));

    if(!m_runningConcrete) {
        const ObjectState* os = addressSpace.findObject(m_cpuRegistersState);
        assert(os);
        return os->read(offset, width);
    } else {
        uint64_t ret = 0;
        memcpy((void*) &ret, (void*) (m_cpuRegistersState->address + offset),
                       Expr::getMinBytesForWidth(width));
        return ConstantExpr::create(ret, width);
    }
}

void S2EExecutionState::writeCpuRegister(unsigned offset,
                                         klee::ref<klee::Expr> value)
{
    unsigned width = value->getWidth();
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset + Expr::getMinBytesForWidth(width) <= CPU_OFFSET(eip));

    if(!m_runningConcrete) {
        const ObjectState* os = addressSpace.findObject(m_cpuRegistersState);
        assert(os);
        ObjectState *wos = addressSpace.getWriteable(m_cpuRegistersState, os);
        wos->write(offset, value);

    } else {
        assert(isa<ConstantExpr>(value) &&
               "Can not write symbolic values to registers while executing"
               " in concrete mode. TODO: fix it by longjmping to main loop");
        ConstantExpr* ce = cast<ConstantExpr>(value);
        uint64_t v = ce->getZExtValue(64);
        memcpy((void*) (m_cpuRegistersState->address + offset), (void*) &v,
                    Expr::getMinBytesForWidth(ce->getWidth()));
    }
}

bool S2EExecutionState::readCpuRegisterConcrete(unsigned offset,
                                                void* buf, unsigned size)
{
    assert(size <= 8);
    ref<Expr> expr = readCpuRegister(offset, size*8);
    if(!isa<ConstantExpr>(expr))
        return false;
    uint64_t value = cast<ConstantExpr>(expr)->getZExtValue();
    memcpy(buf, &value, size);
    return true;
}

void S2EExecutionState::writeCpuRegisterConcrete(unsigned offset,
                                                 const void* buf, unsigned size)
{
    uint64_t value = 0;
    memcpy(&value, buf, size);
    writeCpuRegister(offset, ConstantExpr::create(value, size*8));
}

uint64_t S2EExecutionState::readCpuState(unsigned offset,
                                         unsigned width) const
{
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset >= offsetof(CPUX86State, eip));
    assert(offset + Expr::getMinBytesForWidth(width) <= sizeof(CPUX86State));

    uint8_t* address;
    if(m_active) {
        address = (uint8_t*) m_cpuSystemState->address - CPU_OFFSET(eip);
    } else {
        const ObjectState* os = m_cpuSystemObject; //addressSpace.findObject(m_cpuSystemState);
        assert(os);
        address = os->getConcreteStore(); assert(address);
        address -= CPU_OFFSET(eip);
    }

    uint64_t ret = 0;
    memcpy((void*) &ret, address + offset, Expr::getMinBytesForWidth(width));

    if(width == 1)
        ret &= 1;

    return ret;
}

void S2EExecutionState::writeCpuState(unsigned offset, uint64_t value,
                                      unsigned width)
{
    assert((width == 1 || (width&7) == 0) && width <= 64);
    assert(offset >= offsetof(CPUX86State, eip));
    assert(offset + Expr::getMinBytesForWidth(width) <= sizeof(CPUX86State));

    uint8_t* address;
    if(m_active) {
        address = (uint8_t*) m_cpuSystemState->address - CPU_OFFSET(eip);
    } else {
        const ObjectState* os = m_cpuSystemObject; //addressSpace.findObject(m_cpuSystemState);
        assert(os);
        ObjectState *wos = addressSpace.getWriteable(m_cpuSystemState, os);
        address = wos->getConcreteStore(); assert(address);
        address -= CPU_OFFSET(eip);
    }

    if(width == 1)
        value &= 1;
    memcpy(address + offset, (void*) &value, Expr::getMinBytesForWidth(width));
}

//Get the program counter in the current state.
//Allows plugins to retrieve it in a hardware-independent manner.
uint64_t S2EExecutionState::getPc() const
{
    return readCpuState(CPU_OFFSET(eip), 8*sizeof(target_ulong));
}

uint64_t S2EExecutionState::getTotalInstructionCount()
{
    if (!m_cpuSystemState) {
        return 0;
    }
    return readCpuState(CPU_OFFSET(s2e_total_icount), 8*sizeof(uint64_t));
}

void S2EExecutionState::setTbInstructionCount(uint64_t count)
{
    if (!m_cpuSystemState) {
        return;
    }
    writeCpuState(CPU_OFFSET(s2e_tb_icount), count, 8*sizeof(count));
}

void S2EExecutionState::setTotalInstructionCount(uint64_t count)
{
    if (!m_cpuSystemState) {
        return;
    }
    writeCpuState(CPU_OFFSET(s2e_total_icount), count, 8*sizeof(count));
}


uint64_t S2EExecutionState::getSp() const
{
    ref<Expr> e = readCpuRegister(CPU_OFFSET(regs[R_ESP]),
                                  8*sizeof(target_ulong));
    return cast<ConstantExpr>(e)->getZExtValue(64);
}

TranslationBlock *S2EExecutionState::getTb() const
{
    return (TranslationBlock*)
            readCpuState(CPU_OFFSET(s2e_current_tb), 8*sizeof(void*));
}

uint64_t S2EExecutionState::getPid() const
{
    return readCpuState(offsetof(CPUX86State, cr[3]), 8*sizeof(target_ulong));
}

/* XXX: this function belongs to S2EExecutor */
bool S2EExecutionState::readMemoryConcrete(uint64_t address, void *dest, uint64_t size)
{
    uint8_t *d = (uint8_t*)dest;
    while (size>0) {
        ref<Expr> v = readMemory(address, Expr::Int8, false);
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

bool S2EExecutionState::readString(uint64_t address, std::string &s, unsigned maxLen)
{
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
    }while(maxLen>=0);
    return true;
}

bool S2EExecutionState::readUnicodeString(uint64_t address, std::string &s, unsigned maxLen)
{
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
    }while(maxLen>=0);
    return true;
}

ref<Expr> S2EExecutionState::readMemory(uint64_t address,
                            Expr::Width width, bool physical) const
{
    assert(width == 1 || (width & 7) == 0);
    uint64_t size = width / 8;

    uint64_t pageOffset = address & ~TARGET_PAGE_SIZE;
    if(pageOffset + size <= TARGET_PAGE_SIZE) {
        /* Fast path: read belongs to one physical page */
        if(!physical) {
            address = getPhysicalAddress(address);
            if(address == (uint64_t) -1)
                return ref<Expr>(0);
        }

        uint64_t hostAddress = (uint64_t) qemu_get_ram_ptr(address);
        if(!hostAddress)
            return ref<Expr>(0);


        ObjectPair op = fetchObjectStateMem(hostAddress & TARGET_PAGE_MASK, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified
               && op.first->size == TARGET_PAGE_SIZE);

        return op.second->read(pageOffset, width);
    } else {
        /* Access spawns multiple pages (TODO: could optimize it) */
        ref<Expr> res(0);
        for(unsigned i = 0; i != size; ++i) {
            unsigned idx = klee::Context::get().isLittleEndian() ?
                           i : (size - i - 1);
            ref<Expr> byte = readMemory8(address + idx, physical);
            if(byte.isNull()) return ref<Expr>(0);
            res = idx ? ConcatExpr::create(byte, res) : byte;
        }
        return res;
    }
}

ref<Expr> S2EExecutionState::readMemory8(uint64_t address, bool physical) const
{
    if(!physical) {
        address = getPhysicalAddress(address);
        if(address == (uint64_t) -1)
            return ref<Expr>(0);
    }

    uint64_t hostAddress = (uint64_t) qemu_get_ram_ptr(address);
    if(!hostAddress)
        return ref<Expr>(0);

    ObjectPair op = fetchObjectStateMem(hostAddress & TARGET_PAGE_MASK, TARGET_PAGE_MASK);

    assert(op.first && op.first->isUserSpecified
           && op.first->size == TARGET_PAGE_SIZE);

    return op.second->read8(hostAddress & ~TARGET_PAGE_MASK);
}

bool S2EExecutionState::writeMemory(uint64_t address,
                                    ref<Expr> value,
                                    bool physical)
{
    Expr::Width width = value->getWidth();
    assert(width == 1 || (width & 7) == 0);
    ConstantExpr *constantExpr = dyn_cast<ConstantExpr>(value);
    if(constantExpr && width <= 64) {
        // Concrete write of supported width
        uint64_t val = constantExpr->getZExtValue();
        switch (width) {
            case Expr::Bool:
            case Expr::Int8:  return writeMemory8 (address, val, physical);
            case Expr::Int16: return writeMemory16(address, val, physical);
            case Expr::Int32: return writeMemory32(address, val, physical);
            case Expr::Int64: return writeMemory64(address, val, physical);
            default: assert(0);
        }
        return false;

    } else if(width == Expr::Bool) {
        // Boolean write is a special case
        return writeMemory8(address, ZExtExpr::create(value, Expr::Int8),
                            physical);

    } else if((address & ~TARGET_PAGE_MASK) + (width / 8) <= TARGET_PAGE_SIZE) {
        // All bytes belong to a single page

        if(!physical) {
            address = getPhysicalAddress(address);
            if(address == (uint64_t) -1)
                return false;
        }

        uint64_t hostAddress = (uint64_t) qemu_get_ram_ptr(address);
        if(!hostAddress)
            return false;

        ObjectPair op = fetchObjectStateMem(hostAddress & TARGET_PAGE_MASK, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified
               && op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = fetchObjectStateMemWritable(op.first, op.second);
        wos->write(hostAddress & ~TARGET_PAGE_MASK, value);
    } else {
        // Slowest case (TODO: could optimize it)
        unsigned numBytes = width / 8;
        for(unsigned i = 0; i != numBytes; ++i) {
            unsigned idx = Context::get().isLittleEndian() ?
                           i : (numBytes - i - 1);
            if(!writeMemory8(address + idx,
                    ExtractExpr::create(value, 8*i, Expr::Int8), physical)) {
                return false;
            }
        }
    }
    return true;
}

bool S2EExecutionState::writeMemory8(uint64_t address,
                                     ref<Expr> value, bool physical)
{
    assert(value->getWidth() == 8);
    if(!physical) {
        address = getPhysicalAddress(address);
        if(address == (uint64_t) -1)
            return false;
    }

    uint64_t hostAddress = (uint64_t) qemu_get_ram_ptr(address);
    if(!hostAddress)
        return false;

    ObjectPair op = fetchObjectStateMem(hostAddress & TARGET_PAGE_MASK, TARGET_PAGE_MASK);

    assert(op.first && op.first->isUserSpecified
           && op.first->size == TARGET_PAGE_SIZE);

    ObjectState *wos = fetchObjectStateMemWritable(op.first, op.second);
    wos->write(hostAddress & ~TARGET_PAGE_MASK, value);
    return true;
}

bool S2EExecutionState::writeMemory(uint64_t address,
                            uint8_t* buf, Expr::Width width, bool physical)
{
    assert((width & ~7) == 0);
    uint64_t size = width / 8;

    uint64_t pageOffset = address & ~TARGET_PAGE_SIZE;
    if(pageOffset + size <= TARGET_PAGE_SIZE) {
        /* Fast path: write belongs to one physical page */

        if(!physical) {
            address = getPhysicalAddress(address);
            if(address == (uint64_t) -1)
                return false;
        }

        uint64_t hostAddress = (uint64_t) qemu_get_ram_ptr(address);
        if(!hostAddress)
            return false;

        ObjectPair op = fetchObjectStateMem(hostAddress & TARGET_PAGE_MASK, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified
               && op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = fetchObjectStateMemWritable(op.first, op.second);
        for(uint64_t i = 0; i < width / 8; ++i)
            wos->write8(pageOffset + i, buf[i]);

    } else {
        /* Access spawns multiple pages */
        uint64_t size1 = TARGET_PAGE_SIZE - pageOffset;
        if(!writeMemory(address, buf, size1, physical))
            return false;
        if(!writeMemory(address + size1, buf + size1, size - size1))
            return false;
    }
    return true;
}

bool S2EExecutionState::writeMemory8(uint64_t address,
                                     uint8_t value, bool physical)
{
    return writeMemory(address, &value, 8, physical);
}

bool S2EExecutionState::writeMemory16(uint64_t address,
                                     uint16_t value, bool physical)
{
    return writeMemory(address, (uint8_t*) &value, 16, physical);
}

bool S2EExecutionState::writeMemory32(uint64_t address,
                                     uint32_t value, bool physical)
{
    return writeMemory(address, (uint8_t*) &value, 32, physical);
}

bool S2EExecutionState::writeMemory64(uint64_t address,
                                     uint64_t value, bool physical)
{
    return writeMemory(address, (uint8_t*) &value, 64, physical);
}

namespace {
static int _lastSymbolicId = 0;
}

ref<Expr> S2EExecutionState::createSymbolicValue(
            Expr::Width width, const std::string& name)
{

    std::string sname = !name.empty() ? name : "symb_" + llvm::utostr(++_lastSymbolicId);

    const Array *array = new Array(sname, Expr::getMinBytesForWidth(width));

    //Add it to the set of symbolic expressions, to be able to generate
    //test cases later.
    //Dummy memory object
    MemoryObject *mo = new MemoryObject(0, Expr::getMinBytesForWidth(width), false, false, false, NULL);
    mo->setName(sname);

    symbolics.push_back(std::make_pair(mo, array));

    return  Expr::createTempRead(array, width);
}

std::vector<ref<Expr> > S2EExecutionState::createSymbolicArray(
            unsigned size, const std::string& name)
{
    std::string sname = !name.empty() ? name : "symb_" + llvm::utostr(++_lastSymbolicId);
    const Array *array = new Array(sname, size);

    UpdateList ul(array, 0);

    std::vector<ref<Expr> > result; result.reserve(size);
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

    return result;
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

extern "C" {

S2EExecutionState* g_s2e_state = NULL;

void s2e_increment_executed_instructions(uint64_t incr)
{
    uint64_t i = g_s2e_state->getTotalInstructionCount();
    g_s2e_state->setTotalInstructionCount(i + incr);
}


uint64_t s2e_get_executed_instructions()
{
    return g_s2e_state->getTotalInstructionCount();
}

} // extern "C"
