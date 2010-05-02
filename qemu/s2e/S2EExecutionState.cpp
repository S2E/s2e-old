extern "C" {
#include "config.h"
#include "qemu-common.h"

int cpu_memory_rw_debug_se(uint64_t addr, uint8_t *buf, int len, int is_write);
//target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr);
}

#include "S2EExecutionState.h"

#include <klee/Context.h>
#include <klee/Memory.h>

#include <s2e/s2e_qemu.h>

namespace s2e {

using namespace klee;

ExecutionState* S2EExecutionState::clone()
{
    return new S2EExecutionState(*this);
}

//Get the program counter in the current state.
//Allows plugins to retrieve it in a hardware-independent manner.
uint64_t S2EExecutionState::getPc() const
{ 
    return cpuState->eip;
}

uint64_t S2EExecutionState::getSp() const
{
    return cpuState->regs[R_ESP];
}

TranslationBlock *S2EExecutionState::getTb() const
{
    return cpuState->s2e_current_tb; 
}

uint64_t S2EExecutionState::getPid() const
{ 
    return cpuState->cr[3];
}

/* XXX: this function belongs to S2EExecutor */
bool S2EExecutionState::readMemoryConcrete(uint64_t address, void *dest, char size)
{
    return cpu_memory_rw_debug_se(address, (uint8_t*)dest, size, 0 ) == 0;
}

uint64_t S2EExecutionState::getPhysicalAddress(uint64_t virtualAddress) const
{
    target_phys_addr_t physicalAddress =
        cpu_get_phys_page_debug(cpuState, virtualAddress & TARGET_PAGE_MASK);
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

        ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
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

    ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
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

        ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
        assert(op.first && op.first->isUserSpecified
               && op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
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

    ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
    assert(op.first && op.first->isUserSpecified
           && op.first->size == TARGET_PAGE_SIZE);

    ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
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

        ObjectPair op = addressSpace.findObject(hostAddress & TARGET_PAGE_MASK);
        assert(op.first && op.first->isUserSpecified
               && op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = addressSpace.getWriteable(op.first, op.second);
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

ref<Expr> S2EExecutionState::createSymbolicValue(
            Expr::Width width, const std::string& name) const
{
    static int lastId = 0;

    const Array *array = new Array(
            !name.empty() ? name : "symb_" + llvm::utostr(++lastId),
            Expr::getMinBytesForWidth(width));
    return Expr::createTempRead(array, width);
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

extern "C" {

S2EExecutionState* g_s2e_state = NULL;

void s2e_update_state_env(
        struct S2EExecutionState* state, CPUX86State* env)
{
	state->cpuState = env;
}

} // extern "C"
