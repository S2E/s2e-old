/*
 *  Software MMU support
 *
 * Generate helpers used by TCG for qemu_ld/st ops and code load
 * functions.
 *
 * Included from target op helpers and exec.c.
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The file was modified for S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */
#include "qemu-timer.h"
#include "memory.h"

#define DATA_SIZE (1 << SHIFT)

#if DATA_SIZE == 8
#define SUFFIX q
#define USUFFIX q
#define DATA_TYPE uint64_t
#elif DATA_SIZE == 4
#define SUFFIX l
#define USUFFIX l
#define DATA_TYPE uint32_t
#elif DATA_SIZE == 2
#define SUFFIX w
#define USUFFIX uw
#define DATA_TYPE uint16_t
#elif DATA_SIZE == 1
#define SUFFIX b
#define USUFFIX ub
#define DATA_TYPE uint8_t
#else
#error unsupported data size
#endif

#ifdef SOFTMMU_CODE_ACCESS
#define READ_ACCESS_TYPE 2
#define ADDR_READ addr_code
#else
#define READ_ACCESS_TYPE 0
#define ADDR_READ addr_read
#endif

#ifndef CONFIG_TCG_PASS_AREG0
#define ENV_PARAM
#define ENV_VAR
#define CPU_PREFIX
#define HELPER_PREFIX __
#else
#define ENV_PARAM CPUArchState *env,
#define ENV_VAR env,
#define CPU_PREFIX cpu_
#define HELPER_PREFIX helper_
#endif

#define ADDR_MAX ((target_ulong)-1)

#ifdef CONFIG_S2E
#include <s2e/s2e_config.h>

#ifdef S2E_LLVM_LIB
#define S2E_TRACE_MEMORY(vaddr, haddr, value, isWrite, isIO) \
    tcg_llvm_trace_memory_access(vaddr, haddr, \
                                 value, 8*sizeof(value), isWrite, isIO);
#define S2E_FORK_AND_CONCRETIZE(val, max) \
    tcg_llvm_fork_and_concretize(val, 0, max)
#else // S2E_LLVM_LIB
#define S2E_TRACE_MEMORY(vaddr, haddr, value, isWrite, isIO) \
    s2e_trace_memory_access(vaddr, haddr, \
                            (uint8_t*) &value, sizeof(value), isWrite, isIO);
#define S2E_FORK_AND_CONCRETIZE(val, max) (val)
#endif // S2E_LLVM_LIB


#define S2E_FORK_AND_CONCRETIZE_ADDR(val, max) \
    (g_s2e_fork_on_symbolic_address ? S2E_FORK_AND_CONCRETIZE(val, max) : val)

#define S2E_RAM_OBJECT_DIFF (TARGET_PAGE_BITS - S2E_RAM_OBJECT_BITS)

#else // CONFIG_S2E

#define S2E_TRACE_MEMORY(...)
#define S2E_FORK_AND_CONCRETIZE(val, max) (val)
#define S2E_FORK_AND_CONCRETIZE_ADDR(val, max) (val)

#define S2E_RAM_OBJECT_BITS TARGET_PAGE_BITS
#define S2E_RAM_OBJECT_SIZE TARGET_PAGE_SIZE
#define S2E_RAM_OBJECT_MASK TARGET_PAGE_MASK
#define S2E_RAM_OBJECT_DIFF 0

#endif // CONFIG_S2E

static DATA_TYPE glue(glue(slow_ld, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                        int mmu_idx,
                                                        void *retaddr);

DATA_TYPE glue(glue(io_read, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                              target_phys_addr_t physaddr,
                                              target_ulong addr,
                                              void *retaddr);

#ifndef S2E_LLVM_LIB
DATA_TYPE glue(glue(io_read, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                              target_phys_addr_t physaddr,
                                              target_ulong addr,
                                              void *retaddr)
{
    DATA_TYPE res;
    MemoryRegion *mr = iotlb_to_region(physaddr);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;

#ifdef CONFIG_S2E
    if (glue(g_s2e_enable_mmio_checks && s2e_is_mmio_symbolic_, SUFFIX)(physaddr)) {
        s2e_switch_to_symbolic(g_s2e, g_s2e_state);
    }
#endif

    env->mem_io_pc = (uintptr_t)retaddr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }

    env->mem_io_vaddr = addr;
#if SHIFT <= 2
    res = io_mem_read(mr, physaddr, 1 << SHIFT);
#else
#ifdef TARGET_WORDS_BIGENDIAN
    res = io_mem_read(mr, physaddr, 4) << 32;
    res |= io_mem_read(mr, physaddr + 4, 4);
#else
    res = io_mem_read(mr, physaddr, 4);
    res |= io_mem_read(mr, physaddr + 4, 4) << 32;
#endif
#endif /* SHIFT > 2 */
    return res;
}

inline DATA_TYPE glue(glue(io_read_chk, SUFFIX), MMUSUFFIX)(ENV_PARAM target_phys_addr_t physaddr,
                                          target_ulong addr,
                                          void *retaddr)
{
    return glue(glue(io_read, SUFFIX), MMUSUFFIX)(ENV_VAR physaddr, addr, retaddr);
}


#elif defined(S2E_LLVM_LIB) //S2E_LLVM_LIB

inline DATA_TYPE glue(io_make_symbolic, SUFFIX)(const char *name) {
    uint8_t ret;
    tcg_llvm_make_symbolic(&ret, sizeof(ret), name);
    return ret;
}


inline DATA_TYPE glue(io_read_chk_symb_, SUFFIX)(const char *label, target_ulong physaddr, uintptr_t pa)
{
    union {
        DATA_TYPE dt;
        uint8_t arr[1<<SHIFT];
    }data;
    unsigned i;

    data.dt = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa));

    for (i = 0; i<(1<<SHIFT); ++i) {
        if (g_s2e_enable_mmio_checks && s2e_is_mmio_symbolic_b(physaddr + i)) {
            data.arr[i] = glue(io_make_symbolic, SUFFIX)(label);
        }
    }
    return data.dt;
}

inline DATA_TYPE glue(glue(io_read_chk, SUFFIX), MMUSUFFIX)(ENV_PARAM target_phys_addr_t physaddr,
                                          target_ulong addr,
                                          void *retaddr)
{
    DATA_TYPE res;
    target_phys_addr_t origaddr = physaddr;
    MemoryRegion *mr = iotlb_to_region(physaddr);

    target_ulong naddr = (physaddr & TARGET_PAGE_MASK)+addr;
    char label[64];
    int isSymb = 0;
    if (g_s2e_enable_mmio_checks && (isSymb = glue(s2e_is_mmio_symbolic_, SUFFIX)(naddr))) {
        //If at least one byte is symbolic, generate a label
#ifdef TARGET_ARM
        trace_port(label, "iommuread_", naddr, env->regs[15]);
#elif defined(TARGET_I386)
        trace_port(label, "iommuread_", naddr, env->eip);
#endif
    }

    //If it is not DMA, then check if it is normal memory
    env->mem_io_pc = (uintptr_t)retaddr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }

    env->mem_io_vaddr = addr;
#if SHIFT <= 2
    if (s2e_ismemfunc(mr, 0)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        if (isSymb) {
            return glue(io_read_chk_symb_, SUFFIX)(ENV_VAR label, naddr, (uintptr_t)(pa));
        }
        res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa));
        return res;
    }
#else
#ifdef TARGET_WORDS_BIGENDIAN
    if (s2e_ismemfunc(mr, 0)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);

        if (isSymb) {
            res = glue(io_read_chk_symb_, SUFFIX)(label, naddr, (uintptr_t)(pa)) << 32;
            res |= glue(io_read_chk_symb_, SUFFIX)(label, naddr,(uintptr_t)(pa+4));
        }else {
            res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa)) << 32;
            res |= glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa+4));
        }

        return res;
    }
#else
    if (s2e_ismemfunc(mr, 0)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        if (isSymb) {
            res = glue(io_read_chk_symb_, SUFFIX)(label, naddr, (uintptr_t)(pa));
            res |= glue(io_read_chk_symb_, SUFFIX)(label, naddr, (uintptr_t)(pa+4)) << 32;
        }else {
            res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa));
            res |= glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(pa + 4)) << 32;
        }
        return res;
    }
#endif
#endif /* SHIFT > 2 */

    //By default, call the original io_read function, which is external
    return glue(glue(io_read, SUFFIX), MMUSUFFIX)(ENV_VAR origaddr, addr, retaddr);
}


#endif

/* handle all cases except unaligned access which span two pages */
DATA_TYPE
glue(glue(glue(HELPER_PREFIX, ld), SUFFIX), MMUSUFFIX)(ENV_PARAM
                                                       target_ulong addr,
                                                       int mmu_idx)
{
    DATA_TYPE res;
    target_ulong object_index, index;
    target_ulong tlb_addr;
    target_phys_addr_t addend, ioaddr;
    void *retaddr = NULL;

    /* test if there is match for unaligned or IO access */
    /* XXX: could done more in memory macro in a non portable way */
    addr = S2E_FORK_AND_CONCRETIZE_ADDR(addr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    if (likely((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK)))) {
        if (unlikely(tlb_addr & ~TARGET_PAGE_MASK)) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
#ifndef S2E_LLVM_LIB
            retaddr = GETPC();
#endif
            ioaddr = env->iotlb[mmu_idx][index];
            res = glue(glue(io_read_chk, SUFFIX), MMUSUFFIX)(ENV_VAR ioaddr, addr, retaddr);

            S2E_TRACE_MEMORY(addr, addr+ioaddr, res, 0, 1);

        } else if (unlikely(((addr & ~S2E_RAM_OBJECT_MASK) + DATA_SIZE - 1) >= S2E_RAM_OBJECT_SIZE)) {
            /* slow unaligned access (it spans two pages or IO) */
        do_unaligned_access:
#ifndef S2E_LLVM_LIB
            retaddr = GETPC();
#endif
#ifdef ALIGNED_ONLY
            do_unaligned_access(ENV_VAR addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
#endif
            res = glue(glue(slow_ld, SUFFIX), MMUSUFFIX)(ENV_VAR addr,
                                                         mmu_idx, retaddr);
        } else {
            /* unaligned/aligned access in the same page */
#ifdef ALIGNED_ONLY
            if ((addr & (DATA_SIZE - 1)) != 0) {
#ifndef S2E_LLVM_LIB
                retaddr = GETPC();
#endif
                do_unaligned_access(ENV_VAR addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
            }
#endif
            addend = env->tlb_table[mmu_idx][index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
            S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
            if(likely(_s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE)))
                res = glue(glue(ld, USUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)));
            else
#endif
                res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(addr+addend));

            S2E_TRACE_MEMORY(addr, addr+addend, res, 0, 0);
        }
    } else {
        /* the page is not in the TLB : fill it */
#ifndef S2E_LLVM_LIB
        retaddr = GETPC();
#endif
#ifdef ALIGNED_ONLY
        if ((addr & (DATA_SIZE - 1)) != 0)
            do_unaligned_access(ENV_VAR addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
#endif
        tlb_fill(env, addr, object_index << S2E_RAM_OBJECT_BITS,
                 READ_ACCESS_TYPE, mmu_idx, retaddr);
        goto redo;
    }

    return res;
}

/* handle all unaligned cases */
static DATA_TYPE
glue(glue(slow_ld, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                       target_ulong addr,
                                       int mmu_idx,
                                       void *retaddr)
{
    DATA_TYPE res, res1, res2;
    target_ulong object_index, index, shift;
    target_phys_addr_t addend, ioaddr;
    target_ulong tlb_addr, addr1, addr2;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(addr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            ioaddr = env->iotlb[mmu_idx][index];
            res = glue(glue(io_read_chk, SUFFIX), MMUSUFFIX)(ENV_VAR ioaddr, addr, retaddr);

            S2E_TRACE_MEMORY(addr, addr+ioaddr, res, 0, 1);
        } else if (((addr & ~S2E_RAM_OBJECT_MASK) + DATA_SIZE - 1) >= S2E_RAM_OBJECT_SIZE) {

        do_unaligned_access:
            /* slow unaligned access (it spans two pages) */
            addr1 = addr & ~(DATA_SIZE - 1);
            addr2 = addr1 + DATA_SIZE;
            res1 = glue(glue(slow_ld, SUFFIX), MMUSUFFIX)(ENV_VAR addr1,
                                                          mmu_idx, retaddr);
            res2 = glue(glue(slow_ld, SUFFIX), MMUSUFFIX)(ENV_VAR addr2,
                                                          mmu_idx, retaddr);
            shift = (addr & (DATA_SIZE - 1)) * 8;
#ifdef TARGET_WORDS_BIGENDIAN
            res = (res1 << shift) | (res2 >> ((DATA_SIZE * 8) - shift));
#else
            res = (res1 >> shift) | (res2 << ((DATA_SIZE * 8) - shift));
#endif
            res = (DATA_TYPE)res;
        } else {
            /* unaligned/aligned access in the same page */
            addend = env->tlb_table[mmu_idx][index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
            S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
            if(_s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE))
                res = glue(glue(ld, USUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)));
            else
#endif
                res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(intptr_t)(addr+addend));

            S2E_TRACE_MEMORY(addr, addr+addend, res, 0, 0);
        }
    } else {
        /* the page is not in the TLB : fill it */
        tlb_fill(env, addr, object_index << S2E_RAM_OBJECT_BITS,
                 READ_ACCESS_TYPE, mmu_idx, retaddr);
        goto redo;
    }
    return res;
}

/*************************************************************************************/

#ifndef SOFTMMU_CODE_ACCESS

static void glue(glue(slow_st, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                                   target_ulong addr,
                                                   DATA_TYPE val,
                                                   int mmu_idx,
                                                   void *retaddr);

void glue(glue(io_write, SUFFIX), MMUSUFFIX)(
                                          ENV_PARAM target_phys_addr_t physaddr,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          void *retaddr);
#ifndef S2E_LLVM_LIB


void glue(glue(io_write, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                             target_phys_addr_t physaddr,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          void *retaddr)
{
    MemoryRegion *mr = iotlb_to_region(physaddr);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }

    env->mem_io_vaddr = addr;
    env->mem_io_pc = (uintptr_t)retaddr;
#if SHIFT <= 2
    io_mem_write(mr, physaddr, val, 1 << SHIFT);
#else
#ifdef TARGET_WORDS_BIGENDIAN
    io_mem_write(mr, physaddr, (val >> 32), 4);
    io_mem_write(mr, physaddr + 4, (uint32_t)val, 4);
#else
    io_mem_write(mr, physaddr, (uint32_t)val, 4);
    io_mem_write(mr, physaddr + 4, val >> 32, 4);
#endif
#endif /* SHIFT > 2 */
}

inline void glue(glue(io_write_chk, SUFFIX), MMUSUFFIX)(ENV_PARAM target_phys_addr_t physaddr,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          void *retaddr)
{
    //XXX: check symbolic memory mapped devices and write log here.
    glue(glue(io_write, SUFFIX), MMUSUFFIX)(ENV_VAR physaddr, val, addr, retaddr);
}

#else


/**
  * Only if compiling for LLVM.
  * This function checks whether a write goes to a clean memory page.
  * If yes, does the write directly.
  * This avoids symbolic values flowing outside the LLVM code and killing the states.
  *
  * It also deals with writes to memory-mapped devices that are symbolic
  */
inline void glue(glue(io_write_chk, SUFFIX), MMUSUFFIX)(ENV_PARAM target_phys_addr_t physaddr,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          void *retaddr)
{
    target_phys_addr_t origaddr = physaddr;
    MemoryRegion *mr = iotlb_to_region(physaddr);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }


    env->mem_io_vaddr = addr;
    env->mem_io_pc = (uintptr_t)retaddr;
#if SHIFT <= 2
    if (s2e_ismemfunc(mr, 1)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        glue(glue(st, SUFFIX), _raw)((uint8_t *)(intptr_t)(pa), val);
        return;
    }
#else
#ifdef TARGET_WORDS_BIGENDIAN
    if (s2e_ismemfunc(s2e_ismemfunc(mr, 1)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        stl_raw((uint8_t *)(intptr_t)(pa), val>>32);
        stl_raw((uint8_t *)(intptr_t)(pa+4), val);
        return;
    }
#else
    if (s2e_ismemfunc(mr, 1)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        stl_raw((uint8_t *)(intptr_t)(pa), val);
        stl_raw((uint8_t *)(intptr_t)(pa+4), val>>32);
        return;
    }
#endif
#endif /* SHIFT > 2 */

    //XXX: Check if MMIO is symbolic, and add corresponding trace entry

    //Since we do not handle symbolic devices for now, we offer the
    //option of concretizing the arguments to I/O helpers.
    if (g_s2e_concretize_io_writes) {
        tcg_llvm_get_value(&val, sizeof(val), true);
    }

    if (g_s2e_concretize_io_addresses) {
        tcg_llvm_get_value(&addr, sizeof(addr), true);
    }

    //By default, call the original io_write function, which is external
    glue(glue(io_write, SUFFIX), MMUSUFFIX)(ENV_VAR origaddr, val, addr, retaddr);
}

#endif


void glue(glue(glue(HELPER_PREFIX, st), SUFFIX), MMUSUFFIX)(ENV_PARAM
                                                            target_ulong addr,
                                                            DATA_TYPE val,
                                                            int mmu_idx)
{
    target_phys_addr_t addend, ioaddr;
    target_ulong tlb_addr;
    void *retaddr = NULL;
    target_ulong object_index, index;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(addr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    if (likely((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK)))) {
        if (unlikely(tlb_addr & ~TARGET_PAGE_MASK)) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
#ifndef S2E_LLVM_LIB
            retaddr = GETPC();
#endif
            ioaddr = env->iotlb[mmu_idx][index];
            glue(glue(io_write_chk, SUFFIX), MMUSUFFIX)(ENV_VAR ioaddr, val, addr, retaddr);

            S2E_TRACE_MEMORY(addr, addr+ioaddr, val, 1, 1);
        } else if (unlikely(((addr & ~S2E_RAM_OBJECT_MASK) + DATA_SIZE - 1) >= S2E_RAM_OBJECT_SIZE)) {

        do_unaligned_access:
#ifndef S2E_LLVM_LIB
            retaddr = GETPC();
#endif
#ifdef ALIGNED_ONLY
            do_unaligned_access(ENV_VAR addr, 1, mmu_idx, retaddr);
#endif
            glue(glue(slow_st, SUFFIX), MMUSUFFIX)(ENV_VAR addr, val,
                                                   mmu_idx, retaddr);
        } else {
            /* aligned/unaligned access in the same page */
#ifdef ALIGNED_ONLY
            if ((addr & (DATA_SIZE - 1)) != 0) {
#ifndef S2E_LLVM_LIB
                retaddr = GETPC();
#endif
                do_unaligned_access(ENV_VAR addr, 1, mmu_idx, retaddr);
            }
#endif
            addend = env->tlb_table[mmu_idx][index].addend;
#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
            S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
            if(likely((e->addend & 1) && _s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE)))
                glue(glue(st, SUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)), val);
            else
#endif
                glue(glue(st, SUFFIX), _raw)((uint8_t *)(intptr_t)(addr+addend), val);

            S2E_TRACE_MEMORY(addr, addr+addend, val, 1, 0);
        }
    } else {
        /* the page is not in the TLB : fill it */
#ifndef S2E_LLVM_LIB
        retaddr = GETPC();
#endif
#ifdef ALIGNED_ONLY
        if ((addr & (DATA_SIZE - 1)) != 0)
            do_unaligned_access(ENV_VAR addr, 1, mmu_idx, retaddr);
#endif
        tlb_fill(env, addr, object_index << S2E_RAM_OBJECT_BITS,
                 1, mmu_idx, retaddr);
        goto redo;
    }
}

/* handles all unaligned cases */
static void glue(glue(slow_st, SUFFIX), MMUSUFFIX)(ENV_PARAM
                                                   target_ulong addr,
                                                   DATA_TYPE val,
                                                   int mmu_idx,
                                                   void *retaddr)
{
    target_phys_addr_t addend, ioaddr;
    target_ulong tlb_addr;
    target_ulong object_index, index;
    int i;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(addr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            ioaddr = env->iotlb[mmu_idx][index];
            glue(glue(io_write_chk, SUFFIX), MMUSUFFIX)(ENV_VAR ioaddr, val, addr, retaddr);

            S2E_TRACE_MEMORY(addr, addr+ioaddr, val, 1, 1);
        } else if (((addr & ~S2E_RAM_OBJECT_MASK) + DATA_SIZE - 1) >= S2E_RAM_OBJECT_SIZE) {

        do_unaligned_access:
            /* XXX: not efficient, but simple */
            /* Note: relies on the fact that tlb_fill() does not remove the
             * previous page from the TLB cache.  */
            for(i = DATA_SIZE - 1; i >= 0; i--) {
#ifdef TARGET_WORDS_BIGENDIAN
                glue(slow_stb, MMUSUFFIX)(ENV_VAR addr + i,
                                          val >> (((DATA_SIZE - 1) * 8) - (i * 8)),
                                          mmu_idx, retaddr);
#else
                glue(slow_stb, MMUSUFFIX)(ENV_VAR addr + i,
                                          val >> (i * 8),
                                          mmu_idx, retaddr);
#endif
            }
        } else {
            /* aligned/unaligned access in the same page */
            addend = env->tlb_table[mmu_idx][index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
            S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
            if((e->addend & 1) && _s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE))
                glue(glue(st, SUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)), val);
            else
#endif
                glue(glue(st, SUFFIX), _raw)((uint8_t *)(intptr_t)(addr+addend), val);

            S2E_TRACE_MEMORY(addr, addr+addend, val, 1, 0);
        }
    } else {
        /* the page is not in the TLB : fill it */
        tlb_fill(env, addr, object_index << S2E_RAM_OBJECT_BITS,
                 1, mmu_idx, retaddr);
        goto redo;
    }
}

#endif /* !defined(SOFTMMU_CODE_ACCESS) */

#ifndef CONFIG_S2E
#undef S2E_RAM_OBJECT_BITS
#undef S2E_RAM_OBJECT_SIZE
#undef S2E_RAM_OBJECT_MASK
#endif
#undef S2E_FORK_AND_CONCRETIZE_ADDR
#undef S2E_FORK_AND_CONCRETIZE
#undef S2E_TRACE_MEMORY
#undef ADDR_MAX
#undef READ_ACCESS_TYPE
#undef SHIFT
#undef DATA_TYPE
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef ADDR_READ
#undef ENV_PARAM
#undef ENV_VAR
#undef CPU_PREFIX
#undef HELPER_PREFIX
