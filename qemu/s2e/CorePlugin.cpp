#include "CorePlugin.h"
#include <s2e/S2E.h>
#include <s2e/Utils.h>

extern "C" {
#include <tcg.h>
#include <tcg-op.h>
}

using namespace std;

S2E_DEFINE_PLUGIN(CorePlugin, "S2E core functionality");

void s2e_tcg_instrument_code(const vector<ExecutionHandler>& handlers, uint64_t pc)
{
    TCGv_ptr t0 = tcg_temp_new_ptr();
    TCGv_i64 t1 = tcg_temp_new_i64();
    TCGv_i64 t2 = tcg_temp_new_i64();
    tcg_gen_movi_i64(t2, pc);

    TCGArg args[3];
    args[0] = GET_TCGV_PTR(t0);
    args[1] = GET_TCGV_PTR(t1);
    args[2] = GET_TCGV_PTR(t2);

    foreach(const ExecutionHandler& handler, handlers) {
#if TCG_TARGET_REG_BITS == 64
        const int sizemask = 8 | 4 | 2;
        tcg_gen_movi_i64(t0, (tcg_target_ulong) handler.plugin);
#else
        const int sizemask = 8 | 4;
        tcg_gen_movi_i32(t0, (tcg_target_ulong) handler.plugin);
#endif
        tcg_gen_movi_i64(t1, handler.arg);

        tcg_gen_helperN((void*) handler.function,
                    0, sizemask, TCG_CALL_DUMMY_ARG, 3, args);
    }
    tcg_temp_free_i64(t2);
    tcg_temp_free_i64(t1);
    tcg_temp_free_ptr(t0);
}

void s2e_on_translate_start(S2E* s2e, uint64_t pc)
{
    vector<ExecutionHandler> handlers;
    s2e->getCorePlugin()->onTranslateStart.emit(&handlers, pc);
    if(!handlers.empty())
        s2e_tcg_instrument_code(handlers, pc);
}

void s2e_on_translate_instruction_start(S2E* s2e, uint64_t pc)
{
    vector<ExecutionHandler> handlers;
    s2e->getCorePlugin()->onTranslateInstructionStart.emit(&handlers, pc);
    if(!handlers.empty())
        s2e_tcg_instrument_code(handlers, pc);
}

void s2e_on_translate_instruction_end(S2E* s2e, uint64_t pc)
{
    vector<ExecutionHandler> handlers;
    s2e->getCorePlugin()->onTranslateInstructionEnd.emit(&handlers, pc);
    if(!handlers.empty())
        s2e_tcg_instrument_code(handlers, pc);
}
