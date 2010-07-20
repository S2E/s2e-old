extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg-llvm.h>
#include <exec-all.h>
#include <sysemu.h>
extern struct CPUX86State *env;
void QEMU_NORETURN raise_exception(int exception_index);
void QEMU_NORETURN raise_exception_err(int exception_index, int error_code);
extern const uint8_t parity_table[256];
extern const uint8_t rclw_table[32];
extern const uint8_t rclb_table[32];

uint64_t helper_set_cc_op_eflags(void);
uint64_t helper_do_interrupt(int intno, int is_int, int error_code,
                  target_ulong next_eip, int is_hw);
}

#include "S2EExecutor.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ExecutionTracers/TestCaseGenerator.h>
#include <s2e/S2EDeviceState.h>

#include <s2e/s2e_qemu.h>

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Target/TargetData.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/System/DynamicLibrary.h>

#include <klee/StatsTracker.h>
#include <klee/PTree.h>
#include <klee/Memory.h>
#include <klee/Searcher.h>
#include <klee/ExternalDispatcher.h>
#include <klee/UserSearcher.h>

#include <llvm/System/TimeValue.h>

#include <vector>

#include <sstream>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

//#define S2E_DEBUG_INSTRUCTIONS
//#define S2E_TRACE_EFLAGS

using namespace std;
using namespace llvm;
using namespace klee;

extern "C" {
    // XXX
    void* g_s2e_exec_ret_addr = 0;
}

namespace s2e {

/* Global array to hold tb function arguments */
volatile void* tb_function_args[3];

/* External dispatcher to convert QEMU longjmp's into C++ exceptions */
class S2EExternalDispatcher: public klee::ExternalDispatcher
{
protected:
    virtual bool runProtectedCall(llvm::Function *f, uint64_t *args);

public:
    S2EExternalDispatcher(ExecutionEngine* engine):
            ExternalDispatcher(engine) {}
};

extern "C" {

// FIXME: This is not reentrant.
static jmp_buf s2e_escapeCallJmpBuf;
static jmp_buf s2e_cpuExitJmpBuf;

#ifdef _WIN32
static void s2e_ext_sigsegv_handler(int signal)
{
}
#else
static void s2e_ext_sigsegv_handler(int signal, siginfo_t *info, void *context) {
  longjmp(s2e_escapeCallJmpBuf, 1);
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

  std::vector<GenericValue> gvArgs;
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

  if(setjmp(env->jmp_env)) {
      memcpy(env->jmp_env, s2e_cpuExitJmpBuf, sizeof(env->jmp_env));
      throw CpuExitException();
  } else {
      if (setjmp(s2e_escapeCallJmpBuf)) {
        res = false;
      } else {

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

S2EHandler::S2EHandler(S2E* s2e)
        : m_s2e(s2e)
{
}

std::ostream &S2EHandler::getInfoStream() const
{
    return m_s2e->getInfoStream();
}

std::string S2EHandler::getOutputFilename(const std::string &fileName)
{
    return m_s2e->getOutputFilename(fileName);
}

std::ostream *S2EHandler::openOutputFile(const std::string &fileName)
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
    assert(dynamic_cast<const S2EExecutionState*>(&state) != 0);
    const S2EExecutionState* s = static_cast<const S2EExecutionState*>(&state);
    m_s2e->getWarningsStream(s)
           << "Terminating state " << s->getID()
           << " with error message '" << (err ? err : "") << "'" << std::endl;

    s2e::plugins::TestCaseGenerator *tc =
            dynamic_cast<s2e::plugins::TestCaseGenerator*>(m_s2e->getPlugin("TestCaseGenerator"));
    if (tc) {
        tc->processTestCase(*s, err, suffix);
    }
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


S2EExecutor::S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLLVMContext,
                    const InterpreterOptions &opts,
                            InterpreterHandler *ie)
        : Executor(opts, ie, tcgLLVMContext->getExecutionEngine()),
          m_s2e(s2e), m_tcgLLVMContext(tcgLLVMContext),
          m_executeAlwaysKlee(false)
{
    delete externalDispatcher;
    externalDispatcher = new S2EExternalDispatcher(
            tcgLLVMContext->getExecutionEngine());

    LLVMContext& ctx = m_tcgLLVMContext->getLLVMContext();

    /* Add dummy TB function declaration */
    const PointerType* tbFunctionArgTy =
            PointerType::get(IntegerType::get(ctx, 64), 0);
    FunctionType* tbFunctionTy = FunctionType::get(
            IntegerType::get(ctx, TCG_TARGET_REG_BITS),
            vector<const Type*>(1, PointerType::get(
                    IntegerType::get(ctx, 64), 0)),
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
    CallInst::Create(tbFunction, tbFunctionArgs.begin(), tbFunctionArgs.end(),
            "tbFunctionCall", dummyMainBB);
    ReturnInst::Create(m_tcgLLVMContext->getLLVMContext(), dummyMainBB);

    // XXX: this will not work without creating JIT
    // XXX: how to get data layout without without ExecutionEngine ?
    m_tcgLLVMContext->getModule()->setDataLayout(
            m_tcgLLVMContext->getExecutionEngine()
                ->getTargetData()->getStringRepresentation());

    /* Define globally accessible functions */
#define __DEFINE_EXT_FUNCTION(name) \
    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) name);

    //__DEFINE_EXT_FUNCTION(raise_exception)
    //__DEFINE_EXT_FUNCTION(raise_exception_err)

    __DEFINE_EXT_FUNCTION(fprintf)
    __DEFINE_EXT_FUNCTION(fputc)
    __DEFINE_EXT_FUNCTION(fwrite)

    __DEFINE_EXT_FUNCTION(cpu_io_recompile)
    __DEFINE_EXT_FUNCTION(cpu_x86_handle_mmu_fault)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr0)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr3)
    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr4)
    __DEFINE_EXT_FUNCTION(cpu_x86_cpuid)
    __DEFINE_EXT_FUNCTION(cpu_get_tsc)
    __DEFINE_EXT_FUNCTION(cpu_get_apic_base)
    __DEFINE_EXT_FUNCTION(cpu_set_apic_base)
    __DEFINE_EXT_FUNCTION(cpu_get_apic_tpr)
    __DEFINE_EXT_FUNCTION(cpu_set_apic_tpr)
    __DEFINE_EXT_FUNCTION(cpu_smm_update)
    __DEFINE_EXT_FUNCTION(cpu_restore_state)
    __DEFINE_EXT_FUNCTION(cpu_abort)
    __DEFINE_EXT_FUNCTION(cpu_loop_exit)
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

    __DEFINE_EXT_FUNCTION(s2e_on_tlb_miss)
    __DEFINE_EXT_FUNCTION(s2e_on_page_fault)
    __DEFINE_EXT_FUNCTION(s2e_is_port_symbolic)

    /* XXX */
    __DEFINE_EXT_FUNCTION(ldub_phys)
    __DEFINE_EXT_FUNCTION(stb_phys)

    __DEFINE_EXT_FUNCTION(lduw_phys)
    __DEFINE_EXT_FUNCTION(stw_phys)

    __DEFINE_EXT_FUNCTION(ldl_phys)
    __DEFINE_EXT_FUNCTION(stl_phys)

    __DEFINE_EXT_FUNCTION(ldq_phys)
    __DEFINE_EXT_FUNCTION(stq_phys)

    /* Set module for the executor */
#if 1
    char* filename =  qemu_find_file(QEMU_FILE_TYPE_LIB, "op_helper.bc");
    assert(filename);
    ModuleOptions MOpts(vector<string>(1, filename),
            /* Optimize= */ false, /* CheckDivZero= */ false, m_tcgLLVMContext->getFunctionPassManager());

    qemu_free(filename);

#else
    ModuleOptions MOpts(vector<string>(),
            /* Optimize= */ false, /* CheckDivZero= */ false);
#endif



    setModule(m_tcgLLVMContext->getModule(), MOpts);

    m_tcgLLVMContext->initializeHelpers();

    m_dummyMain = kmodule->functionMap[dummyMain];

    Function* traceFunction =
            kmodule->module->getFunction("tcg_llvm_trace_memory_access");
    assert(traceFunction);
    addSpecialFunctionHandler(traceFunction, handlerTraceMemoryAccess);

    traceFunction = kmodule->module->getFunction("tcg_llvm_trace_port_access");
    assert(traceFunction);
    addSpecialFunctionHandler(traceFunction, handlerTracePortAccess);

    searcher = constructUserSearcher(*this);

    //setAllExternalWarnings(true);
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
                      /* isSharedConcrete = */ true);

    addExternalObject(*state, (void*) tb_function_args,
                      sizeof(tb_function_args), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);

#define __DEFINE_EXT_OBJECT(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      false, true, true);

#define __DEFINE_EXT_OBJECT_RO_SYMB(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      true, true, false);

    __DEFINE_EXT_OBJECT(env)
    __DEFINE_EXT_OBJECT(g_s2e)
    __DEFINE_EXT_OBJECT(g_s2e_state)
    __DEFINE_EXT_OBJECT(g_s2e_exec_ret_addr)
    __DEFINE_EXT_OBJECT(io_mem_read)
    __DEFINE_EXT_OBJECT(io_mem_write)
    __DEFINE_EXT_OBJECT(io_mem_opaque)
    __DEFINE_EXT_OBJECT(use_icount)
    __DEFINE_EXT_OBJECT(cpu_single_env)
    __DEFINE_EXT_OBJECT(loglevel)
    __DEFINE_EXT_OBJECT(logfile)
    __DEFINE_EXT_OBJECT_RO_SYMB(parity_table)
    __DEFINE_EXT_OBJECT_RO_SYMB(rclw_table)
    __DEFINE_EXT_OBJECT_RO_SYMB(rclb_table)


    m_s2e->getMessagesStream(state)
            << "Created initial state" << std::endl;

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
}

void S2EExecutor::registerCpu(S2EExecutionState *initialState,
                              CPUX86State *cpuEnv)
{
    std::cout << std::hex
              << "Adding CPU (addr = 0x" << cpuEnv
              << ", size = 0x" << sizeof(*cpuEnv) << ")"
              << std::dec << std::endl;

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

    const ObjectState *cpuSystemObject = initialState->addressSpace.findObject(initialState->m_cpuSystemState);
    const ObjectState *cpuRegistersObject = initialState->addressSpace.findObject(initialState->m_cpuRegistersState);

    initialState->m_cpuRegistersObject = initialState->addressSpace.getWriteable(initialState->m_cpuRegistersState, cpuRegistersObject);
    initialState->m_cpuSystemObject = initialState->addressSpace.getWriteable(initialState->m_cpuSystemState, cpuSystemObject);
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

    std::cout << std::hex
              << "Adding memory block (startAddr = 0x" << startAddress
              << ", size = 0x" << size << ", hostAddr = 0x" << hostAddress
              << ", isSharedConcrete=" << isSharedConcrete << ")" << std::dec << std::endl;

    for(uint64_t addr = hostAddress; addr < hostAddress+size;
                 addr += TARGET_PAGE_SIZE) {
        std::stringstream ss;

        ss << name << "_" << std::hex << (addr-hostAddress);

        MemoryObject *mo = addExternalObject(
                *initialState, (void*) addr, TARGET_PAGE_SIZE, false,
                /* isUserSpecified = */ true, isSharedConcrete);

        mo->setName(ss.str());

        if (isSharedConcrete && saveOnContextSwitch) {
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
}

bool S2EExecutor::isRamRegistered(S2EExecutionState *state,
                                      uint64_t hostAddress)
{
    ObjectPair op = state->fetchObjectStateMem(hostAddress, TARGET_PAGE_MASK);
    return op.first != NULL && op.first->isUserSpecified;
}

bool S2EExecutor::isRamSharedConcrete(S2EExecutionState *state,
                                      uint64_t hostAddress)
{
    ObjectPair op = state->fetchObjectStateMem(hostAddress, TARGET_PAGE_MASK);
    assert(op.first);
    return op.first->isSharedConcrete;
}

void S2EExecutor::readRamConcreteCheck(S2EExecutionState *state,
                    uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    assert(state->m_active && state->m_runningConcrete);
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->fetchObjectStateMem(page_addr, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                m_s2e->getMessagesStream(state)
                        << "Switching to KLEE executor at pc = "
                        << hexval(state->getPc()) << std::endl;
                state->m_startSymbexAtPC = state->getPc();
                // XXX: what about regs_to_env ?
                longjmp(env->jmp_env, 1);
            }
        }
    } else {
        /* Access spans multiple pages */
        uint64_t size1 = TARGET_PAGE_SIZE - page_offset;
        readRamConcreteCheck(state, hostAddress, buf, size1);
        readRamConcreteCheck(state, hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutor::readRamConcrete(S2EExecutionState *state,
                    uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    assert(state->m_active);
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->fetchObjectStateMem(page_addr, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = NULL;
        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                if(!wos) {
                    op.second = wos = state->fetchObjectStateMemWritable(
                                                    op.first, op.second);
                }
                buf[i] = toConstant(*state, wos->read8(page_offset+i),
                       "memory access from concrete code")->getZExtValue(8);
                wos->write8(page_offset+i, buf[i]);
            }
        }
    } else {
        /* Access spans multiple pages */
        uint64_t size1 = TARGET_PAGE_SIZE - page_offset;
        readRamConcrete(state, hostAddress, buf, size1);
        readRamConcrete(state, hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutor::writeRamConcrete(S2EExecutionState *state,
                       uint64_t hostAddress, const uint8_t* buf, uint64_t size)
{
    assert(state->m_active);
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->fetchObjectStateMem(page_addr, TARGET_PAGE_MASK);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        ObjectState* wos =
                state->fetchObjectStateMemWritable(op.first, op.second);
        for(uint64_t i=0; i<size; ++i) {
            wos->write8(page_offset+i, buf[i]);
        }

    } else {
        /* Access spans multiple pages */
        uint64_t size1 = TARGET_PAGE_SIZE - page_offset;
        writeRamConcrete(state, hostAddress, buf, size1);
        writeRamConcrete(state, hostAddress + size1, buf + size1, size - size1);
    }
}

void S2EExecutor::readRegisterConcrete(S2EExecutionState *state,
        CPUX86State *cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    assert(state->m_active);
    assert(((uint64_t)cpuState) == state->m_cpuRegistersState->address);
    assert(offset + size <= CPU_OFFSET(eip));

    if(state->m_runningConcrete) {
        memcpy(buf, ((uint8_t*)cpuState)+offset, size);
    } else {
        const MemoryObject* mo = state->m_cpuRegistersState;
        const ObjectState* os = state->addressSpace.findObject(mo);

        ObjectState* wos = 0;

        for(unsigned i = 0; i < size; ++i) {
            if(!os->readConcrete8(offset+i, buf+i)) {
                if(!wos){
                    wos = state->addressSpace.getWriteable(mo, os);
                    os = wos;
                }
                const char* reg;
                switch(offset) {
                    case 0x00: reg = "eax"; break;
                    case 0x04: reg = "ecx"; break;
                    case 0x08: reg = "edx"; break;
                    case 0x0c: reg = "ebx"; break;
                    case 0x10: reg = "esp"; break;
                    case 0x14: reg = "ebp"; break;
                    case 0x18: reg = "esi"; break;
                    case 0x1c: reg = "edi"; break;

                    case 0x20: reg = "cc_src"; break;
                    case 0x24: reg = "cc_dst"; break;
                    case 0x28: reg = "cc_op"; break;
                    case 0x3c: reg = "df"; break;

                    default: reg = "unknown"; break;
                }
                std::string reason = std::string("access to ") + reg +
                                     " register from QEMU helper";
                buf[i] = toConstant(*state, wos->read8(offset+i),
                                    reason.c_str())->getZExtValue(8);
                wos->write8(offset+i, buf[i]);
            }
        }
    }

#ifdef S2E_TRACE_EFLAGS
    if (offsetof(CPUState, cc_src) == offset) {
        m_s2e->getDebugStream() <<  std::hex << state->getPc() <<
                "read conc cc_src " << (*(uint32_t*)((uint8_t*)buf)) << std::endl;
    }
#endif
}

void S2EExecutor::writeRegisterConcrete(S2EExecutionState *state,
        CPUX86State *cpuState, unsigned offset, const uint8_t* buf, unsigned size)
{
    assert(state->m_active);
    assert(((uint64_t)cpuState) == state->m_cpuRegistersState->address);
    assert(offset + size <= CPU_OFFSET(eip));

    if(state->m_runningConcrete) {
        memcpy(((uint8_t*)cpuState)+offset, buf, size);
    } else {
        MemoryObject* mo = state->m_cpuRegistersState;
        const ObjectState* os = state->addressSpace.findObject(mo);

        ObjectState* wos = state->addressSpace.getWriteable(mo,os);

        for(unsigned i = 0; i < size; ++i) {
            wos->write8(offset+i, buf[i]);
        }
    }

#ifdef S2E_TRACE_EFLAGS
    if (offsetof(CPUState, cc_src) == offset) {
        m_s2e->getDebugStream() <<  std::hex << state->getPc() <<
                "write conc cc_src " << (*(uint32_t*)((uint8_t*)buf)) << std::endl;
    }
#endif

}

void S2EExecutor::switchToConcrete(S2EExecutionState *state)
{
    assert(!state->m_runningConcrete);

    /* Concretize any symbolic registers */
    const MemoryObject* mo = state->m_cpuRegistersState;
    const ObjectState* os = state->addressSpace.findObject(mo);

    assert(os);

    if(!os->isAllConcrete()) {
        /* The object contains symbolic values. We have to
           concretize it */
        /*
        ObjectState *wos;
        os = wos = state->addressSpace.getWriteable(mo,os);

        for(unsigned i = 0; i < wos->size; ++i) {
            ref<Expr> e = wos->read8(i);
            if(!isa<klee::ConstantExpr>(e)) {
                uint8_t ch = toConstant(*state, e,
                    "switching to concrete execution")->getZExtValue(8);
                wos->write8(i, ch);
            }
        }
        */
    }

    //assert(os->isAllConcrete());
    memcpy((void*) mo->address, os->getConcreteStore(true), mo->size);
    static_cast<S2EExecutionState*>(state)->m_runningConcrete = true;

    m_s2e->getMessagesStream(state)
            << "Switched to concrete execution at pc = "
            << hexval(state->getPc()) << std::endl;
}

void S2EExecutor::switchToSymbolic(S2EExecutionState *state)
{
    assert(state->m_runningConcrete);

    const MemoryObject* mo = state->m_cpuRegistersState;
    const ObjectState* os = state->addressSpace.findObject(mo);

    //assert(os && os->isAllConcrete());

    ObjectState *wos = state->addressSpace.getWriteable(mo, os);
    memcpy(wos->getConcreteStore(true), (void*) mo->address, mo->size);
    state->m_runningConcrete = false;

    m_s2e->getMessagesStream(state)
            << "Switched to symbolic execution at pc = "
            << hexval(state->getPc()) << std::endl;
}

void S2EExecutor::jumpToSymbolic(S2EExecutionState *state)
{
    assert(state->isRunningConcrete());

    state->m_startSymbexAtPC = state->getPc();
    // XXX: what about regs_to_env ?
    longjmp(env->jmp_env, 1);
}


void S2EExecutor::copyOutConcretes(ExecutionState &state)
{
    return;
}

bool S2EExecutor::copyInConcretes(klee::ExecutionState &state)
{
    return true;
}

void S2EExecutor::doStateSwitch(S2EExecutionState* oldState,
                                S2EExecutionState* newState)
{
    assert(oldState->m_active && !newState->m_active);
    assert(!newState->m_runningConcrete);

    cpu_disable_ticks();

    m_s2e->getMessagesStream(oldState)
            << "Switching from state " << oldState->getID()
            << " to state " << newState->getID() << std::endl;

    if(oldState->m_runningConcrete) {
        switchToSymbolic(oldState);
    }

    oldState->m_active = false;

    copyInConcretes(*oldState);
    oldState->getDeviceState()->saveDeviceState();
    *oldState->m_timersState = timers_state;

    uint64_t totalCopied = 0;
    uint64_t objectsCopied = 0;
    foreach(MemoryObject* mo, m_saveOnContextSwitch) {
        const ObjectState *oldOS = oldState->fetchObjectState(mo, TARGET_PAGE_MASK);
        const ObjectState *newOS = newState->fetchObjectState(mo, TARGET_PAGE_MASK);
        ObjectState *oldWOS = oldState->fetchObjectStateWritable(mo, oldOS);

        uint8_t *oldStore = oldWOS->getConcreteStore();
        const uint8_t *newStore = newOS->getConcreteStore();

        assert(oldStore);
        assert(newStore);

        memcpy(oldStore, (uint8_t*) mo->address, mo->size);
        memcpy((uint8_t*) mo->address, newStore, mo->size);
        totalCopied += mo->size;
        objectsCopied++;
    }

    s2e_debug_print("Copying %d (count=%d)\n", totalCopied, objectsCopied);
    timers_state = *newState->m_timersState;
    newState->getDeviceState()->restoreDeviceState();
    copyOutConcretes(*newState);
    newState->m_active = true;

    cpu_enable_ticks();
    //m_s2e->getCorePlugin()->onStateSwitch.emit(oldState, newState);
}

S2EExecutionState* S2EExecutor::selectNextState(S2EExecutionState *state)
{
    updateStates(state);
    if(states.empty()) {
        m_s2e->getWarningsStream() << "All states were terminated" << std::endl;
        exit(0);
    }

    ExecutionState& newKleeState = searcher->selectState();
    assert(dynamic_cast<S2EExecutionState*>(&newKleeState));

    S2EExecutionState* newState =
            static_cast<S2EExecutionState*  >(&newKleeState);
    assert(states.find(newState) != states.end());

    if(newState != state) {
        doStateSwitch(state, newState);
    }
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
    //int64_t start_clock = get_clock();
    cpu_disable_ticks();

    KInstruction *ki = state->pc;

#ifdef S2E_DEBUG_INSTRUCTIONS
    m_s2e->getDebugStream(state) << "executing "
              << ki->inst->getParent()->getParent()->getNameStr()
              << ": " << *ki->inst << std::endl;
#endif

    stepInstruction(*state);

    bool shouldExitCpu = false;
    try {

        executeInstruction(*state, ki);

#ifdef S2E_TRACE_EFLAGS
        ref<Expr> efl = state->readCpuRegister(offsetof(CPUState, cc_src), klee::Expr::Int32);
        m_s2e->getDebugStream() << std::hex << state->getPc() << "  CC_SRC " << efl << std::endl;
#endif

    } catch(CpuExitException&) {
        // Instruction that forks should never be interrupted
        // (and, consequently, restarted)
        assert(addedStates.empty());
        shouldExitCpu = true;
    }

    /* TODO: timers */
    /* TODO: MaxMemory */

    updateStates(state);

    // assume that symbex is 50 times slower
    cpu_enable_ticks();

    //int64_t inst_clock = get_clock() - start_clock;
    //cpu_adjust_clock(- inst_clock*(1-0.02));

    if(shouldExitCpu)
        throw CpuExitException();
}

void S2EExecutor::finalizeState(S2EExecutionState *state)
{
    if(state->stack.size() == 1) {
        //No need for finalization
        return;
    }

    m_s2e->getDebugStream() << "Finalizing state " << std::dec << state->getID() << std::endl;
    foreach(const StackFrame& fr, state->stack) {
        m_s2e->getDebugStream() << fr.kf->function->getNameStr() << std::endl;
    }

    /* Information for GETPC() macro */
    g_s2e_exec_ret_addr = state->getTb()->tc_ptr;

    while(state->stack.size() != 1) {
        executeOneInstruction(state);
    }

    state->prevPC = 0;
    state->pc = m_dummyMain->instructions;

    copyOutConcretes(*state);

}

uintptr_t S2EExecutor::executeTranslationBlockKlee(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    tb_function_args[0] = env;
    tb_function_args[1] = 0;
    tb_function_args[2] = 0;



    //XXX: hack to clean interrupted translation blocks (that forked)
    //cleanupTranslationBlock(state, tb);

    assert(state->m_active && !state->m_runningConcrete);
    assert(state->stack.size() == 1);
    assert(state->pc == m_dummyMain->instructions);

    /* Update state */
    if (!copyInConcretes(*state)) {
        std::cerr << "external modified read-only object" << std::endl;
        exit(1);
    }

    /* loop until TB chain is not broken */
    do {
        /* Make sure to init tb_next value */
        tcg_llvm_runtime.goto_tb = 0xff;

        /* Generate LLVM code if nesessary */
        if(!tb->llvm_function) {
            cpu_gen_llvm(env, tb);
            assert(tb->llvm_function);
        }

        /* Prepare function execution */
        prepareFunctionExecution(state,
                tb->llvm_function, std::vector<ref<Expr> >(1,
                    Expr::createPointer((uint64_t) tb_function_args)));

        /* Information for GETPC() macro */
        g_s2e_exec_ret_addr = tb->tc_ptr;

        /* Execute */
        while(state->stack.size() != 1) {
            executeOneInstruction(state);

            /* Check to goto_tb request */
            if(tcg_llvm_runtime.goto_tb != 0xff) {
                assert(tcg_llvm_runtime.goto_tb < 2);

                /* The next should be atomic with respect to signals */
                /* XXX: what else should we block ? */
#ifdef _WIN32
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
                    g_s2e_exec_ret_addr = tb->tc_ptr;

                    /* assert that blocking works */
#ifdef _WIN32
                    if (old_tb->s2e_tb_next[tcg_llvm_runtime.goto_tb] != tb) {
                        env->s2e_current_tb = old_tb;
                        g_s2e_exec_ret_addr = old_tb->tc_ptr;
                    }else {
                        cleanupTranslationBlock(state, tb);
                        break;
                    }
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
#else
                sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
            }
        }

        state->prevPC = 0;
        state->pc = m_dummyMain->instructions;

    } while(tcg_llvm_runtime.goto_tb != 0xff);

    g_s2e_exec_ret_addr = 0;

    /* Get return value */
    ref<Expr> resExpr =
            getDestCell(*state, state->pc).value;
    assert(isa<klee::ConstantExpr>(resExpr));

    copyOutConcretes(*state);

    return cast<klee::ConstantExpr>(resExpr)->getZExtValue();
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

#ifdef _WIN32

#warning Implement signal enabling/disabling...

typedef int sigset_t;

static void s2e_disable_signals(sigset_t *oldset)
{

}

static void s2e_enable_signals(sigset_t *oldset)
{

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
        if(depth > 1 || (smask & tb1->reg_rmask) || (smask & tb1->reg_wmask)) {
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
    assert(state->isActive());


    const ObjectState* os = state->m_cpuRegistersObject;

    state->setTbInstructionCount(tb->icount);

    bool executeKlee = m_executeAlwaysKlee;
    if(state->m_symbexEnabled) {
        if(state->m_startSymbexAtPC != (uint64_t) -1) {
            executeKlee |= (state->getPc() == state->m_startSymbexAtPC);
            state->m_startSymbexAtPC = (uint64_t) -1;
        }
        if(!executeKlee) {
            //XXX: This should be fixed to make sure that helpers do not read/write corrupted data
            //because they think that execution is concrete while it should be symbolic (see issue #30).
#if 0
            /* We can not execute TB natively if it reads any symbolic regs */
            if(!os->isAllConcrete()) {
                uint64_t smask = state->getSymbolicRegistersMask();
#if 1
                if((smask & tb->reg_rmask) || (smask & tb->reg_wmask)) {
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
#endif
            }
#else
            executeKlee |= !os->isAllConcrete();
#endif
        }
    }

    if(executeKlee) {
        if(state->m_runningConcrete)
            switchToSymbolic(state);
        return executeTranslationBlockKlee(state, tb);

    } else {
        g_s2e_exec_ret_addr = 0;
        if(!state->m_runningConcrete)
            switchToConcrete(state);
        return tcg_qemu_tb_exec(tb->tc_ptr);
    }
}

void S2EExecutor::cleanupTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    assert(state->m_active);

    g_s2e_exec_ret_addr = 0;

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
    assert(!state->prevPC);
    assert(state->stack.size() == 1);

    /* Update state */
    if (!copyInConcretes(*state)) {
        std::cerr << "external modified read-only object" << std::endl;
        exit(1);
    }

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

    copyOutConcretes(*state);

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

void S2EExecutor::enableSymbolicExecution(S2EExecutionState *state)
{
    state->m_symbexEnabled = true;
    m_s2e->getMessagesStream(state) << "Enabled symbex"
            << " at pc = 0x" << (void*) state->getPc() << std::endl;
}

void S2EExecutor::disableSymbolicExecution(S2EExecutionState *state)
{
    state->m_symbexEnabled = false;
    m_s2e->getMessagesStream(state) << "Disabled symbex"
            << " at pc = 0x" << (void*) state->getPc() << std::endl;
}

void S2EExecutor::deleteState(klee::ExecutionState *state)
{
    assert(dynamic_cast<S2EExecutionState*>(state));
    m_deletedStates.push_back(static_cast<S2EExecutionState*>(state));
}

void S2EExecutor::doStateFork(S2EExecutionState *originalState,
                 const vector<S2EExecutionState*>& newStates,
                 const vector<ref<Expr> >& newConditions)
{
    assert(originalState->m_active && !originalState->m_runningConcrete);

    std::ostream& out = m_s2e->getMessagesStream(originalState);
    out << "Forking state " << originalState->getID()
            << " at pc = " << hexval(originalState->getPc())
        << " into states:" << std::endl;


    for(unsigned i = 0; i < newStates.size(); ++i) {
        S2EExecutionState* newState = newStates[i];

        out << "    state " << newState->getID() << " with condition "
            << *newConditions[i].get() << std::endl;

        if(newState != originalState) {
            newState->getDeviceState()->saveDeviceState();

            foreach(MemoryObject* mo, m_saveOnContextSwitch) {
                const ObjectState *os = newState->fetchObjectState(mo, TARGET_PAGE_MASK);
                ObjectState *wos = newState->fetchObjectStateWritable(mo, os);
                uint8_t *store = wos->getConcreteStore();

                assert(store);
                memcpy(store, (uint8_t*) mo->address, mo->size);
            }

            newState->m_active = false;
        }

        const ObjectState *cpuSystemObject = newState->addressSpace.findObject(newState->m_cpuSystemState);
        const ObjectState *cpuRegistersObject = newState->addressSpace.findObject(newState->m_cpuRegistersState);

        newState->m_cpuRegistersObject = newState->addressSpace.getWriteable(newState->m_cpuRegistersState, cpuRegistersObject);
        newState->m_cpuSystemObject = newState->addressSpace.getWriteable(newState->m_cpuSystemState, cpuSystemObject);
    }

    m_s2e->getDebugStream() << "Stack frame at fork:" << std::endl;
    foreach(const StackFrame& fr, originalState->stack) {
        m_s2e->getDebugStream() << fr.kf->function->getNameStr() << std::endl;
    }

    m_s2e->getCorePlugin()->onStateFork.emit(originalState,
                                             newStates, newConditions);
}

S2EExecutor::StatePair S2EExecutor::fork(ExecutionState &current,
                            ref<Expr> condition, bool isInternal)
{
    static int count=0;
    assert(dynamic_cast<S2EExecutionState*>(&current));
    assert(!static_cast<S2EExecutionState*>(&current)->m_runningConcrete);

    StatePair res = Executor::fork(current, condition, isInternal);
    if(res.first && res.second) {
        if (++count == 10) {
//        exit(-1);
        }

        assert(dynamic_cast<S2EExecutionState*>(res.first));
        assert(dynamic_cast<S2EExecutionState*>(res.second));

        std::vector<S2EExecutionState*> newStates(2);
        std::vector<ref<Expr> > newConditions(2);

        newStates[0] = static_cast<S2EExecutionState*>(res.second);
        newStates[1] = static_cast<S2EExecutionState*>(res.first);

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

    if(newStates.size() > 1)
        doStateFork(static_cast<S2EExecutionState*>(&state),
                       newStates, newConditions);
}

void S2EExecutor::invalidateCache(klee::ExecutionState &state, const klee::MemoryObject *mo)
{
    S2EExecutionState *s = dynamic_cast<S2EExecutionState*>(&state);
    assert(s);
    if (mo->size == TARGET_PAGE_SIZE) {
        s->m_memCache.invalidate(mo->address);
    }
}

void S2EExecutor::terminateState(ExecutionState &state)
{
    S2EExecutionState *s = dynamic_cast<S2EExecutionState*>(&state);
    assert(s);

    Executor::terminateState(state);

    s->writeCpuState(CPU_OFFSET(exception_index), EXCP_INTERRUPT, 8*sizeof(int));
    throw StateTerminatedException();
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

int s2e_is_ram_registered(S2E *s2e, S2EExecutionState *state,
                               uint64_t host_address)
{
    return s2e->getExecutor()->isRamRegistered(state, host_address);
}

int s2e_is_ram_shared_concrete(S2E *s2e, S2EExecutionState *state,
                               uint64_t host_address)
{
    return s2e->getExecutor()->isRamSharedConcrete(state, host_address);
}

void s2e_read_ram_concrete_check(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    assert(state->isRunningConcrete());
    if(state->isSymbolicExecutionEnabled())
        s2e->getExecutor()->readRamConcreteCheck(state, host_address, buf, size);
    else
        s2e->getExecutor()->readRamConcrete(state, host_address, buf, size);
}

void s2e_read_ram_concrete(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    s2e->getExecutor()->readRamConcrete(state, host_address, buf, size);
}

void s2e_write_ram_concrete(S2E *s2e, S2EExecutionState *state,
                    uint64_t host_address, const uint8_t* buf, uint64_t size)
{
    s2e->getExecutor()->writeRamConcrete(state, host_address, buf, size);
}

void s2e_read_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    /** XXX: use cpuState */
    s2e->getExecutor()->readRegisterConcrete(state, cpuState, offset, buf, size);
}

void s2e_write_register_concrete(S2E* s2e, S2EExecutionState* state,
        CPUX86State* cpuState, unsigned offset, uint8_t* buf, unsigned size)
{
    /** XXX: use cpuState */
    s2e->getExecutor()->writeRegisterConcrete(state, cpuState, offset, buf, size);
}

S2EExecutionState* s2e_select_next_state(S2E* s2e, S2EExecutionState* state)
{
    return s2e->getExecutor()->selectNextState(state);
}

uintptr_t s2e_qemu_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    /*s2e->getDebugStream() << "icount=" << std::dec << s2e_get_executed_instructions()
            << " pc=0x" << std::hex << state->getPc() << std::dec
            << std::endl;   */
    try {
        return s2e->getExecutor()->executeTranslationBlock(state, tb);
    } catch(s2e::CpuExitException&) {
        s2e->getExecutor()->updateStates(state);
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        s2e->getExecutor()->updateStates(state);
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_qemu_finalize_state(S2E *s2e, S2EExecutionState* state)
{
    try {
        s2e->getExecutor()->finalizeState(state);
    } catch(s2e::CpuExitException&) {
        s2e->getExecutor()->updateStates(state);
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        s2e->getExecutor()->updateStates(state);
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_qemu_cleanup_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    return s2e->getExecutor()->cleanupTranslationBlock(state, tb);
}

void s2e_set_cc_op_eflags(struct S2E* s2e,
                        struct S2EExecutionState* state)
{
    // Check wether any of cc_op, cc_src, cc_dst or cc_tmp are symbolic
    if(state->getSymbolicRegistersMask() & (0xf<<1)) {
        // call set_cc_op_eflags only if cc_op is symbolic or cc_op != CC_OP_EFLAGS
        uint32_t cc_op = 0;
        bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(cc_op),
                                                 &cc_op, sizeof(cc_op));
        if(!ok || cc_op != CC_OP_EFLAGS) {
            try {
                s2e->getExecutor()->executeFunction(state,
                                                "helper_set_cc_op_eflags");
            } catch(s2e::CpuExitException&) {
                s2e->getExecutor()->updateStates(state);
                longjmp(env->jmp_env, 1);
            } catch(s2e::StateTerminatedException&) {
                s2e->getExecutor()->updateStates(state);
                longjmp(env->jmp_env_s2e, 1);
            }
        }
    } else {
        helper_set_cc_op_eflags();
    }

}

void s2e_do_interrupt(struct S2E* s2e, struct S2EExecutionState* state,
                      int intno, int is_int, int error_code,
                      uint64_t next_eip, int is_hw)
{
    if(state->isRunningConcrete()) {
        helper_do_interrupt(intno, is_int, error_code, next_eip, is_hw);
    } else {
        std::vector<klee::ref<klee::Expr> > args(5);
        args[0] = klee::ConstantExpr::create(intno, sizeof(int)*8);
        args[1] = klee::ConstantExpr::create(is_int, sizeof(int)*8);
        args[2] = klee::ConstantExpr::create(error_code, sizeof(int)*8);
        args[3] = klee::ConstantExpr::create(next_eip, sizeof(target_ulong)*8);
        args[4] = klee::ConstantExpr::create(is_hw, sizeof(int)*8);
        try {
            s2e->getExecutor()->executeFunction(state,
                                                "helper_do_interrupt", args);
        } catch(s2e::CpuExitException&) {
            s2e->getExecutor()->updateStates(state);
            longjmp(env->jmp_env, 1);
        } catch(s2e::StateTerminatedException&) {
            s2e->getExecutor()->updateStates(state);
            longjmp(env->jmp_env_s2e, 1);
        }
    }
}


/**
 *  Checks whether we are trying to access an I/O port that returns a symbolic value.
 */
void s2e_switch_to_symbolic(S2E *s2e, S2EExecutionState *state)
{
    s2e->getExecutor()->jumpToSymbolic(state);
}
