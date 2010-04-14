extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg-llvm.h>
}

#include "S2EExecutor.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/s2e_qemu.h>

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Target/TargetData.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <klee/StatsTracker.h>
#include <klee/PTree.h>

#include <vector>

using namespace std;
using namespace llvm;
using namespace klee;

extern "C" {
    // XXX
    extern volatile void* saved_AREGs[3];
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

    /* Create initial execution state */
    m_currentState = new S2EExecutionState(kmodule->functionMap[dummyMain]);

    if(pathWriter)
        m_currentState->pathOS = pathWriter->open();
    if(symPathWriter)
        m_currentState->symPathOS = symPathWriter->open();

    if(statsTracker)
        statsTracker->framePushed(*m_currentState, 0);

    states.insert(m_currentState);

    processTree = new PTree(m_currentState);
    m_currentState->ptreeNode = processTree->root;

    /* Externally accessible global vars */
    addExternalObject(*m_currentState, &tcg_llvm_runtime,
                      sizeof(tcg_llvm_runtime), false);
    addExternalObject(*m_currentState, saved_AREGs,
                      sizeof(saved_AREGs), false);

    /* Make CPUState instances accessible: generated code uses them as globals */
    for(CPUState *env = first_cpu; env != NULL; env = env->next_cpu) {
        std::cout << "Adding CPU addr = " << env
                  << " size = " << sizeof(*env) << std::endl;
        addExternalObject(*m_currentState, env, sizeof(*env), false);
    }

    /* Map physical memory */
    int i = 0;
#define S2E_RAM_BLOCK_SIZE (TARGET_PAGE_SIZE*16)
    std::cout << "Going to add " << (last_ram_offset/S2E_RAM_BLOCK_SIZE)
              << " ram blocks" << std::endl;
    for(ram_addr_t addr = 0; addr < last_ram_offset; addr += S2E_RAM_BLOCK_SIZE) {
        addExternalObject(*m_currentState, qemu_get_ram_ptr(addr),
                min<ram_addr_t>(S2E_RAM_BLOCK_SIZE, last_ram_offset-addr), false);
        ++i;
    }
    std::cout << "Added " << i << " RAM blocks" << std::endl;

    initializeGlobals(*m_currentState);
    bindModuleConstants();
}

S2EExecutor::~S2EExecutor()
{
    if(statsTracker)
        statsTracker->done();
}

void S2EExecutor::updateCurrentState(CPUState* cpuState, uint64_t pc)
{
    assert(m_currentState);

    m_currentState->cpuState = cpuState;
    m_currentState->cpuPC = pc;

    // TODO: update KFunction and instruction iterator
}

void S2EExecutor::updateCurrentState(CPUState* cpuState)
{
    assert(m_currentState);

    m_currentState->cpuState = cpuState;

    // TODO: update KFunction and instruction iterator
}

inline uintptr_t S2EExecutor::executeTranslationBlock(
        TranslationBlock *tb, void* volatile* saved_AREGs)
{
    tcg_llvm_runtime.last_tb = tb;
#if 0
    return ((uintptr_t (*)(void* volatile*)) tb->llvm_tc_ptr)(saved_AREGs);
#else
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

    /* Update state */
    updateCurrentState((CPUX86State*) saved_AREGs[0], tb->pc);

    assert(m_currentState->stack.size() == 1);
    assert(m_currentState->pc == m_dummyMain->instructions);

    /* Emulate call to a TB function */
    m_currentState->prevPC = m_currentState->pc;

    m_currentState->pushFrame(m_currentState->pc, kf);
    m_currentState->pc = kf->instructions;

    if(statsTracker)
        statsTracker->framePushed(*m_currentState,
            &m_currentState->stack[m_currentState->stack.size()-2]);

    /* Pass argument */
    bindArgument(kf, 0, *m_currentState,
                 Expr::createPointer((uint64_t) saved_AREGs));

    if (!m_currentState->addressSpace.copyInConcretes()) {
        std::cerr << "external modified read-only object" << std::endl;
        exit(1);
    }

    /* Execute */
    while(m_currentState->stack.size() != 1) {
        KInstruction *ki = m_currentState->pc;
        stepInstruction(*m_currentState);
        executeInstruction(*m_currentState, ki);

        /* TODO: timers */
        /* TODO: MaxMemory */

        updateStates(m_currentState);
        if(states.find(m_currentState) == states.end()) {
            std::cerr << "The state was killed !" << std::endl;
            std::cerr << "Last executed instruction was:" << std::endl;
            ki->inst->dump();
            exit(1);
        }
    }

    m_currentState->prevPC = 0;
    m_currentState->pc = m_dummyMain->instructions;

    ref<Expr> resExpr =
            getDestCell(*m_currentState, m_currentState->pc).value;
    assert(isa<klee::ConstantExpr>(resExpr));

    m_currentState->addressSpace.copyOutConcretes();

    return cast<klee::ConstantExpr>(resExpr)->getZExtValue();
#endif
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

uintptr_t s2e_qemu_tb_exec(S2E* s2e, struct TranslationBlock* tb,
                           void* volatile* saved_AREGs)
{
    return s2e->getExecutor()->executeTranslationBlock(tb, saved_AREGs);
}
