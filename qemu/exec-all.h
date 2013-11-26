/*
 * internal execution defines for qemu
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

#ifndef _EXEC_ALL_H_
#define _EXEC_ALL_H_

#include "qemu-common.h"

/* allow to see translation results - the slowdown should be negligible, so we leave it */
#define DEBUG_DISAS

/* Page tracking code uses ram addresses in system mode, and virtual
   addresses in userspace mode.  Define tb_page_addr_t to be an appropriate
   type.  */
#if defined(CONFIG_USER_ONLY)
typedef abi_ulong tb_page_addr_t;
#else
typedef ram_addr_t tb_page_addr_t;
#endif

/* is_jmp field values */
#define DISAS_NEXT    0 /* next instruction can be analyzed */
#define DISAS_JUMP    1 /* only pc was modified dynamically */
#define DISAS_UPDATE  2 /* cpu state was modified dynamically */
#define DISAS_TB_JUMP 3 /* only pc was modified statically */

struct TranslationBlock;
typedef struct TranslationBlock TranslationBlock;

/* XXX: make safe guess about sizes */
#define MAX_OP_PER_INSTR 208

#if HOST_LONG_BITS == 32
#define MAX_OPC_PARAM_PER_ARG 2
#else
#define MAX_OPC_PARAM_PER_ARG 1
#endif
#define MAX_OPC_PARAM_IARGS 4
#define MAX_OPC_PARAM_OARGS 1
#define MAX_OPC_PARAM_ARGS (MAX_OPC_PARAM_IARGS + MAX_OPC_PARAM_OARGS)

/* A Call op needs up to 4 + 2N parameters on 32-bit archs,
 * and up to 4 + N parameters on 64-bit archs
 * (N = number of input arguments + output arguments).  */
#define MAX_OPC_PARAM (4 + (MAX_OPC_PARAM_PER_ARG * MAX_OPC_PARAM_ARGS))
#define OPC_BUF_SIZE 640
#define OPC_MAX_SIZE (OPC_BUF_SIZE - MAX_OP_PER_INSTR)

/* Maximum size a TCG op can expand to.  This is complicated because a
   single op may require several host instructions and register reloads.
   For now take a wild guess at 192 bytes, which should allow at least
   a couple of fixup instructions per argument.  */
#define TCG_MAX_OP_SIZE 192

#define OPPARAM_BUF_SIZE (OPC_BUF_SIZE * MAX_OPC_PARAM)

extern target_ulong gen_opc_pc[OPC_BUF_SIZE];
extern uint8_t gen_opc_instr_start[OPC_BUF_SIZE];
extern uint16_t gen_opc_icount[OPC_BUF_SIZE];

#include "qemu-log.h"

void gen_intermediate_code(CPUArchState *env, struct TranslationBlock *tb);
void gen_intermediate_code_pc(CPUArchState *env, struct TranslationBlock *tb);
void restore_state_to_opc(CPUArchState *env, struct TranslationBlock *tb,
                          int pc_pos);


#ifdef CONFIG_S2E
#define INSTRUCTION_SET_ARM 0
#define INSTRUCTION_SET_THUMB 1
#endif

#ifdef CONFIG_S2E
int cpu_gen_llvm(CPUArchState *env, TranslationBlock *tb);
#endif


void cpu_gen_init(void);
int cpu_gen_code(CPUArchState *env, struct TranslationBlock *tb,
                 int *gen_code_size_ptr);
int cpu_restore_state(struct TranslationBlock *tb,
                      CPUArchState *env, uintptr_t searched_pc);
void QEMU_NORETURN cpu_resume_from_signal(CPUArchState *env1, void *puc);
void QEMU_NORETURN cpu_io_recompile(CPUArchState *env, void *retaddr);
TranslationBlock *tb_gen_code(CPUArchState *env, 
                              target_ulong pc, target_ulong cs_base, int flags,
                              int cflags);
void cpu_exec_init(CPUArchState *env);
void QEMU_NORETURN cpu_loop_exit(CPUArchState *env1);
int page_unprotect(target_ulong address, uintptr_t pc, void *puc);
void tb_invalidate_phys_page_range(tb_page_addr_t start, tb_page_addr_t end,
                                   int is_cpu_write_access);
void tlb_flush_page(CPUArchState *env, target_ulong addr);
void tlb_flush(CPUArchState *env, int flush_global);
#if !defined(CONFIG_USER_ONLY)
void tlb_set_page(CPUArchState *env, target_ulong vaddr,
                  target_phys_addr_t paddr, int prot,
                  int mmu_idx, target_ulong size);
#endif

#define CODE_GEN_ALIGN           16 /* must be >= of the size of a icache line */

#define CODE_GEN_PHYS_HASH_BITS     15
#define CODE_GEN_PHYS_HASH_SIZE     (1 << CODE_GEN_PHYS_HASH_BITS)

#define MIN_CODE_GEN_BUFFER_SIZE     (128 * 1024 * 1024)

/* estimated block size for TB allocation */
/* XXX: use a per code average code fragment size and modulate it
   according to the host CPU */
#if defined(CONFIG_SOFTMMU)
#define CODE_GEN_AVG_BLOCK_SIZE 128
#else
#define CODE_GEN_AVG_BLOCK_SIZE 64
#endif

#if defined(_ARCH_PPC) || defined(__x86_64__) || defined(__arm__) || defined(__i386__)
#define USE_DIRECT_JUMP
#elif defined(CONFIG_TCG_INTERPRETER)
#define USE_DIRECT_JUMP
#endif

#ifdef CONFIG_LLVM
struct TCGLLVMTranslationBlock;
struct TCGLLVMContext;
#ifdef __cplusplus
namespace llvm { class Function; }
namespace s2e { class S2ETranslationBlock; }
using llvm::Function;
using s2e::S2ETranslationBlock;
#else
struct Function;
struct S2ETranslationBlock;
#endif
#endif

enum ETranslationBlockType
{
    TB_DEFAULT=0,
    TB_JMP, TB_JMP_IND,
    TB_COND_JMP, TB_COND_JMP_IND,
    TB_CALL, TB_CALL_IND, TB_REP, TB_RET
};

#ifdef CONFIG_S2E
enum JumpType
{
    JT_RET, JT_LRET
};
#endif


struct TranslationBlock {
    target_ulong pc;   /* simulated PC corresponding to this block (EIP + CS base) */
    target_ulong cs_base; /* CS base for this block */
    uint64_t flags; /* flags defining in which context the code was generated */
    uint16_t size;      /* size of target code for this block (1 <=
                           size <= TARGET_PAGE_SIZE) */
    uint16_t cflags;    /* compile flags */
#define CF_COUNT_MASK  0x7fff
#define CF_LAST_IO     0x8000 /* Last insn may be an IO access.  */

    uint8_t *tc_ptr;    /* pointer to the translated code */
    /* next matching tb for physical address. */
    struct TranslationBlock *phys_hash_next;
    /* first and second physical page containing code. The lower bit
       of the pointer tells the index in page_next[] */
    struct TranslationBlock *page_next[2];
    tb_page_addr_t page_addr[2];

    /* the following data are used to directly call another TB from
       the code of this one. */
    uint16_t tb_next_offset[2]; /* offset of original jump target */
#ifdef USE_DIRECT_JUMP
    uint16_t tb_jmp_offset[2]; /* offset of jump instruction */
#else
    uintptr_t tb_next[2]; /* address of jump generated code */
#endif
    /* list of TBs jumping to this one. This is a circular list using
       the two least significant bits of the pointers to tell what is
       the next pointer: 0 = jmp_next[0], 1 = jmp_next[1], 2 =
       jmp_first */
    struct TranslationBlock *jmp_next[2];
    struct TranslationBlock *jmp_first;
    uint32_t icount;

#ifdef CONFIG_LLVM
    /* pointer to LLVM translated code */
    struct TCGLLVMContext *tcg_llvm_context;
    struct Function *llvm_function;
    uint8_t *llvm_tc_ptr;
    uint8_t *llvm_tc_end;
    struct TranslationBlock* llvm_tb_next[2];
#endif

#ifdef CONFIG_S2E
    uint64_t reg_rmask; /* Registers that TB reads (before overwritting) */
    uint64_t reg_wmask; /* Registers that TB writes */
    uint64_t helper_accesses_mem; /* True if contains helpers that access mem */

    enum ETranslationBlockType s2e_tb_type;
    struct S2ETranslationBlock* s2e_tb;
    struct TranslationBlock* s2e_tb_next[2];
    uint64_t pcOfLastInstr; /* XXX: hack for call instructions */
    uint32_t instruction_set;
#endif

};

static inline unsigned int tb_jmp_cache_hash_page(target_ulong pc)
{
    target_ulong tmp;
    tmp = pc ^ (pc >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS));
    return (tmp >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS)) & TB_JMP_PAGE_MASK;
}

static inline unsigned int tb_jmp_cache_hash_func(target_ulong pc)
{
    target_ulong tmp;
    tmp = pc ^ (pc >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS));
    return (((tmp >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS)) & TB_JMP_PAGE_MASK)
	    | (tmp & TB_JMP_ADDR_MASK));
}

static inline unsigned int tb_phys_hash_func(tb_page_addr_t pc)
{
    return (pc >> 2) & (CODE_GEN_PHYS_HASH_SIZE - 1);
}

void tb_free(TranslationBlock *tb);
void tb_flush(CPUArchState *env);
void tb_link_page(TranslationBlock *tb,
                  tb_page_addr_t phys_pc, tb_page_addr_t phys_page2);
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr);

extern TranslationBlock *tb_phys_hash[CODE_GEN_PHYS_HASH_SIZE];

#if defined(USE_DIRECT_JUMP)

#if defined(CONFIG_TCG_INTERPRETER)
static inline void tb_set_jmp_target1(uintptr_t jmp_addr, uintptr_t addr)
{
    /* patch the branch destination */
    *(uint32_t *)jmp_addr = addr - (jmp_addr + 4);
    /* no need to flush icache explicitly */
}
#elif defined(_ARCH_PPC)
void ppc_tb_set_jmp_target(unsigned long jmp_addr, unsigned long addr);
#define tb_set_jmp_target1 ppc_tb_set_jmp_target
#elif defined(__i386__) || defined(__x86_64__)
static inline void tb_set_jmp_target1(uintptr_t jmp_addr, uintptr_t addr)
{
    /* patch the branch destination */
    *(uint32_t *)jmp_addr = addr - (jmp_addr + 4);
    /* no need to flush icache explicitly */
}
#elif defined(__arm__)
static inline void tb_set_jmp_target1(uintptr_t jmp_addr, uintptr_t addr)
{
#if !QEMU_GNUC_PREREQ(4, 1)
    register unsigned long _beg __asm ("a1");
    register unsigned long _end __asm ("a2");
    register unsigned long _flg __asm ("a3");
#endif

    /* we could use a ldr pc, [pc, #-4] kind of branch and avoid the flush */
    *(uint32_t *)jmp_addr =
        (*(uint32_t *)jmp_addr & ~0xffffff)
        | (((addr - (jmp_addr + 8)) >> 2) & 0xffffff);

#if QEMU_GNUC_PREREQ(4, 1)
    __builtin___clear_cache((char *) jmp_addr, (char *) jmp_addr + 4);
#else
    /* flush icache */
    _beg = jmp_addr;
    _end = jmp_addr + 4;
    _flg = 0;
    __asm __volatile__ ("swi 0x9f0002" : : "r" (_beg), "r" (_end), "r" (_flg));
#endif
}
#else
#error tb_set_jmp_target1 is missing
#endif

static inline void tb_set_jmp_target(TranslationBlock *tb,
                                     int n, uintptr_t addr)
{
    uint16_t offset = tb->tb_jmp_offset[n];
    tb_set_jmp_target1((uintptr_t)(tb->tc_ptr + offset), addr);
}

#else

/* set the jump target */
static inline void tb_set_jmp_target(TranslationBlock *tb,
                                     int n, uintptr_t addr)
{
    tb->tb_next[n] = addr;
}

#endif

static inline void tb_add_jump(TranslationBlock *tb, int n,
                               TranslationBlock *tb_next)
{
    /* NOTE: this test is only needed for thread safety */
    if (!tb->jmp_next[n]) {
        /* patch the native jump address */
        tb_set_jmp_target(tb, n, (uintptr_t)tb_next->tc_ptr);

        /* add in TB jmp circular list */
        tb->jmp_next[n] = tb_next->jmp_first;
        tb_next->jmp_first = (TranslationBlock *)((uintptr_t)(tb) | (n));
#ifdef CONFIG_S2E
        tb->s2e_tb_next[n] = tb_next;
#endif
#ifdef CONFIG_LLVM
        tb->llvm_tb_next[n] = tb_next;
#endif
    }
}

TranslationBlock *tb_find_pc(uintptr_t pc_ptr);

#include "qemu-lock.h"

extern spinlock_t tb_lock;

extern int tb_invalidated_flag;

/* The return address may point to the start of the next instruction.
   Subtracting one gets us the call instruction itself.  */
#if defined(CONFIG_TCG_INTERPRETER)
/* Alpha and SH4 user mode emulations and Softmmu call GETPC().
   For all others, GETPC remains undefined (which makes TCI a little faster. */
# if defined(CONFIG_SOFTMMU) || defined(TARGET_ALPHA) || defined(TARGET_SH4)
extern void *tci_tb_ptr;
#  define GETPC() tci_tb_ptr
# endif
#elif defined(__s390__) && !defined(__s390x__)
# define GETPC() \
    ((void *)(((uintptr_t)__builtin_return_address(0) & 0x7fffffffUL) - 1))
#elif defined(__arm__)
/* Thumb return addresses have the low bit set, so we need to subtract two.
   This is still safe in ARM mode because instructions are 4 bytes.  */
# define GETPC() ((void *)((uintptr_t)__builtin_return_address(0) - 2))
#else
# define GETPC() ((void *)((uintptr_t)__builtin_return_address(0) - 1))
#endif

#if !defined(CONFIG_USER_ONLY)

struct MemoryRegion *iotlb_to_region(target_phys_addr_t index);
uint64_t io_mem_read(struct MemoryRegion *mr, target_phys_addr_t addr,
                     unsigned size);
void io_mem_write(struct MemoryRegion *mr, target_phys_addr_t addr,
                  uint64_t value, unsigned size);

void tlb_fill(CPUArchState *env1, target_ulong addr, target_ulong page_addr,
              int is_write, int mmu_idx,
              void *retaddr);

#include "softmmu_defs.h"

#define ACCESS_TYPE (NB_MMU_MODES + 1)
#define MEMSUFFIX _code
#ifndef CONFIG_TCG_PASS_AREG0
#define env cpu_single_env
#endif

#define DATA_SIZE 1
#include "softmmu_header.h"

#define DATA_SIZE 2
#include "softmmu_header.h"

#define DATA_SIZE 4
#include "softmmu_header.h"

#define DATA_SIZE 8
#include "softmmu_header.h"

#undef ACCESS_TYPE
#undef MEMSUFFIX
#undef env

#endif

#if defined(CONFIG_USER_ONLY)
static inline tb_page_addr_t get_page_addr_code(CPUArchState *env1, target_ulong addr)
{
    return addr;
}
#else
tb_page_addr_t get_page_addr_code(CPUArchState *env1, target_ulong addr);
#endif

typedef void (CPUDebugExcpHandler)(CPUArchState *env);

CPUDebugExcpHandler *cpu_set_debug_excp_handler(CPUDebugExcpHandler *handler);

/* vl.c */
extern int singlestep;

/* cpu-exec.c */
extern volatile sig_atomic_t exit_request;

/* Deterministic execution requires that IO only be performed on the last
   instruction of a TB so that interrupts take effect immediately.  */
static inline int can_do_io(CPUArchState *env)
{
    if (!use_icount) {
        return 1;
    }
    /* If not executing code then assume we are ok.  */
    if (!env->current_tb) {
        return 1;
    }
    return env->can_do_io != 0;
}

#if !defined(CONFIG_USER_ONLY)
ram_addr_t last_ram_offset(void);
#endif

extern int generate_llvm;
extern int execute_llvm;
extern const int has_llvm_engine;

#endif
