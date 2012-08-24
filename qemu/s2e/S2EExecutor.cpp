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
 *    Volodymyr Kuznetsov (vova.kuznetsov@epfl.ch)
 *    Vitaly Chipounov (vitaly.chipounov@epfl.ch)
 *
 * Revision List:
 *    v1.0 - Initial Release - Vitaly Chipounov, Vova Kuznetsov
 *
 * All contributors listed in S2E-AUTHORS.
 *
 */

extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg.h>
#include <tcg-llvm.h>
#include <exec-all.h>
#include <ioport.h>
#include <sysemu.h>
#include <cpus.h>

extern struct CPUX86State *env;
void QEMU_NORETURN raise_exception(int exception_index);
void QEMU_NORETURN raise_exception_err(int exception_index, int error_code);
extern const uint8_t parity_table[256];
extern const uint8_t rclw_table[32];
extern const uint8_t rclb_table[32];

void do_interrupt_all(int intno, int is_int, int error_code,
                             target_ulong next_eip, int is_hw);


void s2e_do_interrupt_all(int intno, int is_int, int error_code,
                             target_ulong next_eip, int is_hw);
uint64_t helper_set_cc_op_eflags(void);
}

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "S2EExecutor.h"
#include <s2e/s2e_config.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>

#include <s2e/S2EDeviceState.h>
#include <s2e/SelectRemovalPass.h>
#include <s2e/S2EStatsTracker.h>

//XXX: Remove this from executor
#include <s2e/Plugins/ModuleExecutionDetector.h>

#include <s2e/s2e_qemu.h>

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Constants.h>
#include <llvm/PassManager.h>
#include <llvm/Target/TargetData.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/CommandLine.h>

#include <klee/PTree.h>
#include <klee/Memory.h>
#include <klee/Searcher.h>
#include <klee/ExternalDispatcher.h>
#include <klee/UserSearcher.h>
#include <klee/CoreStats.h>
#include <klee/TimerStatIncrementer.h>
#include <klee/Solver.h>

#include <llvm/Support/TimeValue.h>

#include <vector>

#include <sstream>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <tr1/functional>

//#define S2E_DEBUG_MEMORY
//#define S2E_DEBUG_INSTRUCTIONS
//#define S2E_TRACE_EFLAGS
//#define FORCE_CONCRETIZATION

using namespace std;
using namespace llvm;
using namespace klee;

extern "C" {
    // XXX
    //void* g_s2e_exec_ret_addr = 0;
}

namespace {
    uint64_t hash64(uint64_t val, uint64_t initial = 14695981039346656037ULL) {
        const char* __first = (const char*) &val;
        for (unsigned int i = 0; i < sizeof(uint64_t); ++i) {
            initial ^= static_cast<uint64_t>(*__first++);
            initial *= static_cast<uint64_t>(1099511628211ULL);
        }
        return initial;
    }
}

namespace {
    cl::opt<bool>
    UseSelectCleaner("use-select-cleaner",
            cl::desc("Remove Select statements from LLVM code"),
            cl::init(false));

    cl::opt<bool>
    StateSharedMemory("state-shared-memory",
            cl::desc("Allow unimportant memory regions (like video RAM) to be shared between states"),
            cl::init(false));


    cl::opt<unsigned>
    MaxForksOnConcretize("max-forks-on-concretize",
            cl::desc("Maximum number of states to fork when concretizing symbolic value"),
            cl::init(256));

    cl::opt<bool>
    FlushTBsOnStateSwitch("flush-tbs-on-state-switch",
            cl::desc("Flush translation blocks when switching states -"
                     " disabling leads to faster but possibly incorrect execution"),
            cl::init(true));

    cl::opt<bool>
    KeepLLVMFunctions("keep-llvm-functions",
            cl::desc("Never delete generated LLVM functions"),
            cl::init(false));

    cl::opt<bool>
    ForkOnSymbolicAddress("fork-on-symbolic-address",
            cl::desc("Fork on each memory access with symbolic address"),
            cl::init(false));

    cl::opt<bool>
    ConcretizeIoAddress("concretize-io-address",
            cl::desc("Concretize symbolic I/O addresses"),
            cl::init(true));

    //XXX: Works for MMIO only, add support for port I/O
    cl::opt<bool>
    ConcretizeIoWrites("concretize-io-writes",
            cl::desc("Concretize symbolic I/O writes"),
            cl::init(true));

    cl::opt<bool>
    S2EDebugInstructions("print-llvm-instructions",
                   cl::desc("Traces all LLVM instructions sent to KLEE"),  cl::init(false));

    cl::opt<bool>
    VerboseFork("verbose-fork-info",
                   cl::desc("Print detailed information on forks"),  cl::init(false));

    cl::opt<bool>
    VerboseStateSwitching("verbose-state-switching",
                   cl::desc("Print detailed information on state switches"),  cl::init(false));

    cl::opt<bool>
    VerboseTbFinalize("verbose-tb-finalize",
                   cl::desc("Print detailed information when finalizing a partially-completed TB"),  cl::init(false));

    cl::opt<bool>
    UseFastHelpers("use-fast-helpers",
                   cl::desc("Replaces LLVM bitcode with fast symbolic-aware equivalent native helpers"),  cl::init(false));

}

//The logs may be flooded with messages when switching execution mode.
//This option allows disabling printing mode switches.
cl::opt<bool>
PrintModeSwitch("print-mode-switch",
                cl::desc("Print message when switching from symbolic to concrete and vice versa"),
                cl::init(false));

cl::opt<bool>
PrintForkingStatus("print-forking-status",
                cl::desc("Print message when enabling/disabling forking."),
                cl::init(false));

cl::opt<bool>
VerboseStateDeletion("verbose-state-deletion",
               cl::desc("Print detailed information on state deletion"),  cl::init(false));

cl::opt<bool>
ConcolicMode("use-concolic-execution",
               cl::desc("Concolic execution mode"),  cl::init(false));



extern cl::opt<bool> UseExprSimplifier;

extern "C" {
    int g_s2e_fork_on_symbolic_address = 0;
    int g_s2e_concretize_io_addresses = 1;
    int g_s2e_concretize_io_writes = 1;
}


namespace s2e {

/* Global array to hold tb function arguments */
volatile void* tb_function_args[3];

/* External dispatcher to convert QEMU s2e_longjmp's into C++ exceptions */
class S2EExternalDispatcher: public klee::ExternalDispatcher
{
protected:
    virtual bool runProtectedCall(llvm::Function *f, uint64_t *args);

public:
    S2EExternalDispatcher(ExecutionEngine* engine):
            ExternalDispatcher(engine) {}

    void removeFunction(llvm::Function *f);
};

extern "C" {

// FIXME: This is not reentrant.
static s2e_jmp_buf s2e_escapeCallJmpBuf;
static s2e_jmp_buf s2e_cpuExitJmpBuf;

#ifdef _WIN32
static void s2e_ext_sigsegv_handler(int signal)
{
}
#else
static void s2e_ext_sigsegv_handler(int signal, siginfo_t *info, void *context) {
  s2e_longjmp(s2e_escapeCallJmpBuf, 1);
}
#endif

}

bool S2EExternalDispatcher::runProtectedCall(Function *f, uint64_t *args) {

  #ifndef _WIN32
  struct sigaction segvAction, segvActionOld;
  #endif
  bool res;

  if (!f)
    return false;

  gTheArgsP = args;

  #ifdef _WIN32
  signal(SIGSEGV, s2e_ext_sigsegv_handler);
  #else
  segvAction.sa_handler = 0;
  memset(&segvAction.sa_mask, 0, sizeof(segvAction.sa_mask));
  segvAction.sa_flags = SA_SIGINFO;
  segvAction.sa_sigaction = s2e_ext_sigsegv_handler;
  sigaction(SIGSEGV, &segvAction, &segvActionOld);
  #endif

  memcpy(s2e_cpuExitJmpBuf, env->jmp_env, sizeof(env->jmp_env));

  if(s2e_setjmp(env->jmp_env)) {
      memcpy(env->jmp_env, s2e_cpuExitJmpBuf, sizeof(env->jmp_env));
      throw CpuExitException();
  } else {
      if (s2e_setjmp(s2e_escapeCallJmpBuf)) {
        res = false;
      } else {
        std::vector<GenericValue> gvArgs;

        executionEngine->runFunction(f, gvArgs);
        res = true;
      }
  }

  memcpy(env->jmp_env, s2e_cpuExitJmpBuf, sizeof(env->jmp_env));

  #ifdef _WIN32
#warning Implement more robust signal handling on windows
  signal(SIGSEGV, SIG_IGN);
#else
  sigaction(SIGSEGV, &segvActionOld, 0);
#endif
  return res;
}

/**
 * Remove all mappings between calls to external functions
 * and the actual external call stub generated by KLEE.
 * Also remove the machine code for the stub and the stub itself.
 * This is used whenever S2E deletes a translation block and its LLVM
 * representation. Failing to do so would leave stale references to
 * machine code in KLEE's external dispatcher.
 */
void S2EExternalDispatcher::removeFunction(llvm::Function *f) {
        dispatchers_ty::iterator it, itn;

        it = dispatchers.begin();
        while (it != dispatchers.end()) {
            if ((*it).first->getParent()->getParent() == f) {

                llvm::Function *dispatcher = (*it).second;
                executionEngine->freeMachineCodeForFunction(dispatcher);
                dispatcher->eraseFromParent();

                itn = it;
                ++itn;
                dispatchers.erase(it);
                it = itn;
            } else {
                ++it;
            }
        }
    }


S2EHandler::S2EHandler(S2E* s2e)
        : m_s2e(s2e)
{
}

llvm::raw_ostream &S2EHandler::getInfoStream() const
{
    return m_s2e->getInfoStream();
}

std::string S2EHandler::getOutputFilename(const std::string &fileName)
{
    return m_s2e->getOutputFilename(fileName);
}

llvm::raw_ostream *S2EHandler::openOutputFile(const std::string &fileName)
{
    return m_s2e->openOutputFile(fileName);
}

/* klee-related function */
void S2EHandler::incPathsExplored()
{
    m_pathsExplored++;
}

/* klee-related function */
void S2EHandler::processTestCase(const klee::ExecutionState &state,
                     const char *err, const char *suffix)
{
    //XXX: This stuff is not used anymore
    //Use onTestCaseGeneration event instead.
}

void S2EExecutor::handlerTraceMemoryAccess(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    assert(dynamic_cast<S2EExecutor*>(executor));

    S2EExecutor* s2eExecutor = static_cast<S2EExecutor*>(executor);
    if(!s2eExecutor->m_s2e->getCorePlugin()->onDataMemoryAccess.empty()) {
        assert(dynamic_cast<S2EExecutionState*>(state));
        S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);

        assert(args.size() == 6);

        Expr::Width width = cast<klee::ConstantExpr>(args[3])->getZExtValue();
        bool isWrite = cast<klee::ConstantExpr>(args[4])->getZExtValue();
        bool isIO    = cast<klee::ConstantExpr>(args[5])->getZExtValue();

        ref<Expr> value = klee::ExtractExpr::create(args[2], 0, width);

        s2eExecutor->m_s2e->getCorePlugin()->onDataMemoryAccess.emit(
                s2eState, args[0], args[1], value, isWrite, isIO);
    }
}

void S2EExecutor::handlerTraceInstruction(klee::Executor* executor,
                                klee::ExecutionState* state,
                                klee::KInstruction* target,
                                std::vector<klee::ref<klee::Expr> > &args)
{
    S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);
    g_s2e->getDebugStream()
            << "pc=" << hexval(s2eState->getPc())
            << " EAX: " << s2eState->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32)
            << " ECX: " << s2eState->readCpuRegister(offsetof(CPUX86State, regs[R_ECX]), klee::Expr::Int32)
            << " CCSRC: " << s2eState->readCpuRegister(offsetof(CPUX86State, cc_src), klee::Expr::Int32)
            << " CCDST: " << s2eState->readCpuRegister(offsetof(CPUX86State, cc_dst), klee::Expr::Int32)
            << " CCOP: " << s2eState->readCpuRegister(offsetof(CPUX86State, cc_op), klee::Expr::Int32) << '\n';
}

void S2EExecutor::handlerOnTlbMiss(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    assert(dynamic_cast<S2EExecutor*>(executor));

    assert(args.size() == 4);

    S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);
    ref<Expr> addr = args[2];
    bool isWrite = cast<klee::ConstantExpr>(args[3])->getZExtValue();

    if(!isa<klee::ConstantExpr>(addr)) {
        /*
        g_s2e->getWarningsStream()
                << "Warning: s2e_on_tlb_miss does not support symbolic addresses"
                << '\n';
                */
        return;
    }

    uint64_t constAddress;
    constAddress = cast<klee::ConstantExpr>(addr)->getZExtValue(64);

    s2e_on_tlb_miss(g_s2e, s2eState, constAddress, isWrite);
}

void S2EExecutor::handlerTracePortAccess(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    assert(dynamic_cast<S2EExecutor*>(executor));

    S2EExecutor* s2eExecutor = static_cast<S2EExecutor*>(executor);

    if(!s2eExecutor->m_s2e->getCorePlugin()->onPortAccess.empty()) {
        assert(dynamic_cast<S2EExecutionState*>(state));
        S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);

        assert(args.size() == 4);

        Expr::Width width = cast<klee::ConstantExpr>(args[2])->getZExtValue();
        bool isWrite = cast<klee::ConstantExpr>(args[3])->getZExtValue();

        ref<Expr> value = klee::ExtractExpr::create(args[1], 0, width);

        s2eExecutor->m_s2e->getCorePlugin()->onPortAccess.emit(
                s2eState, args[0], value, isWrite);
    }
}

void S2EExecutor::handleForkAndConcretize(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args)
{
    S2EExecutor* s2eExecutor = static_cast<S2EExecutor*>(executor);
    S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);

    assert(args.size() == 3);
    assert(isa<klee::ConstantExpr>(args[1]));
    assert(isa<klee::ConstantExpr>(args[2]));

    uint64_t min = cast<klee::ConstantExpr>(args[1])->getZExtValue();
    uint64_t max = cast<klee::ConstantExpr>(args[2])->getZExtValue();
    assert(min <= max);

    ref<Expr> expr = args[0];
    Expr::Width width = expr->getWidth();

    // XXX: this might be expensive...
    if (UseExprSimplifier) {
        expr = s2eExecutor->simplifyExpr(*state, expr);
    }

    expr = state->concolics.evaluate(expr);
    expr = state->constraints.simplifyExpr(expr);


    if(isa<klee::ConstantExpr>(expr)) {
#ifndef NDEBUG
        uint64_t value = cast<klee::ConstantExpr>(expr)->getZExtValue();
        assert(value >= min && value <= max);
#endif
        s2eExecutor->bindLocal(target, *state, expr);
        return;
    }

    g_s2e->getDebugStream(s2eState) << "forkAndConcretize(" << expr << ")" << '\n';

    if (state->forkDisabled) {
        //Simply pick one possible value and continue
        ref<klee::ConstantExpr> value;
        bool success = s2eExecutor->getSolver()->getValue(
                Query(state->constraints, expr), value);

        if (success) {
            g_s2e->getDebugStream(s2eState) << "Chosen " << value << '\n';
            ref<Expr> eqCond = EqExpr::create(expr, value);
            state->addConstraint(eqCond);
            s2eExecutor->bindLocal(target, *state, value);
        }else {
            g_s2e->getDebugStream(s2eState) << "Failed to find a value. Leaving unconstrained." << '\n';
            s2eExecutor->bindLocal(target, *state, expr);
        }
        return;
    }

    // go starting from min
    Query query(state->constraints, expr);
    uint64_t step = 1;
    std::vector< uint64_t > values;
    std::vector< ref<Expr> > conditions;
    while(min <= max) {
        if(conditions.size() >= MaxForksOnConcretize) {
            s2eExecutor->m_s2e->getWarningsStream(s2eState)
                << "Dropping states with constraint \n"
                << UleExpr::create(expr, klee::ConstantExpr::create(min, width))
                << "\n becase max-forks-on-concretize limit was reached." << '\n';
            break;
        }

        ref<Expr> eqCond = EqExpr::create(expr, klee::ConstantExpr::create(min, width));
        bool res = false;

        bool success = s2eExecutor->getSolver()->mayBeTrue(query.withExpr(eqCond), res);
        assert(success && "FIXME: Unhandled solver failure");

        if(res) {
            values.push_back(min);
            conditions.push_back(eqCond);
        }

        if(min == max) {
            break; // this was the last possible value
        }

        // Choice next value to try
        uint64_t lo = min+1, hi = max, mid = min + step;

        if(res) {
            // we succeeded with guessing step, it is worth trying it again
            if(step == 1) {
                min += 1; continue;
            } else {
                bool success =
                    s2eExecutor->getSolver()->mayBeTrue(
                        query.withExpr(
                            AndExpr::create(
                                UgeExpr::create(expr,
                                    klee::ConstantExpr::create(min+1, width)),
                                UleExpr::create(expr,
                                    klee::ConstantExpr::create(min+step-1, width)))),
                        res);
                assert(success && "FIXME: Unhandled solver failure");
                if(res == false) {
                    min += step; continue;
                }

                // there are some values in range [min+1, min+step-1],
                // fall back to binary search to find them
                hi = min + step - 1;
                mid = lo + (hi - lo)/2;
            }
        }

        // Previous step didn't worked, try binary search
        // anyway, start from the same step as initial guess
        while(lo < hi) {
            bool res = false;
            bool success =
                s2eExecutor->getSolver()->mayBeTrue(
                    query.withExpr(
                        AndExpr::create(
                            UgeExpr::create(expr,
                                klee::ConstantExpr::create(lo, width)),
                            UleExpr::create(expr,
                                klee::ConstantExpr::create(mid, width)))),
                    res);

            assert(success && "FIXME: Unhandled solver failure");
            (void) success;

            if (res)
                hi = mid;
            else
                lo = mid+1;

            mid = lo + (hi-lo)/2;
        }

        step = lo - min;
        min = lo;

        if(min + step > max)
            step = max - min;
    }

    std::vector<ExecutionState *> branches;
    s2eExecutor->branch(*state, conditions, branches);

    for(uint64_t i=0; i<conditions.size(); i++) {
#ifndef NDEBUG
        ref<klee::ConstantExpr> value;
        bool success = s2eExecutor->getSolver()->getValue(
                Query(branches[i]->constraints, expr), value);

        assert(success && "The solver failed");
        if (value->getZExtValue() != values[i]) {
            g_s2e->getWarningsStream() << "Solver error: " <<
                    hexval(value->getZExtValue()) << " != " << hexval(values[i]) << '\n';

        }
        assert(success && value->getZExtValue() == values[i]);
#endif
        s2eExecutor->bindLocal(target, *branches[i],
                               klee::ConstantExpr::create(values[i], width));
    }
}

S2EExecutor::S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLLVMContext,
                    const InterpreterOptions &opts,
                            InterpreterHandler *ie)
        : Executor(opts, ie, tcgLLVMContext->getExecutionEngine()),
          m_s2e(s2e), m_tcgLLVMContext(tcgLLVMContext),
          m_executeAlwaysKlee(false), m_forkProcTerminateCurrentState(false),
          m_inLoadBalancing(false)
{
    delete externalDispatcher;
    externalDispatcher = new S2EExternalDispatcher(
            tcgLLVMContext->getExecutionEngine());

    LLVMContext& ctx = m_tcgLLVMContext->getLLVMContext();

    // XXX: this will not work without creating JIT
    // XXX: how to get data layout without without ExecutionEngine ?
    m_tcgLLVMContext->getModule()->setDataLayout(
            m_tcgLLVMContext->getExecutionEngine()
                ->getTargetData()->getStringRepresentation());

    /* Define globally accessible functions */
#define __DEFINE_EXT_FUNCTION(name) \
    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) name);

#define __DEFINE_EXT_VARIABLE(name) \
    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) &name);

    //__DEFINE_EXT_FUNCTION(raise_exception)
    //__DEFINE_EXT_FUNCTION(raise_exception_err)

    __DEFINE_EXT_VARIABLE(g_s2e_concretize_io_addresses)
    __DEFINE_EXT_VARIABLE(g_s2e_concretize_io_writes)
    __DEFINE_EXT_VARIABLE(g_s2e_fork_on_symbolic_address)

    __DEFINE_EXT_FUNCTION(fprintf)
    __DEFINE_EXT_FUNCTION(sprintf)
    __DEFINE_EXT_FUNCTION(fputc)
    __DEFINE_EXT_FUNCTION(fwrite)

    __DEFINE_EXT_VARIABLE(io_mem_ram)
    __DEFINE_EXT_VARIABLE(io_mem_rom)
    __DEFINE_EXT_VARIABLE(io_mem_unassigned)
    __DEFINE_EXT_VARIABLE(io_mem_notdirty)

    __DEFINE_EXT_FUNCTION(cpu_io_recompile)
    __DEFINE_EXT_FUNCTION(cpu_x86_handle_mmu_fault)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr0)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr3)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr4)
    __DEFINE_EXT_FUNCTION(cpu_x86_cpuid)
    __DEFINE_EXT_FUNCTION(cpu_get_apic_base)
    __DEFINE_EXT_FUNCTION(cpu_set_apic_base)
    __DEFINE_EXT_FUNCTION(cpu_get_apic_tpr)
    __DEFINE_EXT_FUNCTION(cpu_set_apic_tpr)
    __DEFINE_EXT_FUNCTION(cpu_smm_update)
    __DEFINE_EXT_FUNCTION(cpu_outb)
    __DEFINE_EXT_FUNCTION(cpu_outw)
    __DEFINE_EXT_FUNCTION(cpu_outl)
    __DEFINE_EXT_FUNCTION(cpu_inb)
    __DEFINE_EXT_FUNCTION(cpu_inw)
    __DEFINE_EXT_FUNCTION(cpu_inl)
    __DEFINE_EXT_FUNCTION(cpu_restore_state)
    __DEFINE_EXT_FUNCTION(cpu_abort)
    __DEFINE_EXT_FUNCTION(cpu_loop_exit)
    __DEFINE_EXT_FUNCTION(cpu_get_tsc)
    __DEFINE_EXT_FUNCTION(tb_find_pc)

    __DEFINE_EXT_FUNCTION(qemu_system_reset_request)

    __DEFINE_EXT_FUNCTION(hw_breakpoint_insert)
    __DEFINE_EXT_FUNCTION(hw_breakpoint_remove)
    __DEFINE_EXT_FUNCTION(check_hw_breakpoints)

    __DEFINE_EXT_FUNCTION(tlb_flush_page)
    __DEFINE_EXT_FUNCTION(tlb_flush)

    __DEFINE_EXT_FUNCTION(io_readb_mmu)
    __DEFINE_EXT_FUNCTION(io_readw_mmu)
    __DEFINE_EXT_FUNCTION(io_readl_mmu)
    __DEFINE_EXT_FUNCTION(io_readq_mmu)

    __DEFINE_EXT_FUNCTION(io_writeb_mmu)
    __DEFINE_EXT_FUNCTION(io_writew_mmu)
    __DEFINE_EXT_FUNCTION(io_writel_mmu)
    __DEFINE_EXT_FUNCTION(io_writeq_mmu)

    __DEFINE_EXT_FUNCTION(iotlb_to_region)


    __DEFINE_EXT_FUNCTION(s2e_ensure_symbolic)

    //__DEFINE_EXT_FUNCTION(s2e_on_tlb_miss)
    __DEFINE_EXT_FUNCTION(s2e_on_page_fault)
    __DEFINE_EXT_FUNCTION(s2e_is_port_symbolic)
    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_b)
    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_w)
    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_l)
    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_q)

    __DEFINE_EXT_FUNCTION(s2e_on_privilege_change);
    __DEFINE_EXT_FUNCTION(s2e_on_page_fault);


    __DEFINE_EXT_FUNCTION(s2e_ismemfunc)
    __DEFINE_EXT_FUNCTION(s2e_notdirty_mem_write)

    __DEFINE_EXT_FUNCTION(cpu_io_recompile)
    __DEFINE_EXT_FUNCTION(can_do_io)

    __DEFINE_EXT_FUNCTION(ldub_phys)
    __DEFINE_EXT_FUNCTION(stb_phys)

    __DEFINE_EXT_FUNCTION(lduw_phys)
    __DEFINE_EXT_FUNCTION(stw_phys)

    __DEFINE_EXT_FUNCTION(ldl_phys)
    __DEFINE_EXT_FUNCTION(stl_phys)

    __DEFINE_EXT_FUNCTION(ldq_phys)
    __DEFINE_EXT_FUNCTION(stq_phys)

#if 0
    //Implementing these functions prevents special function handler
    //from being called...
    if (execute_llvm) {
        __DEFINE_EXT_FUNCTION(tcg_llvm_fork_and_concretize)
        __DEFINE_EXT_FUNCTION(tcg_llvm_trace_memory_access)
        __DEFINE_EXT_FUNCTION(tcg_llvm_trace_port_access)
    }
#endif

    if(UseSelectCleaner) {
        m_tcgLLVMContext->getFunctionPassManager()->add(new SelectRemovalPass());
        m_tcgLLVMContext->getFunctionPassManager()->doInitialization();
    }

    ModuleOptions MOpts = ModuleOptions(vector<string>(),
                                        /* Optimize= */ true, /* CheckDivZero= */ false);
    /* Set module for the executor */


    /**
     * When execute_llvm is true, all code is translated to LLVM, then JITed to x86.
     * This is basically a debug mode that allows reusing all S2E's plugin infrastructure
     * while testing the LLVM backend.
     * Symolic execution IS NOT SUPPORTED when running in LLVM mode!
     */
    if (!execute_llvm) {
        char* filename =  qemu_find_file(QEMU_FILE_TYPE_LIB, "op_helper.bc");
        assert(filename);
        MOpts = ModuleOptions(vector<string>(1, filename),
                /* Optimize= */ true, /* CheckDivZero= */ false,
                m_tcgLLVMContext->getFunctionPassManager());

        g_free(filename);
    }

    /* This catches obvious LLVM misconfigurations */
    Module *M = m_tcgLLVMContext->getModule();
    TargetData TD(M);
    assert(M->getPointerSize() == Module::Pointer64 && "Something is broken in your LLVM build: LLVM thinks pointers are 32-bits!");

    s2e->getDebugStream() << "Current data layout: " << m_tcgLLVMContext->getModule()->getDataLayout() << '\n';
    s2e->getDebugStream() << "Current target triple: " << m_tcgLLVMContext->getModule()->getTargetTriple() << '\n';


    setModule(m_tcgLLVMContext->getModule(), MOpts, false);

    if (UseFastHelpers) {
        disableConcreteLLVMHelpers();
    }

    /* Add dummy TB function declaration */
    PointerType* tbFunctionArgTy =
            PointerType::get(IntegerType::get(ctx, 64), 0);
    FunctionType* tbFunctionTy = FunctionType::get(
            IntegerType::get(ctx, TCG_TARGET_REG_BITS),
            ArrayRef<Type*>(vector<Type*>(1, PointerType::get(
                    IntegerType::get(ctx, 64), 0))),
            false);

    Function* tbFunction = Function::Create(
            tbFunctionTy, Function::PrivateLinkage, "s2e_dummyTbFunction",
            m_tcgLLVMContext->getModule());

    /* Create dummy main function containing just two instructions:
       a call to TB function and ret */
    Function* dummyMain = Function::Create(
            FunctionType::get(Type::getVoidTy(ctx), false),
            Function::PrivateLinkage, "s2e_dummyMainFunction",
            m_tcgLLVMContext->getModule());

    BasicBlock* dummyMainBB = BasicBlock::Create(ctx, "entry", dummyMain);

    vector<Value*> tbFunctionArgs(1, ConstantPointerNull::get(tbFunctionArgTy));
    CallInst::Create(tbFunction, ArrayRef<Value*>(tbFunctionArgs),
            "tbFunctionCall", dummyMainBB);
    ReturnInst::Create(m_tcgLLVMContext->getLLVMContext(), dummyMainBB);

    kmodule->updateModuleWithFunction(dummyMain);
    m_dummyMain = kmodule->functionMap[dummyMain];

    if (!execute_llvm) {
        Function* function;

        function = kmodule->module->getFunction("tcg_llvm_trace_memory_access");
        assert(function);
        addSpecialFunctionHandler(function, handlerTraceMemoryAccess);

        function = kmodule->module->getFunction("tcg_llvm_trace_port_access");
        assert(function);
        addSpecialFunctionHandler(function, handlerTracePortAccess);

        function = kmodule->module->getFunction("s2e_on_tlb_miss");
        assert(function);
        addSpecialFunctionHandler(function, handlerOnTlbMiss);

        function = kmodule->module->getFunction("tcg_llvm_fork_and_concretize");
        assert(function);
        addSpecialFunctionHandler(function, handleForkAndConcretize);

        FunctionType *traceInstTy = FunctionType::get(Type::getVoidTy(M->getContext()), false);
        function = dynamic_cast<Function*>(kmodule->module->getOrInsertFunction("tcg_llvm_trace_instruction", traceInstTy));
        assert(function);
        addSpecialFunctionHandler(function, handlerTraceInstruction);

        if (UseFastHelpers) {
            replaceExternalFunctionsWithSpecialHandlers();
        }

        m_tcgLLVMContext->initializeHelpers();
    }

    initializeStatistics();


    searcher = constructUserSearcher(*this);

    m_forceConcretizations = false;

    g_s2e_fork_on_symbolic_address = ForkOnSymbolicAddress;
    g_s2e_concretize_io_addresses = ConcretizeIoAddress;
    g_s2e_concretize_io_writes = ConcretizeIoWrites;
}

void S2EExecutor::initializeStatistics()
{
    if(StatsTracker::useStatistics()) {
        statsTracker =
                new S2EStatsTracker(*this,
                    interpreterHandler->getOutputFilename("assembly.ll"),
                    userSearcherRequiresMD2U());
        statsTracker->writeHeaders();
    }
}


void S2EExecutor::flushTb() {
    tb_flush(env); // release references to TB functions
}

S2EExecutor::~S2EExecutor()
{
    if(statsTracker)
        statsTracker->done();
}

S2EExecutionState* S2EExecutor::createInitialState()
{
    assert(!processTree);

    /* Create initial execution state */
    S2EExecutionState *state =
        new S2EExecutionState(m_dummyMain);

    state->m_runningConcrete = true;
    state->m_active = true;

    if(pathWriter)
        state->pathOS = pathWriter->open();
    if(symPathWriter)
        state->symPathOS = symPathWriter->open();

    if(statsTracker)
        statsTracker->framePushed(*state, 0);

    states.insert(state);
    searcher->update(0, states, std::set<ExecutionState*>());

    processTree = new PTree(state);
    state->ptreeNode = processTree->root;

    /* Externally accessible global vars */
    /* XXX move away */
    addExternalObject(*state, &tcg_llvm_runtime,
                      sizeof(tcg_llvm_runtime), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true,
                      /* isValueIgnored = */ true);

    addExternalObject(*state, (void*) tb_function_args,
                      sizeof(tb_function_args), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true,
                      /* isValueIgnored = */ true);

#define __DEFINE_EXT_OBJECT_RO(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      true, true, true)->setName(#name);

#define __DEFINE_EXT_OBJECT_RO_SYMB(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      true, true, false)->setName(#name);

    __DEFINE_EXT_OBJECT_RO(env)
    __DEFINE_EXT_OBJECT_RO(g_s2e)
    __DEFINE_EXT_OBJECT_RO(g_s2e_state)
    //__DEFINE_EXT_OBJECT_RO(g_s2e_exec_ret_addr)
    __DEFINE_EXT_OBJECT_RO(io_mem_read)
    __DEFINE_EXT_OBJECT_RO(io_mem_write)
    //__DEFINE_EXT_OBJECT_RO(io_mem_opaque)
    __DEFINE_EXT_OBJECT_RO(use_icount)
    __DEFINE_EXT_OBJECT_RO(cpu_single_env)
    __DEFINE_EXT_OBJECT_RO(loglevel)
    __DEFINE_EXT_OBJECT_RO(logfile)
    __DEFINE_EXT_OBJECT_RO_SYMB(parity_table)
    __DEFINE_EXT_OBJECT_RO_SYMB(rclw_table)
    __DEFINE_EXT_OBJECT_RO_SYMB(rclb_table)


    m_s2e->getMessagesStream(state)
            << "Created initial state" << '\n';

    return state;
}

void S2EExecutor::initializeExecution(S2EExecutionState* state,
                                      bool executeAlwaysKlee)
{
#if 0
    typedef std::pair<uint64_t, uint64_t> _UnusedMemoryRegion;
    foreach(_UnusedMemoryRegion p, m_unusedMemoryRegions) {
        /* XXX */
        /* XXX : use qemu_virtual* */
#ifdef WIN32
        VirtualFree((void*) p.first, p.second, MEM_FREE);
#else
        munmap((void*) p.first, p.second);
#endif
    }
#endif

    m_executeAlwaysKlee = executeAlwaysKlee;

    initializeGlobals(*state);
    bindModuleConstants();

    initTimers();
    initializeStateSwitchTimer();
}

void S2EExecutor::registerCpu(S2EExecutionState *initialState,
                              CPUX86State *cpuEnv)
{
    std::cout << std::hex
            << "Adding CPU (addr = " << std::hex << cpuEnv
              << ", size = 0x" << sizeof(*cpuEnv) << ")"
              << std::dec << '\n';

    /* Add registers and eflags area as a true symbolic area */
    initialState->m_cpuRegistersState =
        addExternalObject(*initialState, cpuEnv,
                      offsetof(CPUX86State, eip),
                      /* isReadOnly = */ false,
                      /* isUserSpecified = */ false,
                      /* isSharedConcrete = */ false);

    initialState->m_cpuRegistersState->setName("CpuRegistersState");

    /* Add the rest of the structure as concrete-only area */
    initialState->m_cpuSystemState =
        addExternalObject(*initialState,
                      ((uint8_t*)cpuEnv) + offsetof(CPUX86State, eip),
                      sizeof(CPUX86State) - offsetof(CPUX86State, eip),
                      /* isReadOnly = */ false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);

    initialState->m_cpuSystemState->setName("CpuSystemState");

    m_saveOnContextSwitch.push_back(initialState->m_cpuSystemState);

    const ObjectState *cpuSystemObject = initialState->addressSpace
                                .findObject(initialState->m_cpuSystemState);
    const ObjectState *cpuRegistersObject = initialState->addressSpace
                                .findObject(initialState->m_cpuRegistersState);

    initialState->m_cpuRegistersObject = initialState->addressSpace
        .getWriteable(initialState->m_cpuRegistersState, cpuRegistersObject);
    initialState->m_cpuSystemObject = initialState->addressSpace
        .getWriteable(initialState->m_cpuSystemState, cpuSystemObject);
}

void S2EExecutor::registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete,
                        bool saveOnContextSwitch, const char *name)
{
    assert(isSharedConcrete || !saveOnContextSwitch);
    assert(startAddress == (uint64_t) -1 ||
           (startAddress & ~TARGET_PAGE_MASK) == 0);
    assert((size & ~TARGET_PAGE_MASK) == 0);
    assert((hostAddress & ~TARGET_PAGE_MASK) == 0);

    m_s2e->getDebugStream()
              << "Adding memory block (startAddr = " << hexval(startAddress)
              << ", size = " << hexval(size) << ", hostAddr = " << hexval(hostAddress)
              << ", isSharedConcrete=" << isSharedConcrete << ", name=" << name << ")\n";

    for(uint64_t addr = hostAddress; addr < hostAddress+size;
                 addr += S2E_RAM_OBJECT_SIZE) {
        std::stringstream ss;

        ss << name << "_" << std::hex << (addr-hostAddress);

        MemoryObject *mo = addExternalObject(
                *initialState, (void*) addr, S2E_RAM_OBJECT_SIZE, false,
                /* isUserSpecified = */ true, isSharedConcrete,
                isSharedConcrete && !saveOnContextSwitch && StateSharedMemory);

        mo->setName(ss.str());

        if (isSharedConcrete && (saveOnContextSwitch || !StateSharedMemory)) {
            m_saveOnContextSwitch.push_back(mo);
        }
    }

    if(!isSharedConcrete) {
        /* XXX */
        /* XXX : use qemu_mprotect */
#ifdef WIN32
        DWORD OldProtect;
        if (!VirtualProtect((void*) hostAddress, size, PAGE_NOACCESS, &OldProtect)) {
            assert(false);
        }
#else
        mprotect((void*) hostAddress, size, PROT_NONE);
#endif
        m_unusedMemoryRegions.push_back(make_pair(hostAddress, size));
    }

    initialState->m_memcache.registerPool(hostAddress, size);

}

void S2EExecutor::registerDirtyMask(S2EExecutionState *initial_state, uint64_t host_address, uint64_t size)
{
    //Assume that dirty mask is small enough, so no need to split it in small pages
    assert(!initial_state->m_dirtyMask);
    initial_state->m_dirtyMask = g_s2e->getExecutor()->addExternalObject(
            *initial_state, (void*) host_address, size, false,
            /* isUserSpecified = */ true, true, false);

    initial_state->m_dirtyMask->setName("dirtyMask");

    m_saveOnContextSwitch.push_back(initial_state->m_dirtyMask);

    const ObjectState *dirtyMaskObject = initial_state->addressSpace
                                .findObject(initial_state->m_dirtyMask);

    initial_state->m_dirtyMaskObject = initial_state->addressSpace
        .getWriteable(initial_state->m_dirtyMask, dirtyMaskObject);
}


void S2EExecutor::switchToConcrete(S2EExecutionState *state)
{
    assert(!state->m_runningConcrete);

    /* Concretize any symbolic registers */
    ObjectState* wos = state->m_cpuRegistersObject;
    assert(wos);

    if (m_forceConcretizations) {
        //XXX: Find a adhoc dirty way to implement overconstrained consistency model
        //There should be a consistency plugin somewhere else
        s2e::plugins::ModuleExecutionDetector *md =
                dynamic_cast<s2e::plugins::ModuleExecutionDetector*>(m_s2e->getPlugin("ModuleExecutionDetector"));
        if (md && !md->getCurrentDescriptor(state)) {

            if(!wos->isAllConcrete()) {
                /* The object contains symbolic values. We have to
               concretize it */

                for(unsigned i = 0; i < wos->size; ++i) {
                    ref<Expr> e = wos->read8(i);
                    if(!isa<klee::ConstantExpr>(e)) {
                        uint8_t ch = toConstant(*state, e,
                                                "switching to concrete execution")->getZExtValue(8);
                        wos->write8(i, ch);
                    }
                }
            }
        }
    }

    //assert(os->isAllConcrete());
    memcpy((void*) state->m_cpuRegistersState->address,
           wos->getConcreteStore(true), wos->size);
    static_cast<S2EExecutionState*>(state)->m_runningConcrete = true;

    if (PrintModeSwitch) {
        m_s2e->getMessagesStream(state)
                << "Switched to concrete execution at pc = "
                << hexval(state->getPc()) << '\n';
    }
}

void S2EExecutor::switchToSymbolic(S2EExecutionState *state)
{
    assert(state->m_runningConcrete);

    //assert(os && os->isAllConcrete());

    // TODO: check that symbolic registers were not accessed
    // in shared location ! Ideas: use hw breakpoints, or instrument
    // translated code.

    ObjectState *wos = state->m_cpuRegistersObject;
    memcpy(wos->getConcreteStore(true),
           (void*) state->m_cpuRegistersState->address, wos->size);
    state->m_runningConcrete = false;

    if (PrintModeSwitch) {
        m_s2e->getMessagesStream(state)
                << "Switched to symbolic execution at pc = "
                << hexval(state->getPc()) << '\n';
    }
}



void S2EExecutor::copyOutConcretes(ExecutionState &state)
{
    return;
}

bool S2EExecutor::copyInConcretes(klee::ExecutionState &state)
{
    return true;
}

void S2EExecutor::doLoadBalancing()
{
    if (states.size() < 2) {
        return;
    }

    //Don't bother copying stuff if it's obvious that it'll very likely fail
    if (m_s2e->getCurrentProcessCount() == m_s2e->getMaxProcesses()) {
        return;
    }

    std::vector<ExecutionState*> allStates;

    foreach2(it, states.begin(), states.end()) {
        S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(*it);
        if (!s2estate->isZombie()) {
            allStates.push_back(s2estate);
        }
    }

    if (allStates.size() < 2) {
        return;
    }

    g_s2e->getDebugStream() << "LoadBalancing: starting\n";

    m_inLoadBalancing = true;

    vm_stop(RUN_STATE_SAVE_VM);

    unsigned parentId = m_s2e->getCurrentProcessIndex();
    m_s2e->getCorePlugin()->onProcessFork.emit(true, false, -1);
    int child = m_s2e->fork();
    if (child < 0) {
        //Fork did not succeed
        m_s2e->getCorePlugin()->onProcessFork.emit(false, false, -1);
        m_inLoadBalancing = false;
        vm_start();
        return;
    }

    unsigned size = allStates.size();
    unsigned n = size / 2;
    unsigned lower = child ? 0 : n;
    unsigned upper = child ? n : size;

    m_s2e->getCorePlugin()->onProcessFork.emit(false, child, parentId);

    g_s2e->getDebugStream() << "LoadBalancing: terminating states\n";

    for (unsigned i=lower; i<upper; ++i) {
        S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(allStates[i]);
        terminateStateAtFork(*s2estate);
    }

    m_s2e->getCorePlugin()->onProcessForkComplete.emit(child);

    m_inLoadBalancing = false;
    vm_start();
}

void S2EExecutor::stateSwitchTimerCallback(void *opaque)
{
    S2EExecutor *c = (S2EExecutor*)opaque;

    if (g_s2e_state) {
        c->doLoadBalancing();
        S2EExecutionState *nextState = c->selectNextState(g_s2e_state);
        if (nextState) {
            g_s2e_state = nextState;
        } else {
            //Do not reschedule the timer anymore
            return;
        }
    }

    qemu_mod_timer(c->m_stateSwitchTimer, qemu_get_clock_ms(rt_clock) + 100);
}

void S2EExecutor::initializeStateSwitchTimer()
{
    m_stateSwitchTimer = qemu_new_timer_ms(rt_clock, &stateSwitchTimerCallback, this);
    qemu_mod_timer(m_stateSwitchTimer, qemu_get_clock_ms(rt_clock) + 100);
}

void S2EExecutor::doStateSwitch(S2EExecutionState* oldState,
                                S2EExecutionState* newState)
{
    assert(oldState || newState);
    assert(!oldState || oldState->m_active);
    assert(!newState || !newState->m_active);
    assert(!newState || !newState->m_runningConcrete);

    //Some state save/restore logic in QEMU flushes the cache.
    //This can have bad effects in case of saving/restoring states
    //that were in the middle of a memory operation. Therefore,
    //we disable it here and re-enable after the new state has been activated.
    g_s2e_disable_tlb_flush = 1;

    //Clear the asynchronous request queue, which is not saved as part of
    //the snapshots by QEMU. This is the same mechanism as used by
    //load/save_vmstate, so it should work reliably
    qemu_aio_flush();
    bdrv_flush_all();

    m_s2e->getMessagesStream(oldState)
            << "Switching from state " << (oldState ? oldState->getID() : -1)
            << " to state " << (newState ? newState->getID() : -1) << '\n';

    const MemoryObject* cpuMo = oldState ? oldState->m_cpuSystemState :
                                            newState->m_cpuSystemState;

    if(oldState) {
        if(oldState->m_runningConcrete)
            switchToSymbolic(oldState);

        /*
        if(use_icount) {
            assert(env->s2e_icount == (uint64_t) (qemu_icount -
                            (env->icount_decr.u16.low + env->icount_extra)));
        }
        */

        //copyInConcretes(*oldState);
        oldState->getDeviceState()->saveDeviceState();
        //oldState->m_qemuIcount = qemu_icount;
        *oldState->m_timersState = timers_state;

        uint8_t *oldStore = oldState->m_cpuSystemObject->getConcreteStore();
        memcpy(oldStore, (uint8_t*) cpuMo->address, cpuMo->size);

        oldState->m_active = false;
    }

    if(newState) {
        timers_state = *newState->m_timersState;
        //qemu_icount = newState->m_qemuIcount;

        jmp_buf jmp_env;
        memcpy(&jmp_env, &env->jmp_env, sizeof(jmp_buf));

        const uint8_t *newStore = newState->m_cpuSystemObject->getConcreteStore();
        memcpy((uint8_t*) cpuMo->address, newStore, cpuMo->size);

        memcpy(&env->jmp_env, &jmp_env, sizeof(jmp_buf));

        newState->m_active = true;

        //Devices may need to write to memory, which can be done
        //after the state is activated
        //XXX: assigning g_s2e_state here is ugly but is required for restoreDeviceState...
        g_s2e_state = newState;
        newState->getDeviceState()->restoreDeviceState();
    }

    uint64_t totalCopied = 0;
    uint64_t objectsCopied = 0;
    foreach(MemoryObject* mo, m_saveOnContextSwitch) {
        if(mo == cpuMo)
            continue;

        if(oldState) {
            const ObjectState *oldOS = oldState->addressSpace.findObject(mo);
            ObjectState *oldWOS = oldState->addressSpace.getWriteable(mo, oldOS);
            uint8_t *oldStore = oldWOS->getConcreteStore();
            assert(oldStore);
            memcpy(oldStore, (uint8_t*) mo->address, mo->size);
        }

        if(newState) {
            const ObjectState *newOS = newState->addressSpace.findObject(mo);
            const uint8_t *newStore = newOS->getConcreteStore();
            assert(newStore);
            memcpy((uint8_t*) mo->address, newStore, mo->size);
        }

        totalCopied += mo->size;
        objectsCopied++;
    }

    if (VerboseStateSwitching) {
        s2e_debug_print("Copied %d (count=%d)\n", totalCopied, objectsCopied);
    }

    if(FlushTBsOnStateSwitch)
        tb_flush(env);

    g_s2e_disable_tlb_flush = 0;

    //m_s2e->getCorePlugin()->onStateSwitch.emit(oldState, newState);
}

ExecutionState* S2EExecutor::selectNonSpeculativeState(S2EExecutionState *state)
{
    ExecutionState *newState;
    std::set<ExecutionState*> empty;

    do {
        if (searcher->empty()) {
            newState = NULL;
            break;
        }

        newState = &searcher->selectState();

        if (newState->isSpeculative()) {
            //The searcher wants us to execute a speculative state.
            //The engine must make sure that such a state
            //satisfies all the path constraints.
            if (!resolveSpeculativeState(*newState)) {
                terminateState(*newState);
                updateStates(state);
                continue;
            }

            //Give a chance to the searcher to update
            //the status of the state.
            searcher->update(newState, empty, empty);
        }
    } while(newState->isSpeculative());

    if (!newState) {
        m_s2e->getWarningsStream() << "All states were terminated" << '\n';
        foreach(S2EExecutionState* s, m_deletedStates) {
            //Leave the current state in a zombie form to let QEMU exit gracefully.
            if (s != g_s2e_state) {
                unrefS2ETb(s->m_lastS2ETb);
                s->m_lastS2ETb = NULL;
                delete s;
            }
        }
        m_deletedStates.clear();
        qemu_system_shutdown_request();
    }

    return newState;
}

S2EExecutionState* S2EExecutor::selectNextState(S2EExecutionState *state)
{
    assert(state->m_active);
    updateStates(state);

    ExecutionState *nstate = selectNonSpeculativeState(state);
    if (nstate == NULL) {
        return NULL;
    }

    ExecutionState& newKleeState = *nstate;
    assert(dynamic_cast<S2EExecutionState*>(&newKleeState));

    S2EExecutionState* newState =
            static_cast<S2EExecutionState*  >(&newKleeState);
    assert(states.find(newState) != states.end());

    if(!state->m_active) {
        /* Current state might be switched off by merge method */
        state = NULL;
    }

    if(newState != state) {
        g_s2e->getCorePlugin()->onStateSwitch.emit(state, newState);
        vm_stop(RUN_STATE_SAVE_VM);
        doStateSwitch(state, newState);
        vm_start();
    }

    //We can't free the state immediately if it is the current state.
    //Do it now.
    foreach(S2EExecutionState* s, m_deletedStates) {
        assert(s != newState);
        unrefS2ETb(s->m_lastS2ETb);
        s->m_lastS2ETb = NULL;
        delete s;
    }
    m_deletedStates.clear();

    return newState;
}

/** Simulate start of function execution, creating KLEE structs of required */
void S2EExecutor::prepareFunctionExecution(S2EExecutionState *state,
                            llvm::Function *function,
                            const std::vector<klee::ref<klee::Expr> > &args)
{
    KFunction *kf;
    typeof(kmodule->functionMap.begin()) it =
            kmodule->functionMap.find(function);
    if(it != kmodule->functionMap.end()) {
        kf = it->second;
    } else {

        unsigned cIndex = kmodule->constants.size();
        kf = kmodule->updateModuleWithFunction(function);

        for(unsigned i = 0; i < kf->numInstructions; ++i)
            bindInstructionConstants(kf->instructions[i]);

        /* Update global functions (new functions can be added
           while creating added function) */
        for (Module::iterator i = kmodule->module->begin(),
                              ie = kmodule->module->end(); i != ie; ++i) {
            Function *f = i;
            ref<klee::ConstantExpr> addr(0);

            // If the symbol has external weak linkage then it is implicitly
            // not defined in this module; if it isn't resolvable then it
            // should be null.
            if (f->hasExternalWeakLinkage() &&
                    !externalDispatcher->resolveSymbol(f->getNameStr())) {
                addr = Expr::createPointer(0);
            } else {
                addr = Expr::createPointer((uintptr_t) (void*) f);
                legalFunctions.insert((uint64_t) (uintptr_t) (void*) f);
            }

            globalAddresses.insert(std::make_pair(f, addr));
        }

        kmodule->constantTable.resize(kmodule->constants.size());

        for(unsigned i = cIndex; i < kmodule->constants.size(); ++i) {
            Cell &c = kmodule->constantTable[i];
            c.value = evalConstant(kmodule->constants[i]);
        }
    }

    /* Emulate call to a TB function */
    state->prevPC = state->pc;

    state->pushFrame(state->pc, kf);
    state->pc = kf->instructions;

    if(statsTracker)
        statsTracker->framePushed(*state,
            &state->stack[state->stack.size()-2]);

    /* Pass argument */
    for(unsigned i = 0; i < args.size(); ++i)
        bindArgument(kf, i, *state, args[i]);
}

inline void S2EExecutor::executeOneInstruction(S2EExecutionState *state)
{
    ++state->m_stats.m_statInstructionCountSymbolic;

    //int64_t start_clock = get_clock();
    cpu_disable_ticks();

    KInstruction *ki = state->pc;

    if ( S2EDebugInstructions ) {
    m_s2e->getDebugStream(state) << "executing "
              << ki->inst->getParent()->getParent()->getNameStr()
              << ": " << *ki->inst << '\n';
    }

    stepInstruction(*state);

    bool shouldExitCpu = false;
    try {

        executeInstruction(*state, ki);

#ifdef S2E_TRACE_EFLAGS
        ref<Expr> efl = state->readCpuRegister(offsetof(CPUState, cc_src), klee::Expr::Int32);
        m_s2e->getDebugStream() << std::hex << state->getPc() << "  CC_SRC " << efl << '\n';
#endif

    } catch(CpuExitException&) {
        // Instruction that forks should never be interrupted
        // (and, consequently, restarted)
        assert(addedStates.empty());
        shouldExitCpu = true;
    }

    if (getMaxMemory()) {
      if ((stats::instructions & 0xFFFF) == 0) {
        // We need to avoid calling GetMallocUsage() often because it
        // is O(elts on freelist). This is really bad since we start
        // to pummel the freelist once we hit the memory cap.
        unsigned mbs = sys::Process::GetTotalMemoryUsage() >> 20;

        if (mbs > getMaxMemory()) {
          if (mbs > getMaxMemory() + 100) {
            // just guess at how many to kill
            unsigned numStates = states.size();
            unsigned toKill = std::max(1U, numStates - numStates*getMaxMemory()/mbs);

            if (getMaxMemoryInhibit())
              klee_warning("killing %d states (over memory cap)",
                           toKill);

            std::vector<ExecutionState*> arr(states.begin(), states.end());
            for (unsigned i=0,N=arr.size(); N && i<toKill; ++i,--N) {
              unsigned idx = rand() % N;

              // Make two pulls to try and not hit a state that
              // covered new code.
              if (arr[idx]->coveredNew)
                idx = rand() % N;

              std::swap(arr[idx], arr[N-1]);
              terminateStateEarly(*arr[N-1], "memory limit");
            }
          }
          atMemoryLimit = true;
        } else {
          atMemoryLimit = false;
        }
      }
    }

    /* TODO: timers */
    /* TODO: MaxMemory */

    updateStates(state);

    // assume that symbex is 50 times slower
    cpu_enable_ticks();

    //int64_t inst_clock = get_clock() - start_clock;
    //cpu_adjust_clock(- inst_clock*(1-0.02));

    //Handle the case where we killed the current state inside processFork
    if (m_forkProcTerminateCurrentState) {
        state->writeCpuState(CPU_OFFSET(exception_index), EXCP_S2E, 8*sizeof(int));
        state->zombify();
        m_forkProcTerminateCurrentState = false;
        throw CpuExitException();
    }

    //S2E doesn't know if the current state can be run
    //if concolic fork marks it as speculative.
    if (state->isSpeculative()) {
        throw CpuExitException();
    }

    if(shouldExitCpu)
        throw CpuExitException();
}

void S2EExecutor::finalizeTranslationBlockExec(S2EExecutionState *state)
{
    if(!state->m_needFinalizeTBExec)
        return;

    state->m_needFinalizeTBExec = false;
    assert(state->stack.size() != 1);

    assert(!state->m_runningConcrete);

    if (VerboseTbFinalize) {
        m_s2e->getDebugStream() << "Finalizing TB execution " << state->getID() << '\n';
        foreach(const StackFrame& fr, state->stack) {
            m_s2e->getDebugStream() << fr.kf->function->getNameStr() << '\n';
        }
    }

    /* Information for GETPC() macro */
    /* XXX: tc_ptr could be already freed at this moment */
    /*      however, GETPC is not used in S2E anyway */
    //g_s2e_exec_ret_addr = 0; //state->getTb()->tc_ptr;

    while(state->stack.size() != 1) {
        executeOneInstruction(state);
    }

    state->prevPC = 0;
    state->pc = m_dummyMain->instructions;

    //copyOutConcretes(*state);

}

#ifdef _WIN32

extern "C" volatile LONG g_signals_enabled;

typedef int sigset_t;

static void s2e_disable_signals(sigset_t *oldset)
{
    while(InterlockedCompareExchange(&g_signals_enabled, 0, 1) == 0)
       ;
}

static void s2e_enable_signals(sigset_t *oldset)
{
    g_signals_enabled = 1;
}

#else

static void s2e_disable_signals(sigset_t *oldset)
{
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, oldset);
}

static void s2e_enable_signals(sigset_t *oldset)
{
    sigprocmask(SIG_SETMASK, oldset, NULL);
}

#endif

uintptr_t S2EExecutor::executeTranslationBlockKlee(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    tb_function_args[0] = env;
    tb_function_args[1] = 0;
    tb_function_args[2] = 0;

    assert(state->m_active && !state->m_runningConcrete);
    assert(state->stack.size() == 1);
    assert(state->pc == m_dummyMain->instructions);

    ++state->m_stats.m_statTranslationBlockSymbolic;

    /* Update state */
    //if (!copyInConcretes(*state)) {
    //    std::cerr << "external modified read-only object" << '\n';
    //    exit(1);
    //}

    /* loop until TB chain is not broken */
    do {
        /* Make sure to init tb_next value */
        tcg_llvm_runtime.goto_tb = 0xff;

        /* Generate LLVM code if necessary */
        if(!tb->llvm_function) {
            cpu_gen_llvm(env, tb);
            assert(tb->llvm_function);
        }

        if(tb->s2e_tb != state->m_lastS2ETb) {
            unrefS2ETb(state->m_lastS2ETb);
            state->m_lastS2ETb = tb->s2e_tb;
            state->m_lastS2ETb->refCount += 1;
        }

        /* Prepare function execution */
        prepareFunctionExecution(state,
                tb->llvm_function, std::vector<ref<Expr> >(1,
                    Expr::createPointer((uint64_t) tb_function_args)));

        /* Information for GETPC() macro */
        //g_s2e_exec_ret_addr = tb->tc_ptr;

        /* Execute */
        while(state->stack.size() != 1) {
            executeOneInstruction(state);

            /* Check to goto_tb request */
            if(tcg_llvm_runtime.goto_tb != 0xff) {
                assert(tcg_llvm_runtime.goto_tb < 2);

                /* The next should be atomic with respect to signals */
                /* XXX: what else should we block ? */
#ifdef _WIN32
                //Timers can run in different threads...
                s2e_disable_signals(NULL);
#else
                sigset_t set, oldset;
                sigfillset(&set);
                sigprocmask(SIG_BLOCK, &set, &oldset);
#endif

                TranslationBlock* next_tb =
                        tb->s2e_tb_next[tcg_llvm_runtime.goto_tb];

                if(next_tb) {
#ifndef NDEBUG
                    TranslationBlock* old_tb = tb;
#endif

                    assert(state->stack.size() == 2);
                    state->popFrame();

                    tb = next_tb;
                    env->s2e_current_tb = tb;
                    //g_s2e_exec_ret_addr = tb->tc_ptr;

                    /* assert that blocking works */
#ifdef _WIN32
                    assert(old_tb->s2e_tb_next[tcg_llvm_runtime.goto_tb] == tb);
                    cleanupTranslationBlock(state, tb);
                    s2e_enable_signals(NULL);
                    break;
#else
                    assert(old_tb->s2e_tb_next[tcg_llvm_runtime.goto_tb] == tb);
                    cleanupTranslationBlock(state, tb);
                    sigprocmask(SIG_SETMASK, &oldset, NULL);
                    break;
#endif
                }

                /* the block was unchained by signal handler */
                tcg_llvm_runtime.goto_tb = 0xff;
#ifdef _WIN32
                s2e_enable_signals(NULL);
#else
                sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
            }

        }
        state->prevPC = 0;
        state->pc = m_dummyMain->instructions;

    } while(tcg_llvm_runtime.goto_tb != 0xff);

    //g_s2e_exec_ret_addr = 0;

    /* Get return value */
    ref<Expr> resExpr =
            getDestCell(*state, state->pc).value;
    assert(isa<klee::ConstantExpr>(resExpr));

    //copyOutConcretes(*state);

    return cast<klee::ConstantExpr>(resExpr)->getZExtValue();
}

uintptr_t S2EExecutor::executeTranslationBlockConcrete(S2EExecutionState *state,
                                                       TranslationBlock *tb)
{
    assert(state->m_active && state->m_runningConcrete);
    ++state->m_stats.m_statTranslationBlockConcrete;

    uintptr_t ret = 0;
    memcpy(s2e_cpuExitJmpBuf, env->jmp_env, sizeof(env->jmp_env));

    if(s2e_setjmp(env->jmp_env)) {
        memcpy(env->jmp_env, s2e_cpuExitJmpBuf, sizeof(env->jmp_env));
        throw CpuExitException();
    } else {

        if(execute_llvm) {
            ret = tcg_llvm_qemu_tb_exec(env, tb);
        } else {
            ret = tcg_qemu_tb_exec(env, tb->tc_ptr);
        }
    }

    memcpy(env->jmp_env, s2e_cpuExitJmpBuf, sizeof(env->jmp_env));
    return ret;
}

static inline void s2e_tb_reset_jump(TranslationBlock *tb, unsigned int n)
{
    TranslationBlock *tb1, *tb_next, **ptb;
    unsigned int n1;

    tb1 = tb->jmp_next[n];
    if (tb1 != NULL) {
        /* find head of list */
        for(;;) {
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == 2)
                break;
            tb1 = tb1->jmp_next[n1];
        }
        /* we are now sure now that tb jumps to tb1 */
        tb_next = tb1;

        /* remove tb from the jmp_first list */
        ptb = &tb_next->jmp_first;
        for(;;) {
            tb1 = *ptb;
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == n && tb1 == tb)
                break;
            ptb = &tb1->jmp_next[n1];
        }
        *ptb = tb->jmp_next[n];
        tb->jmp_next[n] = NULL;

        /* suppress the jump to next tb in generated code */
        tb_set_jmp_target(tb, n, (uintptr_t)(tb->tc_ptr + tb->tb_next_offset[n]));
        tb->s2e_tb_next[n] = NULL;
    }
}


//XXX: inline causes compiler internal errors
static void s2e_tb_reset_jump_smask(TranslationBlock* tb, unsigned int n,
                                           uint64_t smask, int depth = 0)
{
    TranslationBlock *tb1 = tb->s2e_tb_next[n];
    sigset_t oldset;
    if (depth == 0) {
        s2e_disable_signals(&oldset);
    }

    if(tb1) {
        if(depth > 2 || (smask & tb1->reg_rmask) || (smask & tb1->reg_wmask)
                             || (tb1->helper_accesses_mem & 4)) {
            s2e_tb_reset_jump(tb, n);
        } else if(tb1 != tb) {
            s2e_tb_reset_jump_smask(tb1, 0, smask, depth + 1);
            s2e_tb_reset_jump_smask(tb1, 1, smask, depth + 1);
        }
    }

    if (depth == 0) {
        s2e_enable_signals(&oldset);
    }
}

uintptr_t S2EExecutor::executeTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    //Avoid incrementing stats everytime, very expensive.
    static unsigned doStatsIncrementCount= 0;
    assert(state->isActive());

    bool executeKlee = m_executeAlwaysKlee;

    /* Think how can we optimize if symbex is disabled */
    if(true/* state->m_symbexEnabled*/) {
        if(state->m_startSymbexAtPC != (uint64_t) -1) {
            executeKlee |= (state->getPc() == state->m_startSymbexAtPC);
            state->m_startSymbexAtPC = (uint64_t) -1;
        }

        //XXX: hack to run code symbolically that may be delayed because of interrupts.
        //Size check is important to avoid expensive calls to getPc/getPid in the common case
        if (state->m_toRunSymbolically.size() > 0 &&  state->m_toRunSymbolically.find(std::make_pair(state->getPc(), state->getPid()))
            != state->m_toRunSymbolically.end()) {
            executeKlee = true;
            state->m_toRunSymbolically.erase(std::make_pair(state->getPc(), state->getPid()));
        }

        if(!executeKlee) {
            //XXX: This should be fixed to make sure that helpers do not read/write corrupted data
            //because they think that execution is concrete while it should be symbolic (see issue #30).
            if (!m_forceConcretizations) {
#if 1
            /* We can not execute TB natively if it reads any symbolic regs */
            uint64_t smask = state->getSymbolicRegistersMask();
            if(smask || (tb->helper_accesses_mem & 4)) {
                if((smask & tb->reg_rmask) || (smask & tb->reg_wmask)
                         || (tb->helper_accesses_mem & 4)) {
                    /* TB reads symbolic variables */
                    executeKlee = true;

                } else {
                    s2e_tb_reset_jump_smask(tb, 0, smask);
                    s2e_tb_reset_jump_smask(tb, 1, smask);

                    /* XXX: check whether we really have to unlink the block */
                    /*
                    tb->jmp_first = (TranslationBlock *)((intptr_t)tb | 2);
                    tb->jmp_next[0] = NULL;
                    tb->jmp_next[1] = NULL;
                    if(tb->tb_next_offset[0] != 0xffff)
                        tb_set_jmp_target(tb, 0,
                              (uintptr_t)(tb->tc_ptr + tb->tb_next_offset[0]));
                    if(tb->tb_next_offset[1] != 0xffff)
                        tb_set_jmp_target(tb, 1,
                              (uintptr_t)(tb->tc_ptr + tb->tb_next_offset[1]));
                    tb->s2e_tb_next[0] = NULL;
                    tb->s2e_tb_next[1] = NULL;
                    */
                }
            }
#else
            executeKlee |= !state->m_cpuRegistersObject->isAllConcrete();
#endif
            } //forced concretizations
        }
    }

    if(executeKlee) {
        if(state->m_runningConcrete) {
            TimerStatIncrementer t(stats::concreteModeTime);
            switchToSymbolic(state);
        }

        TimerStatIncrementer t(stats::symbolicModeTime);

        return executeTranslationBlockKlee(state, tb);

    } else {
        //g_s2e_exec_ret_addr = 0;
        if(!state->m_runningConcrete)
            switchToConcrete(state);

        if (!((++doStatsIncrementCount) & 0xFFF)) {
            TimerStatIncrementer t(stats::concreteModeTime);
        }

        return executeTranslationBlockConcrete(state, tb);
    }
}

void S2EExecutor::cleanupTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    assert(state->m_active);

    //g_s2e_exec_ret_addr = 0;

    while(state->stack.size() != 1)
        state->popFrame();

    state->prevPC = 0;
    state->pc = m_dummyMain->instructions;

#if 0
    if(!state->m_runningConcrete) {
        /* If we was interupted while symbexing, we can be resumed
           for concrete execution */
        copyOutConcretes(*state);
    }
#endif
}

klee::ref<klee::Expr> S2EExecutor::executeFunction(S2EExecutionState *state,
                            llvm::Function *function,
                            const std::vector<klee::ref<klee::Expr> >& args)
{
    assert(!state->m_runningConcrete);
    assert(!state->prevPC);
    assert(state->stack.size() == 1);

    /* Update state */
    //if (!copyInConcretes(*state)) {
    //    std::cerr << "external modified read-only object" << '\n';
    //    exit(1);
    //}

    KInstIterator callerPC = state->pc;
    uint32_t callerStackSize = state->stack.size();

    /* Prepare function execution */
    prepareFunctionExecution(state, function, args);

    /* Execute */
    while(state->stack.size() != callerStackSize) {
        executeOneInstruction(state);
    }

    if(callerPC == m_dummyMain->instructions) {
        assert(state->stack.size() == 1);
        state->prevPC = 0;
        state->pc = callerPC;
    }

    ref<Expr> resExpr(0);
    if(function->getReturnType()->getTypeID() != Type::VoidTyID)
        resExpr = getDestCell(*state, state->pc).value;

    //copyOutConcretes(*state);

    return resExpr;
}

klee::ref<klee::Expr> S2EExecutor::executeFunction(S2EExecutionState *state,
                            const std::string& functionName,
                            const std::vector<klee::ref<klee::Expr> >& args)
{
    llvm::Function *function = kmodule->module->getFunction(functionName);
    assert(function && "function with given name do not exists in LLVM module");
    return executeFunction(state, function, args);
}

void S2EExecutor::deleteState(klee::ExecutionState *state)
{
    assert(dynamic_cast<S2EExecutionState*>(state));
    processTree->remove(state->ptreeNode);
    m_deletedStates.push_back(static_cast<S2EExecutionState*>(state));
}

void S2EExecutor::doStateFork(S2EExecutionState *originalState,
                 const vector<S2EExecutionState*>& newStates,
                 const vector<ref<Expr> >& newConditions)
{
    assert(originalState->m_active && !originalState->m_runningConcrete);

    llvm::raw_ostream& out = m_s2e->getMessagesStream(originalState);
    out << "Forking state " << originalState->getID()
            << " at pc = " << hexval(originalState->getPc()) << '\n';

    qemu_aio_flush();
    bdrv_flush_all();

    for(unsigned i = 0; i < newStates.size(); ++i) {
        S2EExecutionState* newState = newStates[i];

        if (VerboseFork) {
            out << "    state " << newState->getID() << " with condition "
            << newConditions[i] << '\n';
        }

        if(newState != originalState) {
            newState->m_needFinalizeTBExec = true;

            //XXX: How do we make this stuff atomic? Disable all signals?
            //vm_stop(RUN_STATE_SAVE_VM);
            newState->getDeviceState()->saveDeviceState();
            //vm_start();

            //newState->m_qemuIcount = qemu_icount;
            *newState->m_timersState = timers_state;

            /* Save CPU state */
            const MemoryObject* cpuMo = newState->m_cpuSystemState;
            uint8_t *cpuStore = newState->m_cpuSystemObject->getConcreteStore();
            memcpy(cpuStore, (uint8_t*) cpuMo->address, cpuMo->size);
            newState->m_active = false;

            /* Save all other objects */
            foreach(MemoryObject* mo, m_saveOnContextSwitch) {
                if(mo == cpuMo)
                    continue;

                const ObjectState *os = newState->addressSpace.findObject(mo);
                ObjectState *wos = newState->addressSpace.getWriteable(mo, os);
                uint8_t *store = wos->getConcreteStore();

                assert(store);
                memcpy(store, (uint8_t*) mo->address, mo->size);
            }
        }
    }

    if (VerboseFork) {
        m_s2e->getDebugStream() << "Stack frame at fork:" << '\n';
        foreach(const StackFrame& fr, originalState->stack) {
            m_s2e->getDebugStream() << fr.kf->function->getNameStr() << '\n';
        }
    }

    m_s2e->getCorePlugin()->onStateFork.emit(originalState,
                                             newStates, newConditions);

}

S2EExecutor::StatePair S2EExecutor::fork(ExecutionState &current,
                            ref<Expr> condition, bool isInternal)
{
    assert(dynamic_cast<S2EExecutionState*>(&current));
    assert(!static_cast<S2EExecutionState*>(&current)->m_runningConcrete);

    StatePair res;

    if (ConcolicMode) {
        res = Executor::concolicFork(current, condition, isInternal);
    } else {
        res = Executor::fork(current, condition, isInternal);
    }

    if(res.first && res.second) {

        assert(dynamic_cast<S2EExecutionState*>(res.first));
        assert(dynamic_cast<S2EExecutionState*>(res.second));

        std::vector<S2EExecutionState*> newStates(2);
        std::vector<ref<Expr> > newConditions(2);

        newStates[0] = static_cast<S2EExecutionState*>(res.first);
        newStates[1] = static_cast<S2EExecutionState*>(res.second);

        newConditions[0] = condition;
        newConditions[1] = klee::NotExpr::create(condition);

        doStateFork(static_cast<S2EExecutionState*>(&current),
                       newStates, newConditions);
    }
    return res;
}

void S2EExecutor::branch(klee::ExecutionState &state,
          const vector<ref<Expr> > &conditions,
          vector<ExecutionState*> &result)
{
    assert(dynamic_cast<S2EExecutionState*>(&state));
    assert(!static_cast<S2EExecutionState*>(&state)->m_runningConcrete);

    Executor::branch(state, conditions, result);

    unsigned n = conditions.size();

    vector<S2EExecutionState*> newStates;
    vector<ref<Expr> > newConditions;

    newStates.reserve(n);
    newConditions.reserve(n);

    for(unsigned i = 0; i < n; ++i) {
        if(result[i]) {
            assert(dynamic_cast<S2EExecutionState*>(result[i]));
            newStates.push_back(static_cast<S2EExecutionState*>(result[i]));
            newConditions.push_back(conditions[i]);
        }
    }

    if(newStates.size() > 1) {
        doStateFork(static_cast<S2EExecutionState*>(&state),
                       newStates, newConditions);
    }
}

bool S2EExecutor::merge(klee::ExecutionState &_base, klee::ExecutionState &_other)
{
    assert(dynamic_cast<S2EExecutionState*>(&_base));
    assert(dynamic_cast<S2EExecutionState*>(&_other));
    S2EExecutionState& base = static_cast<S2EExecutionState&>(_base);
    S2EExecutionState& other = static_cast<S2EExecutionState&>(_other);

    /* Ensure that both states are inactive, otherwise merging will not work */
    if(base.m_active)
        doStateSwitch(&base, NULL);
    else if(other.m_active)
        doStateSwitch(&other, NULL);

    if(base.merge(other)) {
        m_s2e->getMessagesStream(&base)
                << "Merged with state " << other.getID() << '\n';
        return true;
    } else {
        m_s2e->getDebugStream(&base)
                << "Merge with state " << other.getID() << " failed" << '\n';
        return false;
    }
}

void S2EExecutor::terminateStateEarly(klee::ExecutionState &state, const llvm::Twine &message)
{
    S2EExecutionState  *s2estate = static_cast<S2EExecutionState*>(&state);
    m_s2e->getMessagesStream(s2estate) << message << '\n';
    m_s2e->getCorePlugin()->onTestCaseGeneration.emit(s2estate, message.str());
    terminateState(state);
}

void S2EExecutor::terminateState(ExecutionState &s)
{
    S2EExecutionState& state = static_cast<S2EExecutionState&>(s);
    m_s2e->getCorePlugin()->onStateKill.emit(&state);

    terminateStateAtFork(state);
    state.zombify();

    g_s2e->getWarningsStream().flush();
    g_s2e->getDebugStream().flush();

    //No need for exiting the loop if we kill another state.
    if (!m_inLoadBalancing && (&state == g_s2e_state)) {
        state.writeCpuState(CPU_OFFSET(exception_index), EXCP_S2E, 8*sizeof(int));
        throw CpuExitException();
    }
}

void S2EExecutor::terminateStateAtFork(S2EExecutionState &state)
{
    Executor::terminateState(state);
}

inline void S2EExecutor::setCCOpEflags(S2EExecutionState *state)
{
    uint32_t cc_op = 0;

    // Check wether any of cc_op, cc_src, cc_dst or cc_tmp are symbolic
    if((state->getSymbolicRegistersMask() & (0xf<<1)) || m_executeAlwaysKlee) {
        // call set_cc_op_eflags only if cc_op is symbolic or cc_op != CC_OP_EFLAGS
        bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(cc_op),
                                                 &cc_op, sizeof(cc_op));
        if(!ok || cc_op != CC_OP_EFLAGS) {
            try {
                if(state->m_runningConcrete)
                    switchToSymbolic(state);
                TimerStatIncrementer t(stats::symbolicModeTime);
                executeFunction(state, "helper_set_cc_op_eflags");
            } catch(s2e::CpuExitException&) {
                updateStates(state);
                s2e_longjmp(env->jmp_env, 1);
            }
        }
    } else {
        bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(cc_op),
                                                 &cc_op, sizeof(cc_op));
        assert(ok);
        if(cc_op != CC_OP_EFLAGS) {
            if(!state->m_runningConcrete)
                switchToConcrete(state);
            //TimerStatIncrementer t(stats::concreteModeTime);
            helper_set_cc_op_eflags();
        }
    }
}

inline void S2EExecutor::doInterrupt(S2EExecutionState *state, int intno,
                                     int is_int, int error_code,
                                     uint64_t next_eip, int is_hw)
{
    if(state->m_cpuRegistersObject->isAllConcrete() && !m_executeAlwaysKlee) {
        if(!state->m_runningConcrete)
            switchToConcrete(state);
        //TimerStatIncrementer t(stats::concreteModeTime);
        s2e_do_interrupt_all(intno, is_int, error_code, next_eip, is_hw);
    } else {
        if(state->m_runningConcrete)
            switchToSymbolic(state);
        std::vector<klee::ref<klee::Expr> > args(5);
        args[0] = klee::ConstantExpr::create(intno, sizeof(int)*8);
        args[1] = klee::ConstantExpr::create(is_int, sizeof(int)*8);
        args[2] = klee::ConstantExpr::create(error_code, sizeof(int)*8);
        args[3] = klee::ConstantExpr::create(next_eip, sizeof(target_ulong)*8);
        args[4] = klee::ConstantExpr::create(is_hw, sizeof(int)*8);
        try {
            TimerStatIncrementer t(stats::symbolicModeTime);
            executeFunction(state, "s2e_do_interrupt_all", args);
        } catch(s2e::CpuExitException&) {
            updateStates(state);
            s2e_longjmp(env->jmp_env, 1);
        }
    }
}

void S2EExecutor::setupTimersHandler()
{
    m_s2e->getCorePlugin()->onTimer.connect(
            sigc::bind(sigc::ptr_fun(&onAlarm), 0));
}

/** Suspend the given state (does not kill it) */
bool S2EExecutor::suspendState(S2EExecutionState *state, bool onlyRemoveFromPtree)
{
    if (onlyRemoveFromPtree) {
        processTree->deactivate(state->ptreeNode);
        return true;
    }

    if (searcher)  {
        searcher->removeState(state, NULL);
        size_t r = states.erase(state);
        assert(r == 1);
        processTree->deactivate(state->ptreeNode);
        return true;
    }
    return false;
}

bool S2EExecutor::resumeState(S2EExecutionState *state, bool onlyAddToPtree)
{
    if (onlyAddToPtree) {
        processTree->activate(state->ptreeNode);
        return true;
    }

    if (searcher)  {
        if (states.find(state) != states.end()) {
            return false;
        }
        processTree->activate(state->ptreeNode);
        states.insert(state);
        searcher->addState(state, NULL);
        return true;
    }
    return false;
}


void S2EExecutor::unrefS2ETb(S2ETranslationBlock* s2e_tb)
{
    if(s2e_tb && 0 == --s2e_tb->refCount) {
        if(s2e_tb->llvm_function && !KeepLLVMFunctions) {
            S2EExternalDispatcher *s2eDispatcher = static_cast<S2EExternalDispatcher*>(externalDispatcher);
            s2eDispatcher->removeFunction(s2e_tb->llvm_function);
            kmodule->removeFunction(s2e_tb->llvm_function);
        }
        foreach(void* s, s2e_tb->executionSignals) {
            delete static_cast<ExecutionSignal*>(s);
        }
    }
}

void S2EExecutor::queueStateForMerge(S2EExecutionState *state)
{
    if(dynamic_cast<MergingSearcher*>(searcher) == NULL) {
        m_s2e->getWarningsStream(state)
                << "State merging request is ignored because"
                   " MergingSearcher is not activated\n";
        return;
    }
    assert(state->m_active && !state->m_runningConcrete && state->pc);

    /* Ignore attempt to merge states immediately after previous attempt */
    if(state->m_lastMergeICount == state->getTotalInstructionCount() - 1)
        return;

    state->m_lastMergeICount = state->getTotalInstructionCount();

    uint64_t mergePoint = 0;
    if(!state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]), &mergePoint, 8)) {
        m_s2e->getWarningsStream(state)
                << "Warning: merge request for a state with symbolic ESP" << '\n';
    }
    mergePoint = hash64(mergePoint);
    mergePoint = hash64(state->getPc(), mergePoint);

    m_s2e->getMessagesStream(state) << "Queueing state for merging" << '\n';

    static_cast<MergingSearcher*>(searcher)->queueStateForMerge(*state, mergePoint);
    throw CpuExitException();
}

void S2EExecutor::updateStats(S2EExecutionState *state)
{
    state->m_stats.updateStats(state);
    processTimers(state, 0);
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

S2EExecutionState* s2e_create_initial_state(S2E *s2e)
{
    return s2e->getExecutor()->createInitialState();
}

void s2e_initialize_execution(S2E *s2e, S2EExecutionState *initial_state,
                              int execute_always_klee)
{
    s2e->getExecutor()->initializeExecution(initial_state, execute_always_klee);
    //XXX: move it to better place (signal handler for this?)
    tcg_register_helper((void*)&s2e_tcg_execution_handler, "s2e_tcg_execution_handler");
    tcg_register_helper((void*)&s2e_tcg_custom_instruction_handler, "s2e_tcg_custom_instruction_handler");
}

void s2e_register_cpu(S2E *s2e, S2EExecutionState *initial_state,
                      CPUX86State *cpu_env)
{
    s2e->getExecutor()->registerCpu(initial_state, cpu_env);
}

void s2e_register_ram(S2E* s2e, S2EExecutionState *initial_state,
        uint64_t start_address, uint64_t size,
        uint64_t host_address, int is_shared_concrete,
        int save_on_context_switch, const char *name)
{
    s2e->getExecutor()->registerRam(initial_state,
        start_address, size, host_address, is_shared_concrete,
        save_on_context_switch, name);
}

void s2e_register_dirty_mask(S2E *s2e, S2EExecutionState *initial_state,
                            uint64_t host_address, uint64_t size)
{
    s2e->getExecutor()->registerDirtyMask(initial_state, host_address, size);
}


uintptr_t s2e_qemu_tb_exec(struct CPUX86State* env1, struct TranslationBlock* tb)
{
    /*s2e->getDebugStream() << "icount=" << std::dec << s2e_get_executed_instructions()
            << " pc=0x" << std::hex << state->getPc() << std::dec
            << '\n';   */
    env = env1;
    g_s2e_state->setRunningExceptionEmulationCode(false);

    try {
        uintptr_t ret = g_s2e->getExecutor()->executeTranslationBlock(g_s2e_state, tb);
        return ret;
    } catch(s2e::CpuExitException&) {
        g_s2e->getExecutor()->updateStates(g_s2e_state);
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_qemu_finalize_tb_exec(S2E *s2e, S2EExecutionState* state)
{
    try {
        s2e->getExecutor()->finalizeTranslationBlockExec(state);
    } catch(s2e::CpuExitException&) {
        s2e->getExecutor()->updateStates(state);
        s2e_longjmp(env->jmp_env, 1);
    }
}

void s2e_qemu_cleanup_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    return s2e->getExecutor()->cleanupTranslationBlock(state, tb);
}

void s2e_set_cc_op_eflags(struct CPUX86State *env1)
{
    env = env1;
    g_s2e->getExecutor()->setCCOpEflags(g_s2e_state);
}

/**
 *  We also need to track when execution enters/exits emulation code.
 *  Some plugins do not care about what memory accesses the emulation
 *  code performs internally, therefore, there must be a means for such
 *  plugins to enable/disable tracing upon exiting/entering
 *  the emulation code.
 */
void do_interrupt_all(int intno, int is_int, int error_code,
                      target_ulong next_eip, int is_hw)
{
    g_s2e_state->setRunningExceptionEmulationCode(true);
    s2e_on_exception(intno);

    g_s2e->getExecutor()->doInterrupt(g_s2e_state, intno, is_int, error_code,
                                    next_eip, is_hw);

    g_s2e_state->setRunningExceptionEmulationCode(false);
}

/**
 *  Checks whether we are trying to access an I/O port that returns a symbolic value.
 */
void s2e_switch_to_symbolic(S2E *s2e, S2EExecutionState *state)
{
    //XXX: For now, we assume that symbolic hardware, when triggered,
    //will want to start symbexec.
    state->enableSymbolicExecution();
    state->jumpToSymbolic();
}

void s2e_ensure_symbolic(S2E *s2e, S2EExecutionState *state)
{
    state->jumpToSymbolic();
}

/** Tlb cache helpers */
void s2e_update_tlb_entry(S2EExecutionState* state, CPUX86State* env,
                          int mmu_idx, uint64_t virtAddr, uint64_t hostAddr)
{
#ifdef S2E_ENABLE_S2E_TLB
    state->updateTlbEntry(env, mmu_idx, virtAddr, hostAddr);
#endif
}

void s2e_dma_read(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    return g_s2e_state->dmaRead(hostAddress, buf, size);
}

void s2e_dma_write(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    return g_s2e_state->dmaWrite(hostAddress, buf, size);
}

void s2e_tb_alloc(S2E*, TranslationBlock *tb)
{
    tb->s2e_tb = new S2ETranslationBlock;
    tb->s2e_tb->llvm_function = NULL;
    tb->s2e_tb->refCount = 1;

    /* Push one copy of a signal to use it as a cache */
    tb->s2e_tb->executionSignals.push_back(new s2e::ExecutionSignal);

    tb->s2e_tb_next[0] = 0;
    tb->s2e_tb_next[1] = 0;
}

void s2e_set_tb_function(S2E*, TranslationBlock *tb)
{
    tb->s2e_tb->llvm_function = tb->llvm_function;
}

void s2e_tb_free(S2E* s2e, TranslationBlock *tb)
{
    s2e->getExecutor()->unrefS2ETb(tb->s2e_tb);
}

void s2e_flush_tlb_cache()
{
    g_s2e_state->flushTlbCache();
}

void s2e_flush_tlb_cache_page(void *objectState, int mmu_idx, int index)
{
    g_s2e_state->flushTlbCachePage(static_cast<klee::ObjectState*>(objectState), mmu_idx, index);
}

int s2e_is_load_balancing()
{
    return g_s2e->getExecutor()->isLoadBalancing();
}

#ifdef S2E_DEBUG_MEMORY
#ifdef __linux__
#warning Compiling with memory debugging support...

void *operator new(size_t s)
{
    void *ret = malloc(s);
    if (!ret) {
        throw std::bad_alloc();
    }

    memset(ret, 0xAA, s);
    return ret;
}

void* operator new[](size_t s) {
    void *ret = malloc(s);
    if (!ret) {
        throw std::bad_alloc();
    }

    memset(ret, 0xAA, s);
    return ret;
}



void operator delete( void *pvMem )
{
   size_t s =  malloc_usable_size(pvMem);
   memset(pvMem, 0xBB, s);
   free(pvMem);
}

void operator delete[](void *pvMem) {
    size_t s =  malloc_usable_size(pvMem);
    memset(pvMem, 0xBB, s);
    free(pvMem);
}
#endif

#endif
