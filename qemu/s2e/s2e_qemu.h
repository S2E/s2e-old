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


// XXX
struct CPUX86State;

#ifdef __cplusplus
extern "C" {
#endif



/* This should never be accessed from C++ code */
extern struct S2E* g_s2e;

/* This should never be accessed from C++ code */
extern struct S2EExecutionState* g_s2e_state;

/**************************/
/* Functions from S2E.cpp */

/** Initialize S2E instance. Called by main() */
struct S2E* s2e_initialize(struct TCGLLVMContext *tcgLLVMContext,
                           const char *s2e_config_file,
                           const char *s2e_output_dir);

/** Relese S2E instance and all S2E-related objects. Called by main() */
void s2e_close(struct S2E* s2e);

/*********************************/
/* Functions from CorePlugin.cpp */

/** Allocate S2E parts of the tanslation block. Called from tb_alloc() */
void s2e_tb_alloc(struct TranslationBlock *tb);

/** Free S2E parts of the translation block. Called from tb_flush() and tb_free() */
void s2e_tb_free(struct TranslationBlock *tb);

/** Called by the translator when a custom instruction is detected */
void s2e_tcg_emit_custom_instruction(struct S2E* s2e,
                                     uint64_t value0, uint64_t value1);

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
        struct TranslationBlock* tb, uint64_t pc);

void s2e_on_exception(
        struct S2E *s2e,
        struct S2EExecutionState* state,
        unsigned intNb);

/**********************************/
/* Functions from S2EExecutor.cpp */

/** Variable that holds the latest return address when
    executiong helper code from KLEE */
extern void* g_s2e_exec_ret_addr;

/** Create initial S2E execution state */
struct S2EExecutionState* s2e_create_initial_state(struct S2E *s2e);

/** Initialize symbolic execution machinery. Should be called after
    QEMU pc is completely constructed */
void s2e_initialize_execution(struct S2E *s2e,
                              struct S2EExecutionState *initial_state);

void s2e_register_cpu(struct S2E* s2e,
                      struct S2EExecutionState *initial_state,
                      struct CPUX86State* cpu_env);

void s2e_register_ram(struct S2E* s2e,
                      struct S2EExecutionState *initial_state,
                      uint64_t start_address, uint64_t size,
                      uint64_t host_address, int is_shared_concrete,
                      int save_on_context_switch);

int s2e_is_ram_registered(struct S2E* s2e,
                          struct S2EExecutionState *state,
                          uint64_t host_address);

int s2e_is_ram_shared_concrete(struct S2E* s2e,
                          struct S2EExecutionState *state,
                          uint64_t host_address);

void s2e_read_ram_concrete(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, uint8_t* buf, uint64_t size);

void s2e_write_ram_concrete(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, const uint8_t* buf, uint64_t size);

/** This function is called when RAM is read by concretely executed
    generated code. If the memory location turns out to be symbolic,
    this function will either concretize it of switch to execution
    in KLEE */
void s2e_read_ram_concrete_check(struct S2E* s2e,
        struct S2EExecutionState* state,
        uint64_t host_address, uint8_t* buf, uint64_t size);

struct S2EExecutionState* s2e_select_next_state(
        struct S2E* s2e, struct S2EExecutionState* state);

uintptr_t s2e_qemu_tb_exec(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb);

/* Called by QEMU when execution is aborted using longjmp */
void s2e_qemu_cleanup_tb_exec(
        struct S2E* s2e,
        struct S2EExecutionState* state,
        struct TranslationBlock* tb);

void s2e_init_timers(void);


/* Called by the load/savevm functions to restore/save the state of the vm */
extern int s2e_dev_snapshot_enable;
void s2e_init_device_state(struct S2EExecutionState *s);
void *s2e_qemu_get_first_se(void);
void *s2e_qemu_get_next_se(void *se);
const char *s2e_qemu_get_se_idstr(void *se);
void s2e_qemu_save_state(void *se);
void s2e_qemu_load_state(void *se);

void s2e_qemu_put_byte(struct S2EExecutionState *s, int v);
int s2e_qemu_get_byte(struct S2EExecutionState *s);
int s2e_qemu_get_buffer(struct S2EExecutionState *s, uint8_t *buf, int size1);
void s2e_qemu_put_buffer(struct S2EExecutionState *s, const uint8_t *buf, int size);



#ifdef __cplusplus
}
#endif

#endif // S2E_QEMU_H
