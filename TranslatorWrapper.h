#ifndef STATIC_TRANSLATOR_WRAPPER_H

#define STATIC_TRANSLATOR_WRAPPER_H

class S2E {

};

class S2EExecutionState  {

};



#ifdef __cplusplus
extern "C" {
#endif

struct TranslationBlock;
struct CPUX86State;

#if 0

void s2e_tcg_emit_custom_instruction(struct S2E*, uint64_t arg);

void s2e_trace_port_access(
        S2E *s2e, S2EExecutionState* state,
        uint64_t port, uint64_t value, unsigned size,
        int isWrite);

int s2e_is_port_symbolic(struct S2E *s2e, struct S2EExecutionState* state, uint64_t port);
int s2e_is_mmio_symbolic(uint64_t address, uint64_t size);
int s2e_is_mmio_symbolic_b(uint64_t address);
int s2e_is_mmio_symbolic_w(uint64_t address);
int s2e_is_mmio_symbolic_l(uint64_t address);
int s2e_is_mmio_symbolic_q(uint64_t address);



void s2e_on_translate_block_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc);

void s2e_on_translate_block_end(
        S2E* s2e, S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t insPc, int staticTarget, uint64_t targetPc);

void s2e_on_translate_jump_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, int jump_type);

void s2e_on_translate_instruction_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc);

void s2e_on_translate_instruction_end(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, uint64_t nextpc);

void s2e_on_page_fault(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write);
void s2e_on_tlb_miss(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write);

void s2e_trace_memory_access(
        struct S2E *s2e, struct S2EExecutionState* state,
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO);


void s2e_read_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size);

void s2e_write_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size);

void s2e_write_ram_concrete(S2E *s2e, S2EExecutionState *state,
                    uint64_t host_address, const uint8_t* buf, uint64_t size);

void s2e_read_ram_concrete(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size);

void s2e_switch_to_symbolic(S2E *s2e, S2EExecutionState *state);

void s2e_read_ram_concrete_check(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size);

#endif

#if 0
void cpu_outb(uint32_t addr, uint8_t val);
void cpu_outw(uint32_t addr, uint16_t val);
void cpu_outl(uint32_t addr, uint32_t val);
uint8_t cpu_inb(uint32_t addr);
uint16_t cpu_inw(uint32_t addr);
uint32_t cpu_inl(uint32_t addr);
#endif

#if 0
int cpu_breakpoint_insert(CPUX86State *env, target_ulong pc, int flags,
                          CPUBreakpoint **breakpoint);

int cpu_breakpoint_remove(CPUState *env, target_ulong pc, int flags);
void cpu_breakpoint_remove_by_ref(CPUState *env, CPUBreakpoint *breakpoint);
void cpu_watchpoint_remove_all(CPUX86State *env, int mask);
#endif

#ifdef __cplusplus
}
#endif

#endif
