/*
 *  Software MMU support
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
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

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
#define DATA_STYPE int16_t
#elif DATA_SIZE == 1
#define SUFFIX b
#define USUFFIX ub
#define DATA_TYPE uint8_t
#define DATA_STYPE int8_t
#else
#error unsupported data size
#endif

#if ACCESS_TYPE < (NB_MMU_MODES)

#define CPU_MMU_INDEX ACCESS_TYPE
#define MMUSUFFIX _mmu

#elif ACCESS_TYPE == (NB_MMU_MODES)

#define CPU_MMU_INDEX (cpu_mmu_index(env))
#define MMUSUFFIX _mmu

#elif ACCESS_TYPE == (NB_MMU_MODES + 1)

#define CPU_MMU_INDEX (cpu_mmu_index(env))
#define MMUSUFFIX _cmmu

#else
#error invalid ACCESS_TYPE
#endif

#if DATA_SIZE == 8
#define RES_TYPE uint64_t
#else
#define RES_TYPE int
#endif

#if ACCESS_TYPE == (NB_MMU_MODES + 1)
#define ADDR_READ addr_code
#else
#define ADDR_READ addr_read
#endif

#define ADDR_MAX 0xffffffff

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
#define S2E_RAM_OBJECT_DIFF 0

#endif // CONFIG_S2E

#ifdef STATIC_TRANSLATOR

//The static translator uses QEMU's translator as a library and redirects all memory accesses
//to its custom routines.
//Here we simply declare the functions
RES_TYPE glue(glue(ld, USUFFIX), MEMSUFFIX)(target_ulong ptr);

#if DATA_SIZE <= 2
int glue(glue(lds, SUFFIX), MEMSUFFIX)(target_ulong ptr);
#endif

#if ACCESS_TYPE != (NB_MMU_MODES + 1)
/* generic store macro */
void glue(glue(st, SUFFIX), MEMSUFFIX)(target_ulong ptr, RES_TYPE v);
#endif

#else //STATIC_TRANSLATOR

/* generic load/store macros */

static inline RES_TYPE glue(glue(ld, USUFFIX), MEMSUFFIX)(target_ulong ptr)
{
    int object_index, page_index;
    RES_TYPE res;
    target_ulong addr;
    uintptr_t physaddr;
    int mmu_idx;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(ptr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    page_index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);

    mmu_idx = CPU_MMU_INDEX;
    if (unlikely(env->tlb_table[mmu_idx][page_index].ADDR_READ !=
                 (addr & (TARGET_PAGE_MASK | (DATA_SIZE - 1))))) {
        res = glue(glue(__ld, SUFFIX), MMUSUFFIX)(addr, mmu_idx);
    } else {
        //When we get here, the address is aligned with the size of the access,
        //which by definition means that it will fall inside the small page, without overflowing.
        physaddr = addr + env->tlb_table[mmu_idx][page_index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
        S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
        if(likely(_s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE)))
            res = glue(glue(ld, USUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)));
        else
#endif
            res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)physaddr);

        //XXX: Fix this to be on the dataflow
        //res = S2E_TRACE_MEMORY(addr, physaddr, res, 0, 0);
        S2E_TRACE_MEMORY(addr, physaddr, res, 0, 0);
    }
    return res;
}

#if DATA_SIZE <= 2
static inline int glue(glue(lds, SUFFIX), MEMSUFFIX)(target_ulong ptr)
{
    int res, object_index, page_index;
    target_ulong addr;
    uintptr_t physaddr;
    int mmu_idx;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(ptr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    page_index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);

    mmu_idx = CPU_MMU_INDEX;
    if (unlikely(env->tlb_table[mmu_idx][page_index].ADDR_READ !=
                 (addr & (TARGET_PAGE_MASK | (DATA_SIZE - 1))))) {
        res = (DATA_STYPE)glue(glue(__ld, SUFFIX), MMUSUFFIX)(addr, mmu_idx);
    } else {
        physaddr = addr + env->tlb_table[mmu_idx][page_index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
        S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
        if(likely(_s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE)))
            res = glue(glue(lds, SUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)));
        else
#endif
            res = glue(glue(lds, SUFFIX), _raw)((uint8_t *)physaddr);

        S2E_TRACE_MEMORY(addr, physaddr, res, 0, 0);
    }
    return res;
}
#endif

#if ACCESS_TYPE != (NB_MMU_MODES + 1)

/* generic store macro */

static inline void glue(glue(st, SUFFIX), MEMSUFFIX)(target_ulong ptr, RES_TYPE v)
{
    int object_index, page_index;
    target_ulong addr;
    uintptr_t physaddr;
    int mmu_idx;

    addr = S2E_FORK_AND_CONCRETIZE_ADDR(ptr, ADDR_MAX);
    object_index = S2E_FORK_AND_CONCRETIZE(addr >> S2E_RAM_OBJECT_BITS,
                                           ADDR_MAX >> S2E_RAM_OBJECT_BITS);
    page_index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);

    mmu_idx = CPU_MMU_INDEX;
    if (unlikely(env->tlb_table[mmu_idx][page_index].addr_write !=
                 (addr & (TARGET_PAGE_MASK | (DATA_SIZE - 1))))) {
        glue(glue(__st, SUFFIX), MMUSUFFIX)(addr, v, mmu_idx);
    } else {
        physaddr = addr + env->tlb_table[mmu_idx][page_index].addend;

#if defined(CONFIG_S2E) && defined(S2E_ENABLE_S2E_TLB) && !defined(S2E_LLVM_LIB)
        S2ETLBEntry *e = &env->s2e_tlb_table[mmu_idx][object_index & (CPU_S2E_TLB_SIZE-1)];
        if(likely((e->addend & 1) && _s2e_check_concrete(e->objectState, addr & ~S2E_RAM_OBJECT_MASK, DATA_SIZE)))
            glue(glue(st, SUFFIX), _p)((uint8_t*)(addr + (e->addend&~1)), v);
        else
#endif
            glue(glue(st, SUFFIX), _raw)((uint8_t *)physaddr, v);

        S2E_TRACE_MEMORY(addr, physaddr, v, 1, 0);
    }
}

#endif /* ACCESS_TYPE != (NB_MMU_MODES + 1) */

#endif //STATIC_TRANSLATOR

#if ACCESS_TYPE != (NB_MMU_MODES + 1)

#if DATA_SIZE == 8
static inline float64 glue(ldfq, MEMSUFFIX)(target_ulong ptr)
{
    union {
        float64 d;
        uint64_t i;
    } u;
    u.i = glue(ldq, MEMSUFFIX)(ptr);
    return u.d;
}

static inline void glue(stfq, MEMSUFFIX)(target_ulong ptr, float64 v)
{
    union {
        float64 d;
        uint64_t i;
    } u;
    u.d = v;
    glue(stq, MEMSUFFIX)(ptr, u.i);
}
#endif /* DATA_SIZE == 8 */

#if DATA_SIZE == 4
static inline float32 glue(ldfl, MEMSUFFIX)(target_ulong ptr)
{
    union {
        float32 f;
        uint32_t i;
    } u;
    u.i = glue(ldl, MEMSUFFIX)(ptr);
    return u.f;
}

static inline void glue(stfl, MEMSUFFIX)(target_ulong ptr, float32 v)
{
    union {
        float32 f;
        uint32_t i;
    } u;
    u.f = v;
    glue(stl, MEMSUFFIX)(ptr, u.i);
}
#endif /* DATA_SIZE == 4 */

#endif /* ACCESS_TYPE != (NB_MMU_MODES + 1) */


#ifndef CONFIG_S2E
#undef S2E_RAM_OBJECT_BITS
#endif
#undef S2E_RAM_OBJECT_DIFF
#undef S2E_FORK_AND_CONCRETIZE
#undef S2E_TRACE_MEMORY
#undef ADDR_MAX
#undef RES_TYPE
#undef DATA_TYPE
#undef DATA_STYPE
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef CPU_MMU_INDEX
#undef MMUSUFFIX
#undef ADDR_READ
