extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg-llvm.h>
}

#include "S2EExecutor.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Utils.h>

#include <s2e/s2e_qemu.h>

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Target/TargetData.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <klee/StatsTracker.h>
#include <klee/PTree.h>
#include <klee/Memory.h>

#include <vector>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using namespace std;
using namespace llvm;
using namespace klee;

extern "C" {
    // XXX
    extern volatile void* saved_AREGs[3];
    void* g_s2e_exec_ret_addr = 0;
}

namespace s2e {

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
    m_s2e->getWarningsStream() << "Terminating state '" << (&state)
           << "with error message '" << (err ? err : "") << "'" << std::endl;
}

S2EExecutor::S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLLVMContext,
                    const InterpreterOptions &opts,
                            InterpreterHandler *ie)
        : Executor(opts, ie, tcgLLVMContext->getExecutionEngine()),
          m_s2e(s2e), m_tcgLLVMContext(tcgLLVMContext)
{
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

    /* Set module for the executor */
    ModuleOptions MOpts(KLEE_LIBRARY_DIR,
                    /* Optimize= */ false, /* CheckDivZero= */ false);
    setModule(m_tcgLLVMContext->getModule(), MOpts);

    m_dummyMain = kmodule->functionMap[dummyMain];
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

    if(pathWriter)
        state->pathOS = pathWriter->open();
    if(symPathWriter)
        state->symPathOS = symPathWriter->open();

    if(statsTracker)
        statsTracker->framePushed(*state, 0);

    states.insert(state);

    processTree = new PTree(state);
    state->ptreeNode = processTree->root;

    /* Externally accessible global vars */
    /* XXX move away */
    addExternalObject(*state, &tcg_llvm_runtime,
                      sizeof(tcg_llvm_runtime), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);

    /* XXX: is this really required ? */
    addExternalObject(*state, saved_AREGs,
                      sizeof(saved_AREGs), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);

#if 0
    /* Make CPUState instances accessible: generated code uses them as globals */
    for(CPUState *env = first_cpu; env != NULL; env = env->next_cpu) {
        std::cout << "Adding KLEE CPU (addr = " << env
                  << " size = " << sizeof(*env) << ")" << std::endl;
        addExternalObject(*state, env, sizeof(*env), false);
    }

    /* Map physical memory */
    std::cout << "Populating KLEE memory..." << std::endl;

    for(ram_addr_t addr = 0; addr < last_ram_offset; addr += TARGET_PAGE_SIZE) {
        int pd = cpu_get_physical_page_desc(addr) & ~TARGET_PAGE_MASK;
        if(pd > IO_MEM_ROM && !(pd & IO_MEM_ROMD))
            continue;

        void* p = qemu_get_ram_ptr(addr);
        MemoryObject* mo = addExternalObject(
                *state, p, TARGET_PAGE_SIZE, false);
        mo->isUserSpecified = true; // XXX hack

        /* XXX */
        munmap(p, TARGET_PAGE_SIZE);
    }
    std::cout << "...done" << std::endl;
#endif

    return state;
}

void S2EExecutor::initializeExecution(S2EExecutionState* state)
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

    state->cpuState = first_cpu;
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
    addExternalObject(*initialState, cpuEnv, sizeof(*cpuEnv), false);
    if(!initialState->cpuState) initialState->cpuState = cpuEnv;
}

void S2EExecutor::registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete)
{
    assert(startAddress == (uint64_t) -1 ||
           (startAddress & ~TARGET_PAGE_MASK) == 0);
    assert((size & ~TARGET_PAGE_MASK) == 0);
    assert((hostAddress & ~TARGET_PAGE_MASK) == 0);

    std::cout << std::hex
              << "Adding memory block (startAddr = 0x" << startAddress
              << ", size = 0x" << size << ", hostAddr = 0x" << hostAddress
              << ")" << std::dec << std::endl;

    for(uint64_t addr = hostAddress; addr < hostAddress+size;
                 addr += TARGET_PAGE_SIZE) {
        addExternalObject(
                *initialState, (void*) addr, TARGET_PAGE_SIZE, false,
                /* isUserSpecified = */ true, isSharedConcrete);
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
    ObjectPair op = state->addressSpace.findObject(hostAddress);
    return op.first != NULL && op.first->isUserSpecified;
}

bool S2EExecutor::isRamSharedConcrete(S2EExecutionState *state,
                                      uint64_t hostAddress)
{
    ObjectPair op = state->addressSpace.findObject(hostAddress);
    assert(op.first);
    return op.first->isSharedConcrete;
}

void S2EExecutor::readRamConcrete(S2EExecutionState *state,
                    uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->addressSpace.findObject(page_addr);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        ObjectState *wos = NULL;
        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                if(!wos) {
                    op.second = wos = state->addressSpace.getWriteable(
                                                    op.first, op.second);
                }
                buf[i] = toConstant(*state, wos->read8(page_offset+i),
                               "concrete memory access")->getZExtValue(8);
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
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->addressSpace.findObject(page_addr);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        ObjectState* wos =
                state->addressSpace.getWriteable(op.first, op.second);
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

inline void S2EExecutor::executeTBFunction(
        S2EExecutionState *state,
        TranslationBlock *tb,
        void *volatile* saved_AREGs)
{
}

inline uintptr_t S2EExecutor::executeTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb,
        void* volatile* saved_AREGs)
{
#if 0
    return ((uintptr_t (*)(void* volatile*)) tb->llvm_tc_ptr)(saved_AREGs);
#else

    assert(state->stack.size() == 1);
    assert(state->pc == m_dummyMain->instructions);

    /* Update state */
    state->cpuState = (CPUX86State*) saved_AREGs[0];

    if (!state->addressSpace.copyInConcretes()) {
        std::cerr << "external modified read-only object" << std::endl;
        exit(1);
    }

    /* loop until TB chain is not broken */
    do {
        /* Make sure to init tb_next value */
        tcg_llvm_runtime.goto_tb = 0xff;

        /* Create a KLEE shadow structs */
        KFunction *kf;
        typeof(kmodule->functionMap.begin()) it =
                kmodule->functionMap.find(tb->llvm_function);
        if(it != kmodule->functionMap.end()) {
            kf = it->second;
        } else {
            unsigned cIndex = kmodule->constants.size();
            kf = kmodule->updateModuleWithFunction(tb->llvm_function);

            for(unsigned i = 0; i < kf->numInstructions; ++i)
                bindInstructionConstants(kf->instructions[i]);

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
        bindArgument(kf, 0, *state,
                     Expr::createPointer((uint64_t) saved_AREGs));

        /* Information for GETPC() macro */
        g_s2e_exec_ret_addr = tb->tc_ptr;

        /* Execute */
        while(state->stack.size() != 1) {
            KInstruction *ki = state->pc;
            stepInstruction(*state);
            executeInstruction(*state, ki);

            /* TODO: timers */
            /* TODO: MaxMemory */

            updateStates(state);
            if(!removedStates.empty() && removedStates.find(state) !=
                                         removedStates.end()) {
                std::cerr << "The current state was killed inside KLEE !" << std::endl;
                std::cerr << "Last executed instruction was:" << std::endl;
                ki->inst->dump();
                exit(1);
            }

            /* Check to goto_tb request */
            if(tcg_llvm_runtime.goto_tb != 0xff) {
                assert(tcg_llvm_runtime.goto_tb < 2);

                /* The next should be atomic with respect to signals */
                /* XXX: what else should we block ? */
                sigset_t set, oldset;
                sigfillset(&set);
                sigprocmask(SIG_BLOCK, &set, &oldset);

                TranslationBlock* next_tb =
                        tb->s2e_tb_next[tcg_llvm_runtime.goto_tb];

                if(next_tb) {
#ifndef NDEBUG
                    TranslationBlock* old_tb = tb;
#endif

                    assert(state->stack.size() == 2);
                    state->popFrame();

                    tb = next_tb;
                    state->cpuState->s2e_current_tb = tb;
                    g_s2e_exec_ret_addr = tb->tc_ptr;

                    /* assert that blocking works */
                    assert(old_tb->s2e_tb_next[tcg_llvm_runtime.goto_tb] == tb);

                    sigprocmask(SIG_SETMASK, &oldset, NULL);
                    break;
                }

                /* the block was unchained by signal handler */
                tcg_llvm_runtime.goto_tb = 0xff;
                sigprocmask(SIG_SETMASK, &oldset, NULL);
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

    state->addressSpace.copyOutConcretes();

    return cast<klee::ConstantExpr>(resExpr)->getZExtValue();
#endif
}

inline void S2EExecutor::cleanupTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    g_s2e_exec_ret_addr = 0;

    while(state->stack.size() != 1)
        state->popFrame();

    state->prevPC = 0;
    state->pc = m_dummyMain->instructions;
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

S2EExecutionState* s2e_create_initial_state(S2E *s2e)
{
    return s2e->getExecutor()->createInitialState();
}

void s2e_initialize_execution(S2E *s2e, S2EExecutionState *initial_state)
{
    s2e->getExecutor()->initializeExecution(initial_state);
}

void s2e_register_cpu(S2E *s2e, S2EExecutionState *initial_state,
                      CPUX86State *cpu_env)
{
    s2e->getExecutor()->registerCpu(initial_state, cpu_env);
}

void s2e_register_ram(S2E* s2e, S2EExecutionState *initial_state,
        uint64_t start_address, uint64_t size,
        uint64_t host_address, int is_shared_concrete)
{
    s2e->getExecutor()->registerRam(initial_state,
        start_address, size, host_address, is_shared_concrete);
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

uintptr_t s2e_qemu_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb,
                           void* volatile* saved_AREGs)
{
    return s2e->getExecutor()->executeTranslationBlock(state, tb, saved_AREGs);
}

void s2e_qemu_cleanup_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    return s2e->getExecutor()->cleanupTranslationBlock(state, tb);
}
