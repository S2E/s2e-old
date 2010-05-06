extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg-llvm.h>
#include <exec-all.h>
extern struct CPUX86State *env;
}

#include "S2EExecutor.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EDeviceState.h>

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
#include <klee/Searcher.h>

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
    void* g_s2e_exec_ret_addr = 0;
}

namespace s2e {

/* Global array to hold tb function arguments */
volatile void* tb_function_args[3];

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

    searcher = new RandomSearcher();
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

    m_s2e->getMessagesStream()
            << "Created initial state 0x" << hexval(state) << std::endl;

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

    /* Add the rest of the structure as concrete-only area */
    initialState->m_cpuSystemState =
        addExternalObject(*initialState,
                      ((uint8_t*)cpuEnv) + offsetof(CPUX86State, eip),
                      sizeof(CPUX86State) - offsetof(CPUX86State, eip),
                      /* isReadOnly = */ false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);
    m_saveOnContextSwitch.push_back(initialState->m_cpuSystemState);
}

void S2EExecutor::registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete,
                        bool saveOnContextSwitch)
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
        MemoryObject *mo = addExternalObject(
                *initialState, (void*) addr, TARGET_PAGE_SIZE, false,
                /* isUserSpecified = */ true, isSharedConcrete);

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

void S2EExecutor::readRamConcreteCheck(S2EExecutionState *state,
                    uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
    assert(state->m_active && state->m_runningConcrete);
    uint64_t page_offset = hostAddress & ~TARGET_PAGE_MASK;
    if(page_offset + size <= TARGET_PAGE_SIZE) {
        /* Single-page access */

        uint64_t page_addr = hostAddress & TARGET_PAGE_MASK;
        ObjectPair op = state->addressSpace.findObject(page_addr);

        assert(op.first && op.first->isUserSpecified &&
               op.first->size == TARGET_PAGE_SIZE);

        for(uint64_t i=0; i<size; ++i) {
            if(!op.second->readConcrete8(page_offset+i, buf+i)) {
                m_s2e->getMessagesStream()
                        << "Switching to KLEE executor at pc = "
                        << hexval(state->getPc()) << std::endl;
                execute_s2e_at = state->getPc();
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
    assert(state->m_active);
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

void S2EExecutor::copyOutConcretes(ExecutionState &state) {

    assert(dynamic_cast<S2EExecutionState*>(&state));
    assert(!static_cast<S2EExecutionState*>(&state)->m_runningConcrete);
    static_cast<S2EExecutionState*>(&state)->m_runningConcrete = true;

    /* Concretize any symbolic values */
    for (MemoryMap::iterator
            it = state.addressSpace.nonUserSpecifiedObjects.begin(),
            ie = state.addressSpace.nonUserSpecifiedObjects.end();
            it != ie; ++it) {
        const MemoryObject* mo = it->first;
        const ObjectState* os = it->second;

        assert(!mo->isUserSpecified);

        if(!os->isAllConcrete()) {
            /* The object contains symbolic values. We have to
               concretize it */
            ObjectState *wos = state.addressSpace.getWriteable(mo, os);
            for(unsigned i = 0; i < wos->size; ++i) {
                ref<Expr> e = wos->read8(i);
                if(!isa<klee::ConstantExpr>(e)) {
                    uint8_t ch = toConstant(state, e,
                            "calling external helper")->getZExtValue(8);
                    wos->write8(i, ch);
                }
            }
        }
    }
    Executor::copyOutConcretes(state);
}

bool S2EExecutor::copyInConcretes(klee::ExecutionState &state)
{
    assert(dynamic_cast<S2EExecutionState*>(&state));
    assert(static_cast<S2EExecutionState*>(&state)->m_runningConcrete);
    static_cast<S2EExecutionState*>(&state)->m_runningConcrete = false;
    return Executor::copyInConcretes(state);
}

void S2EExecutor::doStateSwitch(S2EExecutionState* oldState,
                                S2EExecutionState* newState)
{
    m_s2e->getMessagesStream()
            << "Switching from state " << hexval(oldState)
            << " to state " << hexval(newState) << std::endl;

    assert(oldState->m_active && !newState->m_active);
    oldState->m_active = false;

    copyInConcretes(*oldState);
    oldState->getDeviceState()->saveDeviceState();

    foreach(MemoryObject* mo, m_saveOnContextSwitch) {
        const ObjectState *oldOS = oldState->addressSpace.findObject(mo);
        const ObjectState *newOS = newState->addressSpace.findObject(mo);
        ObjectState *oldWOS = oldState->addressSpace.getWriteable(mo, oldOS);

        uint8_t *oldStore = oldWOS->getConcreteStore();
        uint8_t *newStore = newOS->getConcreteStore();

        assert(oldStore);
        assert(newStore);

        memcpy(oldStore, (uint8_t*) mo->address, mo->size);
        memcpy((uint8_t*) mo->address, newStore, mo->size);
    }

    newState->getDeviceState()->restoreDeviceState();
    copyOutConcretes(*newState);
    newState->m_active = true;

    //m_s2e->getCorePlugin()->onStateSwitch.emit(oldState, newState);
}

S2EExecutionState* S2EExecutor::selectNextState(S2EExecutionState *state)
{
    ExecutionState& newKleeState = searcher->selectState();
    assert(dynamic_cast<S2EExecutionState*>(&newKleeState));

    S2EExecutionState* newState =
            static_cast<S2EExecutionState*>(&newKleeState);
    if(newState != state) {
        doStateSwitch(state, newState);
    }
    return newState;
}

uintptr_t S2EExecutor::executeTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    tb_function_args[0] = env;
    tb_function_args[1] = 0;
    tb_function_args[2] = 0;

    assert(state->m_active);
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
                     Expr::createPointer((uint64_t) tb_function_args));

        /* Information for GETPC() macro */
        g_s2e_exec_ret_addr = tb->tc_ptr;

        /* Execute */
        while(state->stack.size() != 1) {
            KInstruction *ki = state->pc;
            stepInstruction(*state);
            executeInstruction(*state, ki);

            /* TODO: timers */
            /* TODO: MaxMemory */

            if(!removedStates.empty() && removedStates.find(state) !=
                                         removedStates.end()) {
                std::cerr << "The current state was killed inside KLEE !" << std::endl;
                std::cerr << "Last executed instruction was:" << std::endl;
                ki->inst->dump();
                exit(1);
            }

            updateStates(state);

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
                    assert(old_tb->s2e_tb_next[tcg_llvm_runtime.goto_tb] == tb);

#ifdef _WIN32      
#else
                    sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif

                    break;
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

    if(!state->m_runningConcrete) {
        /* If we was interupted while symbexing, we can be resumed
           for concrete execution */
        copyOutConcretes(*state);
    }
}

void S2EExecutor::enableSymbolicExecution(S2EExecutionState *state)
{
    state->m_symbexEnabled = true;
    m_s2e->getMessagesStream() << "Enabled symbex for state 0x" << state
            << " at pc = 0x" << (void*) state->getPc() << std::endl;
}

void S2EExecutor::disableSymbolicExecution(S2EExecutionState *state)
{
    state->m_symbexEnabled = false;
    m_s2e->getMessagesStream() << "Disabled symbex for state 0x" << state
            << " at pc = 0x" << (void*) state->getPc() << std::endl;
}

void S2EExecutor::doStateFork(S2EExecutionState *originalState,
                 const vector<S2EExecutionState*>& newStates,
                 const vector<ref<Expr> >& newConditions)
{   
    assert(originalState->m_active);

    m_s2e->getMessagesStream()
        << "Forking state " << hexval(originalState)
        << " into states:" << std::endl;

    for(unsigned i = 0; i < newStates.size(); ++i) {
        S2EExecutionState* newState = newStates[i];

        m_s2e->getMessagesStream()
            << "    " << hexval(newState) << " with condition "
            << *newConditions[i].get() << std::endl;

        if(newState != originalState) {
            newState->getDeviceState()->saveDeviceState();
           
            foreach(MemoryObject* mo, m_saveOnContextSwitch) {
                const ObjectState *os = newState->addressSpace.findObject(mo);
                ObjectState *wos = newState->addressSpace.getWriteable(mo, os);
                uint8_t *store = wos->getConcreteStore();

                assert(store);
                memcpy(store, (uint8_t*) mo->address, mo->size);
            }

            newState->m_active = false;
            if(newState->m_runningConcrete) {
                /* Forking while running concretely is unlikely, but still
                   might happen, for example, due to instrumentation */
                copyInConcretes(*newState);
            }
        }
    }

    m_s2e->getCorePlugin()->onStateFork.emit(originalState,
                                             newStates, newConditions);
}

S2EExecutor::StatePair S2EExecutor::fork(ExecutionState &current,
                            ref<Expr> condition, bool isInternal)
{
    StatePair res = Executor::fork(current, condition, isInternal);
    if(res.first && res.second) {
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
        uint64_t host_address, int is_shared_concrete,
        int save_on_context_switch)
{
    s2e->getExecutor()->registerRam(initial_state,
        start_address, size, host_address, is_shared_concrete,
        save_on_context_switch);
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
    if(!execute_llvm && state->isSymbolicExecutionEnabled())
        s2e->getExecutor()->readRamConcreteCheck(state, host_address, buf, size);
    else
        s2e->getExecutor()->readRamConcrete(state, host_address, buf, size);
    //s2e->getCorePlugin()->onMemoryAccess.emit(
    //                    state, host_address, buf, size, false);
}

void s2e_read_ram_concrete(S2E *s2e, S2EExecutionState *state,
                        uint64_t host_address, uint8_t* buf, uint64_t size)
{
    s2e->getExecutor()->readRamConcrete(state, host_address, buf, size);
    //s2e->getCorePlugin()->onMemoryAccess.emit(
    //                        state, host_address, buf, size, false);
}

void s2e_write_ram_concrete(S2E *s2e, S2EExecutionState *state,
                    uint64_t host_address, const uint8_t* buf, uint64_t size)
{
    //s2e->getCorePlugin()->onMemoryAccess.emit(
    //                        state, host_address, buf, size, true);
    s2e->getExecutor()->writeRamConcrete(state, host_address, buf, size);
}

S2EExecutionState* s2e_select_next_state(S2E* s2e, S2EExecutionState* state)
{
    return s2e->getExecutor()->selectNextState(state);
}

uintptr_t s2e_qemu_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    return s2e->getExecutor()->executeTranslationBlock(state, tb);
}

void s2e_qemu_cleanup_tb_exec(S2E* s2e, S2EExecutionState* state,
                           struct TranslationBlock* tb)
{
    return s2e->getExecutor()->cleanupTranslationBlock(state, tb);
}

