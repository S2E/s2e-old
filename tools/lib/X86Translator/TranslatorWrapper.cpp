extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <inttypes.h>
#include <stdlib.h>
#include <cassert>
#include "TranslatorWrapper.h"

S2EExecutionState *g_s2e_state = NULL;
S2E *g_s2e = NULL;
CPUState *cpu_single_env;
int singlestep = 0;
int loglevel = 0;
int use_icount = 0;
int64_t qemu_icount = 0;
FILE *logfile = NULL;

#define code_gen_section __attribute__((aligned (32)))
uint8_t code_gen_prologue[1024] code_gen_section;

CPUWriteMemoryFunc *io_mem_write[IO_MEM_NB_ENTRIES][4];
CPUReadMemoryFunc *io_mem_read[IO_MEM_NB_ENTRIES][4];
void *io_mem_opaque[IO_MEM_NB_ENTRIES];


void *qemu_malloc(size_t s)
{
    return malloc(s);
}

void *qemu_mallocz(size_t size)
{
    void *ptr;
    ptr = qemu_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void *qemu_realloc(void *ptr, size_t size)
{
    size_t old_size, copy;
    void *new_ptr;

    if (!ptr)
        return qemu_malloc(size);
    old_size = *(size_t *)((char *)ptr - 16);
    copy = old_size < size ? old_size : size;
    new_ptr = qemu_malloc(size);
    memcpy(new_ptr, ptr, copy);
    qemu_free(ptr);
    return new_ptr;
}



void qemu_free(void *ptr)
{
    free(ptr);
}

void qemu_init_vcpu(void *_env)
{

}

extern "C" {
void target_disas(FILE *out, target_ulong code, target_ulong size, int flags)
{
    assert(false && "Not usable statically");
}

void disas(FILE *out, void *code, unsigned long size)
{
    assert(false && "Not usable statically");
}


const char *lookup_symbol(target_ulong orig_addr)
{
    assert(false && "Not usable statically");
    return NULL;
}
}

TranslationBlock *tb_find_pc(uintptr_t tc_ptr)
{
    assert(false && "Not usable statically");
    return NULL;
}

void s2e_set_tb_function(S2E*, TranslationBlock *tb)
{
    assert(false && "Not usable statically");
}

void s2e_tcg_emit_custom_instruction(S2E*, uint64_t arg)
{
    assert(false && "Not usable statically");
}

void s2e_trace_port_access(
        S2E *s2e, S2EExecutionState* state,
        uint64_t port, uint64_t value, unsigned size,
        int isWrite)
{
    assert(false && "Not usable statically");
}

int s2e_is_port_symbolic(struct S2E *s2e, struct S2EExecutionState* state, uint64_t port)
{
    assert(false && "Not usable statically");
    return 0;
}

int s2e_is_mmio_symbolic(uint64_t address, uint64_t size)
{
    assert(false && "Not usable statically");
    return 0;
}

int s2e_is_mmio_symbolic_b(uint64_t address)
{
    assert(false && "Not usable statically");
    return 0;
}

int s2e_is_mmio_symbolic_w(uint64_t address)
{
    assert(false && "Not usable statically");
    return 0;
}

int s2e_is_mmio_symbolic_l(uint64_t address)
{
    assert(false && "Not usable statically");
    return 0;
}

int s2e_is_mmio_symbolic_q(uint64_t address)
{
    assert(false && "Not usable statically");
    return 0;
}

void s2e_on_translate_block_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(false && "Not usable statically");
}

void s2e_on_translate_block_end(
        S2E* s2e, S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t insPc, int staticTarget, uint64_t targetPc)
{
    assert(false && "Not usable statically");
}

void s2e_on_translate_jump_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, int jump_type)
{
    assert(false && "Not usable statically");
}

void s2e_on_translate_instruction_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(false && "Not usable statically");
}

void s2e_on_translate_instruction_end(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, uint64_t nextpc)
{
    assert(false && "Not usable statically");
}

void s2e_on_page_fault(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    assert(false && "Not usable statically");
}

void s2e_on_tlb_miss(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    assert(false && "Not usable statically");
}

void s2e_trace_memory_access(
        struct S2E *s2e, struct S2EExecutionState* state,
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO)
{
    assert(false && "Not usable statically");
}


void s2e_read_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    assert(false && "Not usable statically");
}

void s2e_write_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    assert(false && "Not usable statically");
}

void s2e_write_ram_concrete(S2E *s2e, S2EExecutionState *state,
                    uint64_t host_address, const uint8_t* buf, uint64_t size)
{
    assert(false && "Not usable statically");
}

void s2e_read_ram_concrete(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    assert(false && "Not usable statically");
}

void s2e_switch_to_symbolic(S2E *s2e, S2EExecutionState *state)
{
    assert(false && "Not usable statically");
}

void s2e_read_ram_concrete_check(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    assert(false && "Not usable statically");
}

/***** CPU-RELATED WRAPPERS *****/
void cpu_outb(uint32_t addr, uint8_t val)
{
    assert(false && "Not usable statically");
}

void cpu_outw(uint32_t addr, uint16_t val)
{
    assert(false && "Not usable statically");
}

void cpu_outl(uint32_t addr, uint32_t val)
{
    assert(false && "Not usable statically");
}

uint8_t cpu_inb(uint32_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint16_t cpu_inw(uint32_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint32_t cpu_inl(uint32_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

int cpu_breakpoint_insert(CPUX86State *env, target_ulong pc, int flags,
                          CPUBreakpoint **breakpoint)
{
    assert(false && "Not usable statically");
}

int cpu_breakpoint_remove(CPUState *env, target_ulong pc, int flags)
{
    assert(false && "Not usable statically");
}

void cpu_breakpoint_remove_all(CPUState *env, int mask)
{
    assert(false && "Not usable statically");
}

void cpu_breakpoint_remove_by_ref(CPUState *env, CPUBreakpoint *breakpoint)
{
    assert(false && "Not usable statically");
}

int cpu_watchpoint_insert(CPUState *env, target_ulong addr, target_ulong len,
                          int flags, CPUWatchpoint **watchpoint)
{
    assert(false && "Not usable statically");
}

void cpu_watchpoint_remove_all(CPUState *env, int mask)
{
    assert(false && "Not usable statically");
}

void cpu_watchpoint_remove_by_ref(CPUState *env, CPUWatchpoint *watchpoint)
{
    assert(false && "Not usable statically");
}

void cpu_io_recompile(CPUState *env, void *retaddr)
{
    assert(false && "Not usable statically");
}

uint64_t cpu_get_tsc(CPUX86State *env)
{
    assert(false && "Not usable statically");
}

void cpu_loop_exit(void)
{
  assert(false && "Not usable statically");
}

void cpu_resume_from_signal(CPUState *env1, void *puc)
{
    assert(false && "Not usable statically");
}

uint8_t cpu_get_apic_tpr(CPUX86State *env)
{
    assert(false && "Not usable statically");
}

void cpu_set_ferr(CPUX86State *s)
{
    assert(false && "Not usable statically");
}

void cpu_smm_update(CPUState *env)
{
    assert(false && "Not usable statically");
}


CPUDebugExcpHandler *cpu_set_debug_excp_handler(CPUDebugExcpHandler *handler)
{
    assert(false && "Not usable statically");
}

void cpu_abort(CPUState *env, const char *fmt, ...)
{
    assert(false && "Not usable statically");
}

void cpu_set_apic_base(CPUState *env, uint64_t val)
{
    assert(false && "Not usable statically");
}

int cpu_memory_rw_debug(CPUState *env, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
    assert(false && "Not usable statically");
}

uint64_t cpu_get_apic_base(CPUState *env)
{
    assert(false && "Not usable statically");
}

void cpu_exec_init(CPUState *env)
{
    assert(false && "Not usable statically");
}

void cpu_set_apic_tpr(CPUX86State *env, uint8_t val)
{
    assert(false && "Not usable statically");
}

void cpu_interrupt(CPUState *env, int mask)
{
    assert(false && "Not usable statically");
}

extern "C" {
void qemu_system_reset_request(void)
{
    assert(false && "Not usable statically");
}


/* Memory operations */
void stq_phys(target_phys_addr_t addr, uint64_t val)
{
    assert(false && "Not usable statically");
}

void stl_phys(target_phys_addr_t addr, uint32_t val)
{
    assert(false && "Not usable statically");
}

void stl_phys_notdirty(target_phys_addr_t addr, uint32_t val)
{

}

void stw_phys(target_phys_addr_t addr, uint32_t val)
{
    assert(false && "Not usable statically");
}

void stb_phys(target_phys_addr_t addr, uint32_t val)
{
    assert(false && "Not usable statically");
}


uint64_t ldq_phys(target_phys_addr_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint32_t ldl_phys(target_phys_addr_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint32_t lduw_phys(target_phys_addr_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint32_t ldub_phys(target_phys_addr_t addr)
{
    assert(false && "Not usable statically");
    return 0;
}

uint64_t __ldq_cmmu(target_ulong addr, int mmu_idx)
{
    assert(false && "Not usable statically");
    return 0;
}

uint32_t __ldl_cmmu(target_ulong addr, int mmu_idx)
{
    assert(false && "Not usable statically");
    return 0;
}

uint16_t __ldw_cmmu(target_ulong addr, int mmu_idx)
{
    assert(false && "Not usable statically");
    return 0;
}

uint8_t __ldb_cmmu(target_ulong addr, int mmu_idx)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldsb_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldub_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldub_kernel(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int lduw_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int lduw_kernel(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int lduw_kernel_s2e_trace(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldsw_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldl_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldl_kernel(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldl_kernel_s2e_trace(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

int ldq_data(target_ulong ptr)
{
    assert(false && "Not usable statically");
    return 0;
}

void stb_data(target_ulong ptr, uint8_t data)
{
    assert(false && "Not usable statically");
}

void stb_kernel(target_ulong ptr, uint8_t data)
{
    assert(false && "Not usable statically");
}

void stw_data(target_ulong ptr, uint16_t data)
{
    assert(false && "Not usable statically");
}

void stw_kernel(target_ulong ptr, uint16_t data)
{
    assert(false && "Not usable statically");
}


void stl_data(target_ulong ptr, uint32_t data)
{
    assert(false && "Not usable statically");
}

void stl_kernel(target_ulong ptr, uint32_t data)
{
    assert(false && "Not usable statically");
}

void stq_data(target_ulong ptr, uint32_t data)
{
    assert(false && "Not usable statically");
}

}

/*******************/
void tlb_flush_page(CPUState *env, target_ulong addr)
{

}

int tlb_set_page_exec(CPUState *env, target_ulong vaddr,
                      target_phys_addr_t paddr, int prot,
                      int mmu_idx, int is_softmmu)
{
    return 0;
}

void tlb_flush(CPUState *env, int flush_global)
{

}

void apic_init_reset(CPUState *env)
{

}

void apic_sipi(CPUState *env)
{

}
