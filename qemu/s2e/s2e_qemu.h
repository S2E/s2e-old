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
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_QEMU_H
#define S2E_QEMU_H

#include <inttypes.h>

#ifdef __cplusplus
namespace s2e {
    struct S2E;
    struct S2EExecutionState;
    struct S2ETranslationBlock;
}
using s2e::S2E;
using s2e::S2EExecutionState;
using s2e::S2ETranslationBlock;
#else
struct S2E;
struct S2EExecutionState;
struct S2ETranslationBlock;

#endif

struct TranslationBlock;
struct TCGLLVMContext;
struct S2ETLBEntry;


// XXX
struct CPUX86State;

#ifdef __cplusplus
extern "C" {
#endif



/* This should never be accessed from C++ code */
extern struct S2E* g_s2e;
struct PCIBus;
/* This should never be accessed from C++ code */
extern struct S2EExecutionState* g_s2e_state;

/**************************/
/* Functions from S2E.cpp */

/** Initialize S2E instance. Called by main() */
struct S2E* s2e_initialize(int argc, char** argv,
                           struct TCGLLVMContext *tcgLLVMContext,
                           const char *s2e_config_file,
                           const char *s2e_output_dir,
                           int verbose, unsigned max_processes);

/** Relese S2E instance and all S2E-related objects. Called by main() */
void s2e_close(struct S2E* s2e);
void s2e_close_arg(void);


void s2e_debug_print(const char *fmtstr, ...);
void print_stacktrace(void);

void s2e_print_apic(struct CPUX86State *env);


/*********************************/
/* Functions from CorePlugin.cpp */

void s2e_tcg_execution_handler(void* signal, uint64_t pc);
void s2e_tcg_custom_instruction_handler(uint64_t arg);

/** Called by the translator when a custom instruction is detected */
void s2e_tcg_emit_custom_instruction(struct S2E* s2e, uint64_t arg);

/** Called by cpu_gen_code() at the beginning of translation process */
void s2e_on_translate_block_start(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock *tb, uint64_t pc);

/** Called by cpu_gen_code() before the execution would leave the tb.
    staticTarget is 1 when the target pc at the end of the tb is known */
void s2e_on_translate_block_end(
        struct S2E* s2e, 
        struct S2EExecutionState *state, 
        struct TranslationBlock *tb, uint64_t insPc,
        int staticTarget, uint64_t targetPc);


/** Called by cpu_gen_code() before translation of each instruction */
void s2e_on_translate_instruction_start(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb, uint64_t pc);

/** Called by cpu_gen_code() after translation of each instruction */
void s2e_on_translate_instruction_end(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb, uint64_t pc, uint64_t nextpc);

/** Called by cpu_gen_code() before translation of each jump instruction */
void s2e_on_translate_jump_start(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb, uint64_t pc,
        int jump_type);

void s2e_on_translate_register_access(
        struct TranslationBlock *tb, uint64_t pc,
        uint64_t readMask, uint64_t writeMask, int isMemoryAccess);

void s2e_on_exception(unsigned intNb);

/** Called on memory accesses from generated code */
void s2e_trace_memory_access(
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO);

/** Called on port access from helper code */
void s2e_trace_port_access(
        struct S2E *s2e, struct S2EExecutionState* state,
        uint64_t port, uint64_t value, unsigned bits,
        int isWrite);

void s2e_on_page_fault(struct S2E *s2e, struct S2EExecutionState* state, uint64_t addr, int is_write);
void s2e_on_tlb_miss(struct S2E *s2e, struct S2EExecutionState* state, uint64_t addr, int is_write);

/**********************************/
/* Functions from S2EExecutor.cpp */

/** Variable that holds the latest return address when
    executiong helper code from KLEE */
//extern void* g_s2e_exec_ret_addr;

/** Global variable that determines whether to fork on
    symbolic memory addresses */
extern int g_s2e_fork_on_symbolic_address;

/** Global variable that determines whether to make
    symbolic I/O memory addresses concrete */
extern int g_s2e_concretize_io_addresses;

/** Global variable that determines whether to make
    symbolic I/O writes concrete */
extern int g_s2e_concretize_io_writes;


/** Create initial S2E execution state */
struct S2EExecutionState* s2e_create_initial_state(struct S2E *s2e);

/** Initialize symbolic execution machinery. Should be called after
    QEMU pc is completely constructed */
void s2e_initialize_execution(struct S2E *s2e,
                              struct S2EExecutionState *initial_state,
                              int execute_always_klee);

void s2e_register_cpu(struct S2E* s2e,
                      struct S2EExecutionState *initial_state,
                      struct CPUX86State* cpu_env);

void s2e_register_ram(struct S2E* s2e,
                      struct S2EExecutionState *initial_state,
                      uint64_t start_address, uint64_t size,
                      uint64_t host_address, int is_shared_concrete,
                      int save_on_context_switch, const char *name);

int s2e_is_ram_registered(struct S2E* s2e,
                          struct S2EExecutionState *state,
                          uint64_t host_address);

int s2e_is_ram_shared_concrete(struct S2E* s2e,
                          struct S2EExecutionState *state,
                          uint64_t host_address);

void s2e_read_ram_concrete(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, void* buf, uint64_t size);

void s2e_write_ram_concrete(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, const uint8_t* buf, uint64_t size);

void s2e_read_register_concrete(struct S2E* s2e,
        struct S2EExecutionState* state, struct CPUX86State* cpuState,
        unsigned offset, uint8_t* buf, unsigned size);

void s2e_write_register_concrete(struct S2E* s2e,
        struct S2EExecutionState* state, struct CPUX86State* cpuState,
        unsigned offset, uint8_t* buf, unsigned size);

/* helpers that should be run as LLVM functions */
void s2e_set_cc_op_eflags(struct CPUX86State *state);

void s2e_do_interrupt(struct S2E* s2e, struct S2EExecutionState* state,
                      int intno, int is_int, int error_code,
                      uint64_t next_eip, int is_hw);

/** This function is called when RAM is read by concretely executed
    generated code. If the memory location turns out to be symbolic,
    this function will either concretize it of switch to execution
    in KLEE */
void s2e_read_ram_concrete_check(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, uint8_t* buf, uint64_t size);

struct S2EExecutionState* s2e_select_next_state(
        struct S2E* s2e, struct S2EExecutionState* state);

/** Allocate S2E parts of the tanslation block. Called from tb_alloc() */
void s2e_tb_alloc(struct S2E* s2e, struct TranslationBlock *tb);

/** Free S2E parts of the translation block. Called from tb_flush() and tb_free() */
void s2e_tb_free(struct S2E* s2e, struct TranslationBlock *tb);

/** Called after LLVM code generation
    in order to update tb->s2e_tb->llvm_function */
void s2e_set_tb_function(struct S2E* s2e, struct TranslationBlock *tb);

void s2e_flush_tlb_cache(void);
void s2e_flush_tlb_cache_page(void *objectState, int mmu_idx, int index);

uintptr_t s2e_qemu_tb_exec(struct CPUX86State* env1, struct TranslationBlock* tb);

/* Called by QEMU when execution is aborted using longjmp */
void s2e_qemu_cleanup_tb_exec(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb);

void s2e_qemu_finalize_tb_exec(struct S2E *s2e, struct S2EExecutionState* state);

void s2e_init_timers(struct S2E* s2e);


void s2e_init_device_state(struct S2EExecutionState *s);

#if 0
void s2e_qemu_put_byte(struct S2EExecutionState *s, int v);
int s2e_qemu_get_byte(struct S2EExecutionState *s);
int s2e_qemu_peek_byte(struct S2EExecutionState *s);
int s2e_qemu_get_buffer(struct S2EExecutionState *s, uint8_t *buf, int size1);
int s2e_qemu_peek_buffer(struct S2EExecutionState *s, uint8_t *buf, int size1);
void s2e_qemu_put_buffer(struct S2EExecutionState *s, const uint8_t *buf, int size);
#endif

int s2e_is_zombie(struct S2EExecutionState* state);
int s2e_is_speculative(struct S2EExecutionState *state);
int s2e_is_runnable(struct S2EExecutionState* state);

void s2e_dump_state(void);

void s2e_execute_cmd(const char *cmd);

void s2e_on_device_registration(struct S2E *s2e);
void s2e_on_device_activation(struct S2E *s2e, int bus_type, void *bus);


//Used by port IO for now
void s2e_switch_to_symbolic(struct S2E *s2e, struct S2EExecutionState *state);

void s2e_ensure_symbolic(struct S2E *s2e, struct S2EExecutionState *state);

int s2e_is_port_symbolic(struct S2E *s2e, struct S2EExecutionState* state, uint64_t port);
int s2e_is_mmio_symbolic(uint64_t address, uint64_t size);
int s2e_is_mmio_symbolic_b(uint64_t address);
int s2e_is_mmio_symbolic_w(uint64_t address);
int s2e_is_mmio_symbolic_l(uint64_t address);
int s2e_is_mmio_symbolic_q(uint64_t address);

void s2e_update_tlb_entry(struct S2EExecutionState* state,
                          struct CPUX86State* env,
                          int mmu_idx, uint64_t virtAddr, uint64_t hostAddr);

//Check that no asyc request are pending
int qemu_bh_empty(void);
void qemu_bh_clear(void);

void s2e_register_dirty_mask(struct S2E *s2e, struct S2EExecutionState *initial_state,
                            uint64_t host_address, uint64_t size);
uint8_t s2e_read_dirty_mask(uint64_t host_address);
void s2e_write_dirty_mask(uint64_t host_address, uint8_t val);

void s2e_dma_read(uint64_t hostAddress, uint8_t *buf, unsigned size);
void s2e_dma_write(uint64_t hostAddress, uint8_t *buf, unsigned size);

void s2e_on_privilege_change(unsigned previous, unsigned current);
void s2e_on_page_directory_change(uint64_t previous, uint64_t current);

void s2e_on_initialization_complete(void);

//XXX: Provide a means of including KLEE header
/* Return a possible constant value for the input expression. This
   allows programs to forcibly concretize values on their own. */
unsigned klee_get_value(unsigned expr);


//Used by S2E.h to reinitialize timers in the forked process
int init_timer_alarm(int register_exit_handler);

int s2e_is_load_balancing();
int s2e_is_forking();

/******************************************************/
/* Prototypes for special functions used in LLVM code */
/* NOTE: this functions should never be defined. They */
/* are implemented as a special function handlers.    */

//#if defined(S2E_LLVM_LIB)
uint64_t tcg_llvm_fork_and_concretize(uint64_t value,
                                      uint64_t knownMin,
                                      uint64_t knownMax);
void tcg_llvm_trace_memory_access(uint64_t vaddr, uint64_t haddr,
                                  uint64_t value, uint32_t bits,
                                  uint8_t isWrite, uint8_t isIo);
void tcg_llvm_trace_port_access(uint64_t port, uint64_t value,
                                unsigned bits, int isWrite);
//#endif

#ifdef __cplusplus
}
#endif

#endif // S2E_QEMU_H
