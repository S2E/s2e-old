//===-- Executor.cpp ------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Common.h"

#include "klee/Executor.h"
 
#include "klee/Context.h"
#include "klee/CoreStats.h"
#include "klee/ExternalDispatcher.h"
#include "ImpliedValue.h"
#include "klee/Memory.h"
#include "MemoryManager.h"
#include "klee/PTree.h"
#include "klee/Searcher.h"
#include "SeedInfo.h"
#include "SpecialFunctionHandler.h"
#include "klee/StatsTracker.h"
#include "TimingSolver.h"
#include "klee/UserSearcher.h"
#include "klee/SolverStats.h"
#include "klee/BitfieldSimplifier.h"

#include "klee/ExecutionState.h"
#include "klee/Expr.h"
#include "klee/Interpreter.h"
#include "klee/TimerStatIncrementer.h"
#include "klee/util/Assignment.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"
#include "klee/Config/config.h"
#include "klee/Internal/ADT/KTest.h"
#include "klee/Internal/ADT/RNG.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Support/FloatEvaluation.h"
#include "klee/Internal/System/Time.h"

#include "llvm/Attributes.h"
#include "llvm/BasicBlock.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/LLVMContext.h"
#endif
#include "llvm/Module.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Process.h"
#include "llvm/DataLayout.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdlib.h>

#ifndef __MINGW32__
#include <sys/mman.h>
#endif

#include <errno.h>
#include <cxxabi.h>
#include <inttypes.h>

using namespace llvm;
using namespace klee;

namespace {
  cl::opt<bool>
  DumpStatesOnHalt("dump-states-on-halt",
                   cl::init(true));
 
  cl::opt<bool>
  NoPreferCex("no-prefer-cex",
              cl::init(false));
 
  cl::opt<bool>
  UseAsmAddresses("use-asm-addresses",
                  cl::init(false));
 
  cl::opt<bool>
  RandomizeFork("randomize-fork",
                cl::init(false));
 
  cl::opt<bool>
  AllowExternalSymCalls("allow-external-sym-calls",
                        cl::init(false));

  cl::opt<bool>
  DebugPrintInstructions("debug-print-instructions", 
                         cl::desc("Print instructions during execution."),
                         cl::init(false));

  cl::opt<bool>
  DebugCheckForImpliedValues("debug-check-for-implied-values");


  cl::opt<bool>
  SimplifySymIndices("simplify-sym-indices",
                     cl::init(true));

  cl::opt<unsigned>
  MaxSymArraySize("max-sym-array-size",
                  cl::init(0));

  cl::opt<bool>
  DebugValidateSolver("debug-validate-solver",
		      cl::init(false));

  cl::opt<bool>
  SuppressExternalWarnings("suppress-external-warnings", cl::init(true));

  cl::opt<bool>
  OnlyOutputStatesCoveringNew("only-output-states-covering-new",
                              cl::init(false));

  cl::opt<bool>
  AlwaysOutputSeeds("always-output-seeds",
                              cl::init(false));

  cl::opt<bool>
  UseFastCexSolver("use-fast-cex-solver",
                   cl::init(false));

  cl::opt<bool>
  UseIndependentSolver("use-independent-solver",
                       cl::init(true),
		       cl::desc("Use constraint independence"));

  cl::opt<bool>
  EmitAllErrors("emit-all-errors",
                cl::init(false),
                cl::desc("Generate tests cases for all errors "
                         "(default=one per (error,instruction) pair)"));

  //The counter example cache may have bad interactions with
  //concolic mode. Disabled by default.
  cl::opt<bool>
  UseCexCache("use-cex-cache",
              cl::init(false),
	      cl::desc("Use counterexample caching"));

  cl::opt<bool>
  UseQueryLog("use-query-log",
              cl::init(false));

  cl::opt<bool>
  UseQueryPCLog("use-query-pc-log",
                cl::init(false));
  
  cl::opt<bool>
  UseSTPQueryPCLog("use-stp-query-pc-log",
                   cl::init(false));

  cl::opt<bool>
  NoExternals("no-externals", 
           cl::desc("Do not allow external functin calls"));

  cl::opt<bool>
  UseCache("use-cache",
           cl::init(true),
	   cl::desc("Use validity caching"));

  cl::opt<bool>
  OnlyReplaySeeds("only-replay-seeds", 
                  cl::desc("Discard states that do not have a seed."));
 
  cl::opt<bool>
  OnlySeed("only-seed", 
           cl::desc("Stop execution after seeding is done without doing regular search."));
 
  cl::opt<bool>
  AllowSeedExtension("allow-seed-extension", 
                     cl::desc("Allow extra (unbound) values to become symbolic during seeding."));
 
  cl::opt<bool>
  ZeroSeedExtension("zero-seed-extension");
 
  cl::opt<bool>
  AllowSeedTruncation("allow-seed-truncation", 
                      cl::desc("Allow smaller buffers than in seeds."));
 
  cl::opt<bool>
  NamedSeedMatching("named-seed-matching",
                    cl::desc("Use names to match symbolic objects to inputs."));

  cl::opt<double>
  MaxStaticForkPct("max-static-fork-pct", cl::init(1.));
  cl::opt<double>
  MaxStaticSolvePct("max-static-solve-pct", cl::init(1.));
  cl::opt<double>
  MaxStaticCPForkPct("max-static-cpfork-pct", cl::init(1.));
  cl::opt<double>
  MaxStaticCPSolvePct("max-static-cpsolve-pct", cl::init(1.));

  cl::opt<double>
  MaxInstructionTime("max-instruction-time",
                     cl::desc("Only allow a single instruction to take this much time (default=0 (off))"),
                     cl::init(0));
  
  cl::opt<double>
  SeedTime("seed-time",
           cl::desc("Amount of time to dedicate to seeds, before normal search (default=0 (off))"),
           cl::init(0));
  
  cl::opt<double>
  MaxSTPTime("max-stp-time",
             cl::desc("Maximum amount of time for a single query (default=120s)"),
             cl::init(120.0));
  
  cl::opt<unsigned int>
  StopAfterNInstructions("stop-after-n-instructions",
                         cl::desc("Stop execution after specified number of instructions (0=off)"),
                         cl::init(0));
  
  cl::opt<unsigned>
  MaxForks("max-forks",
           cl::desc("Only fork this many times (-1=off)"),
           cl::init(~0u));
  
  cl::opt<unsigned>
  MaxDepth("max-depth",
           cl::desc("Only allow this many symbolic branches (0=off)"),
           cl::init(0));
  
  cl::opt<unsigned>
  MaxMemory("max-memory",
            cl::desc("Refuse to fork when more above this about of memory (in MB, 0=off)"),
            cl::init(0));

  cl::opt<bool>
  MaxMemoryInhibit("max-memory-inhibit",
            cl::desc("Inhibit forking at memory cap (vs. random terminate)"),
            cl::init(true));

  cl::opt<bool>
  UseForkedSTP("use-forked-stp", 
                 cl::desc("Run STP in forked process"),  cl::init(false));

  /*
  cl::opt<bool>
  IgnoreAlwaysConcrete("ignore-always-concrete",
            cl::desc("Do not add constraints when writing to always concrete memory"),
            cl::init(false));
  */

  cl::opt<bool>
  ValidateSimplifier("validate-expr-simplifier",
            cl::desc("Checks that the simplification algorithm produced correct expressions"),
            cl::init(false));

  cl::opt<bool>
  EnableSpeculativeForking("enable-speculative-forking",
            cl::desc("Enable speculative forking for concolic execution"),
            cl::init(true));
}

//S2E: we want these to be accessible in S2E executor
cl::opt<bool>
UseExprSimplifier("use-expr-simplifier",
          cl::desc("Apply expression simplifier for new expressions"),
          cl::init(true));



unsigned Executor::getMaxMemory() { return MaxMemory; }
bool Executor::getMaxMemoryInhibit() { return MaxMemoryInhibit; }

static void *theMMap = 0;
static unsigned theMMapSize = 0;

namespace klee {
  RNG theRNG;
}

Solver *constructSolverChain(STPSolver *stpSolver,
                             std::string queryLogPath,
                             std::string stpQueryLogPath,
                             std::string queryPCLogPath,
                             std::string stpQueryPCLogPath) {
  Solver *solver = stpSolver;

  if (UseSTPQueryPCLog)
    solver = createPCLoggingSolver(solver, 
                                   stpQueryLogPath);

  if (UseFastCexSolver)
    solver = createFastCexSolver(solver);

  if (UseCexCache)
    solver = createCexCachingSolver(solver);

  if (UseCache)
    solver = createCachingSolver(solver);

  if (UseIndependentSolver)
    solver = createIndependentSolver(solver);

  if (DebugValidateSolver)
    solver = createValidatingSolver(solver, stpSolver);

  if (UseQueryPCLog)
    solver = createPCLoggingSolver(solver, 
                                   queryPCLogPath);
  
  return solver;
}

void Executor::initializeSolver()
{
    if (this->solver) {
        delete this->solver;
    }

    STPSolver *stpSolver = new STPSolver(UseForkedSTP);
    Solver *solver =
      constructSolverChain(stpSolver,
                           interpreterHandler->getOutputFilename("queries.qlog"),
                           interpreterHandler->getOutputFilename("stp-queries.qlog"),
                           interpreterHandler->getOutputFilename("queries.pc"),
                           interpreterHandler->getOutputFilename("stp-queries.pc"));

    this->solver = new TimingSolver(solver, stpSolver);
}

Executor::Executor(const InterpreterOptions &opts,
                   InterpreterHandler *ih, ExecutionEngine *engine)
  : Interpreter(opts),
    kmodule(0),
    interpreterHandler(ih),
    searcher(0),
    externalDispatcher(new ExternalDispatcher(engine)),
    statsTracker(0),
    pathWriter(0),
    symPathWriter(0),
    specialFunctionHandler(0),
    processTree(0),
    concolicMode(false),
    replayOut(0),
    replayPath(0),    
    usingSeeds(0),
    atMemoryLimit(false),
    inhibitForking(false),
    haltExecution(false),
    ivcEnabled(false) {

  if(MaxSTPTime == 0) {
      stpTimeout = MaxInstructionTime;
  } else if(MaxInstructionTime == 0) {
      stpTimeout = MaxSTPTime;
  } else {
    stpTimeout = std::min(MaxSTPTime,MaxInstructionTime);
  }

  this->solver = NULL;
  initializeSolver();

  memory = new MemoryManager();

  //Mandatory for AddressSpace
  exprSimplifier = new BitfieldSimplifier;
}


const Module *Executor::setModule(llvm::Module *module,
                                  const ModuleOptions &opts,
                                  bool createStatsTracker) {
  assert(!kmodule && module && "can only register one module"); // XXX gross
  
  kmodule = new KModule(module);

  // Initialize the context.
  DataLayout *TD = kmodule->targetData;
  Context::initialize(TD->isLittleEndian(),
                      (Expr::Width) TD->getPointerSizeInBits());

  specialFunctionHandler = new SpecialFunctionHandler(*this);

  specialFunctionHandler->prepare();
  kmodule->prepare(opts, interpreterHandler);
  specialFunctionHandler->bind();

  if (createStatsTracker && StatsTracker::useStatistics()) {
    statsTracker = 
      new StatsTracker(*this,
                       interpreterHandler->getOutputFilename("assembly.ll"),
                       userSearcherRequiresMD2U());
    statsTracker->writeHeaders();
  }
  
  return module;
}

Executor::~Executor() {
  delete memory;
  delete externalDispatcher;
  if (processTree)
    delete processTree;
  if (specialFunctionHandler)
    delete specialFunctionHandler;
  if (statsTracker)
    delete statsTracker;
  delete solver;
  delete kmodule;
}

/***/

ref<Expr> Executor::simplifyExpr(const ExecutionState &s, ref<Expr> e)
{
    if(UseExprSimplifier) {
        //*klee_message_stream << "Simpl hits:" << exprSimplifier->m_cacheHits
        //                     << " misses:" << exprSimplifier->m_cacheMisses << "\n";

        ref<Expr> simplified = exprSimplifier->simplify(e);

        if (ValidateSimplifier) {
            bool isEqual;

            if (concolicMode) {
                ref<Expr> originalConcrete = s.concolics.evaluate(e);
                ref<Expr> simplifiedConcrete = s.concolics.evaluate(simplified);
                isEqual = originalConcrete == simplifiedConcrete;
            } else {
                ref<Expr> eq = EqExpr::create(simplified, e);
                bool result = solver->mustBeTrue(s, eq, isEqual);
                assert(result);
            }

            if(!isEqual) {
                llvm::errs() << "Error in expression simplifier:" << '\n';
                e->dump();
                llvm::errs() << "!=" << '\n';
                simplified->dump();
                assert(false);
            }
        }

        return simplified;

    }else {
        return e;
    }
}

void Executor::initializeGlobalObject(ExecutionState &state, ObjectState *os,
                                      Constant *c, 
                                      unsigned offset) {
  DataLayout *targetData = kmodule->targetData;
  if (ConstantVector *cp = dyn_cast<ConstantVector>(c)) {
    unsigned elementSize =
      targetData->getTypeStoreSize(cp->getType()->getElementType());
    for (unsigned i=0, e=cp->getNumOperands(); i != e; ++i)
      initializeGlobalObject(state, os, cp->getOperand(i), 
			     offset + i*elementSize);
  } else if (isa<ConstantAggregateZero>(c)) {
    unsigned i, size = targetData->getTypeStoreSize(c->getType());
    for (i=0; i<size; i++)
      os->write8(offset+i, (uint8_t) 0);
  } else if (ConstantArray *ca = dyn_cast<ConstantArray>(c)) {
    unsigned elementSize =
      targetData->getTypeStoreSize(ca->getType()->getElementType());
    for (unsigned i=0, e=ca->getNumOperands(); i != e; ++i)
      initializeGlobalObject(state, os, ca->getOperand(i), 
			     offset + i*elementSize);
  } else if (ConstantStruct *cs = dyn_cast<ConstantStruct>(c)) {
    const StructLayout *sl =
      targetData->getStructLayout(cast<StructType>(cs->getType()));
    for (unsigned i=0, e=cs->getNumOperands(); i != e; ++i)
      initializeGlobalObject(state, os, cs->getOperand(i), 
			     offset + sl->getElementOffset(i));
  } else if (const ConstantDataSequential *cds =
               dyn_cast<ConstantDataSequential>(c)) {
    unsigned elementSize =
      targetData->getTypeStoreSize(cds->getElementType());
    for (unsigned i=0, e=cds->getNumElements(); i != e; ++i)
      initializeGlobalObject(state, os, cds->getElementAsConstant(i),
                             offset + i*elementSize);
  } else {
    unsigned StoreBits = targetData->getTypeStoreSizeInBits(c->getType());
    ref<ConstantExpr> C = evalConstant(c);

    // Extend the constant if necessary;
    assert(StoreBits >= C->getWidth() && "Invalid store size!");
    if (StoreBits > C->getWidth())
      C = C->ZExt(StoreBits);

    os->write(offset, C);
  }
}

MemoryObject * Executor::addExternalObject(ExecutionState &state, 
                                           void *addr, unsigned size, 
                                           bool isReadOnly,
                                           bool isUserSpecified,
                                           bool isSharedConcrete,
                                           bool isValueIgnored) {
  MemoryObject *mo = memory->allocateFixed((uint64_t) addr,
                                           size, 0);
  mo->isUserSpecified = isUserSpecified;
  mo->isSharedConcrete = isSharedConcrete;
  mo->isValueIgnored = isValueIgnored;
  ObjectState *os = bindObjectInState(state, mo, false);
  if(!isSharedConcrete) {
    memcpy(os->getConcreteStore(), addr, size);
    /*
    for(unsigned i = 0; i < size; i++)
      os->write8(i, ((uint8_t*)addr)[i]);
    */
  }
  if(isReadOnly)
    os->setReadOnly(true);  
  return mo;
}

void Executor::initializeGlobals(ExecutionState &state) {
  Module *m = kmodule->module;

  if (m->getModuleInlineAsm() != "")
    klee_warning("executable has module level assembly (ignoring)");

  assert(m->lib_begin() == m->lib_end() &&
         "XXX do not support dependent libraries");

  // represent function globals using the address of the actual llvm function
  // object. given that we use malloc to allocate memory in states this also
  // ensures that we won't conflict. we don't need to allocate a memory object
  // since reading/writing via a function pointer is unsupported anyway.
  for (Module::iterator i = m->begin(), ie = m->end(); i != ie; ++i) {
    Function *f = i;
    ref<ConstantExpr> addr(0);

    // If the symbol has external weak linkage then it is implicitly
    // not defined in this module; if it isn't resolvable then it
    // should be null.
    if (f->hasExternalWeakLinkage() && 
        !externalDispatcher->resolveSymbol(f->getName())) {
      addr = Expr::createPointer(0);
    } else {
      addr = Expr::createPointer((uintptr_t) (void*) f);
      legalFunctions.insert((uint64_t) (uintptr_t) (void*) f);
    }
    
    globalAddresses.insert(std::make_pair(f, addr));
  }

  // Disabled, we don't want to promote use of live externals.
#ifdef HAVE_CTYPE_EXTERNALS
#ifndef WINDOWS
#ifndef DARWIN
  /* From /usr/include/errno.h: it [errno] is a per-thread variable. */
  int *errno_addr = __errno_location();
  addExternalObject(state, (void *)errno_addr, sizeof *errno_addr, false);

  /* from /usr/include/ctype.h:
       These point into arrays of 384, so they can be indexed by any `unsigned
       char' value [0,255]; by EOF (-1); or by any `signed char' value
       [-128,-1).  ISO C requires that the ctype functions work for `unsigned */
  const uint16_t **addr = __ctype_b_loc();
  addExternalObject(state, (void *)(*addr-128), 
                    384 * sizeof **addr, true);
  addExternalObject(state, addr, sizeof(*addr), true);
    
  const int32_t **lower_addr = __ctype_tolower_loc();
  addExternalObject(state, (void *)(*lower_addr-128), 
                    384 * sizeof **lower_addr, true);
  addExternalObject(state, lower_addr, sizeof(*lower_addr), true);
  
  const int32_t **upper_addr = __ctype_toupper_loc();
  addExternalObject(state, (void *)(*upper_addr-128), 
                    384 * sizeof **upper_addr, true);
  addExternalObject(state, upper_addr, sizeof(*upper_addr), true);
#endif
#endif
#endif

  // allocate and initialize globals, done in two passes since we may
  // need address of a global in order to initialize some other one.

  // allocate memory objects for all globals
  for (Module::const_global_iterator i = m->global_begin(),
         e = m->global_end();
       i != e; ++i) {
    std::map<std::string, void*>::iterator po =
            predefinedSymbols.find(i->getName());
    if(po != predefinedSymbols.end()) {
      // This object was externally defined
      globalAddresses.insert(std::make_pair(i,
            ConstantExpr::create((uint64_t) po->second, sizeof(void*)*8)));

    } else if(i->isDeclaration()) {
      // FIXME: We have no general way of handling unknown external
      // symbols. If we really cared about making external stuff work
      // better we could support user definition, or use the EXE style
      // hack where we check the object file information.

      Type *ty = i->getType()->getElementType();
      uint64_t size = kmodule->targetData->getTypeStoreSize(ty);

      // XXX - DWD - hardcode some things until we decide how to fix.
#ifndef WINDOWS
      if (i->getName() == "_ZTVN10__cxxabiv117__class_type_infoE") {
        size = 0x2C;
      } else if (i->getName() == "_ZTVN10__cxxabiv120__si_class_type_infoE") {
        size = 0x2C;
      } else if (i->getName() == "_ZTVN10__cxxabiv121__vmi_class_type_infoE") {
        size = 0x2C;
      }
#endif

      if (size == 0) {
        llvm::errs() << "Unable to find size for global variable: " 
                     << i->getName() 
                     << " (use will result in out of bounds access)\n";
      }

      MemoryObject *mo = memory->allocate(size, false, true, i);
      ObjectState *os = bindObjectInState(state, mo, false);
      globalObjects.insert(std::make_pair(i, mo));
      globalAddresses.insert(std::make_pair(i, mo->getBaseExpr()));

      // Program already running = object already initialized.  Read
      // concrete value and write it to our copy.
      if (size) {
        void *addr;
        addr = externalDispatcher->resolveSymbol(i->getName());

        if (!addr)
          klee_error("unable to load symbol(%s) while initializing globals.", 
                     i->getName().data());

        for (unsigned offset=0; offset<mo->size; offset++)
          os->write8(offset, ((unsigned char*)addr)[offset]);
      }
    } else {
      Type *ty = i->getType()->getElementType();
      uint64_t size = kmodule->targetData->getTypeStoreSize(ty);
      MemoryObject *mo = 0;

      if (UseAsmAddresses && i->getName()[0]=='\01') {
        char *end;
        uint64_t address = ::strtoll(i->getName().str().c_str()+1, &end, 0);

        if (end && *end == '\0') {
          klee_message("NOTE: allocated global at asm specified address: %#08"
                       PRIx64 " (%" PRIu64 " bytes)",
                       address, size);
          mo = memory->allocateFixed(address, size, &*i);
          mo->isUserSpecified = true; // XXX hack;
        }
      }

      if (!mo)
        mo = memory->allocate(size, false, true, &*i);
      assert(mo && "out of memory");
      ObjectState *os = bindObjectInState(state, mo, false);
      globalObjects.insert(std::make_pair(i, mo));
      globalAddresses.insert(std::make_pair(i, mo->getBaseExpr()));

      if (!i->hasInitializer())
          os->initializeToRandom();
    }
  }
  
  // link aliases to their definitions (if bound)
  for (Module::alias_iterator i = m->alias_begin(), ie = m->alias_end(); 
       i != ie; ++i) {
    // Map the alias to its aliasee's address. This works because we have
    // addresses for everything, even undefined functions. 
    globalAddresses.insert(std::make_pair(i, evalConstant(i->getAliasee())));
  }

  // once all objects are allocated, do the actual initialization
  for (Module::global_iterator i = m->global_begin(), e = m->global_end(); i != e; ++i) {
    if (predefinedSymbols.find(i->getName()) != predefinedSymbols.end()) {
        continue;
    }

    if (i->hasInitializer()) {
      assert(globalObjects.find(i) != globalObjects.end());
      const MemoryObject *mo = globalObjects.find(i)->second;
      const ObjectState *os = state.addressSpace.findObject(mo);
      assert(os);
      ObjectState *wos = state.addressSpace.getWriteable(mo, os);
      initializeGlobalObject(state, wos, i->getInitializer(), 0);
      // if(i->isConstant()) os->setReadOnly(true);
    }
  }
}

void Executor::notifyBranch(ExecutionState &state)
{
    //Should not get here
    assert(false && "Must go through S2E");
}

void Executor::branch(ExecutionState &state, 
                      const std::vector< ref<Expr> > &conditions,
                      std::vector<ExecutionState*> &result) {
  TimerStatIncrementer timer(stats::forkTime);
  unsigned N = conditions.size();
  assert(N);

  notifyBranch(state);

  stats::forks += N-1;

  // XXX do proper balance or keep random?
  result.push_back(&state);
  for (unsigned i=1; i<N; ++i) {
    ExecutionState *es = result[theRNG.getInt32() % i];
    ExecutionState *ns = es->branch();
    addedStates.insert(ns);
    result.push_back(ns);
    es->ptreeNode->data = 0;
    std::pair<PTree::Node*,PTree::Node*> res = 
      processTree->split(es->ptreeNode, ns, es);
    ns->ptreeNode = res.first;
    es->ptreeNode = res.second;
  }

  // If necessary redistribute seeds to match conditions, killing
  // states if necessary due to OnlyReplaySeeds (inefficient but
  // simple).
  
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it = 
    seedMap.find(&state);
  if (it != seedMap.end()) {
    std::vector<SeedInfo> seeds = it->second;
    seedMap.erase(it);

    // Assume each seed only satisfies one condition (necessarily true
    // when conditions are mutually exclusive and their conjunction is
    // a tautology).
    for (std::vector<SeedInfo>::iterator siit = seeds.begin(), 
           siie = seeds.end(); siit != siie; ++siit) {
      unsigned i;
      for (i=0; i<N; ++i) {
        ref<ConstantExpr> res;
        bool success = 
          solver->getValue(state, siit->assignment.evaluate(
                                        simplifyExpr(state, conditions[i])),
                           res);
        assert(success && "FIXME: Unhandled solver failure");
        (void) success;
        if (res->isTrue())
          break;
      }
      
      // If we didn't find a satisfying condition randomly pick one
      // (the seed will be patched).
      if (i==N)
        i = theRNG.getInt32() % N;

      seedMap[result[i]].push_back(*siit);
    }

    if (OnlyReplaySeeds) {
      for (unsigned i=0; i<N; ++i) {
        if (!seedMap.count(result[i])) {
          terminateState(*result[i]);
          result[i] = NULL;
        }
      } 
    }
  }

  for (unsigned i=0; i<N; ++i)
    if (result[i])
      addConstraint(*result[i], conditions[i]);
}

Executor::StatePair
Executor::concolicFork(ExecutionState &current, ref<Expr> condition, bool isInternal) {
    condition = simplifyExpr(current, condition);

    //If we are passed a constant, no need to do anything,
    //revert to normal fork (which won't branch).
    if (dyn_cast<ConstantExpr>(condition)) {
        return Executor::fork(current, condition, isInternal);
    }

    //*klee_message_stream << "Concolic fork for expression " << condition << "\n";

    //The current state is guaranteed to be consistent with whatever
    //assignment is stored in the concolics variable.
    assert(!current.speculative);

    //Evaluate the expression using the current variable assignment.
    ref<Expr> evalResult = current.concolics.evaluate(condition);
    ConstantExpr *ce = dyn_cast<ConstantExpr>(evalResult);

    assert(ce && "Could not evaluate the expression to a constant.");

    if (current.forkDisabled) {
       if (ce->isTrue()) {
           //Condition is true in the current state
           addConstraint(current, condition);
           return StatePair(&current, 0);
       } else {
           //Condition is false in the current state
           addConstraint(current, Expr::createIsZero(condition));
           return StatePair(0, &current);
       }
    }

    if (!EnableSpeculativeForking) {
        if (ce->isTrue()) {
            //Condition is true in the current state
            current.speculativeCondition = Expr::createIsZero(condition);
            bool valid = checkSpeculativeState(current);
            current.speculativeCondition = NULL;

            if (!valid) {
                //Speculative state is infeasible
                return StatePair(&current, 0);
            }
        } else {
            //Condition is false in the current state
            current.speculativeCondition = condition;
            bool valid = checkSpeculativeState(current);
            current.speculativeCondition = NULL;

            if (!valid) {
                //Speculative state is infeasible
                return StatePair(0, &current);
            }
        }
    }

    notifyBranch(current);

    ExecutionState *trueState, *falseState, *branchedState;
    branchedState = current.branch();
    addedStates.insert(branchedState);

    branchedState->speculative = true;
    branchedState->concolics.clear();

    //We don't know if the branched state could be valid
    //or not, so we mark it speculative and defer the
    //actual determination of the speculative status to later.
    if (ce->isTrue()) {
        //Condition is true in the current state
        branchedState->speculativeCondition = Expr::createIsZero(condition);
        addConstraint(current, condition);
        trueState = &current;
        falseState = branchedState;
    } else {
        //Condition is false in the current state
        branchedState->speculativeCondition = condition;
        addConstraint(current, Expr::createIsZero(condition));
        falseState = &current;
        trueState = branchedState;
    }

    current.ptreeNode->data = 0;
    std::pair<PTree::Node*, PTree::Node*> res =
      processTree->split(current.ptreeNode, falseState, trueState);
    falseState->ptreeNode = res.first;
    trueState->ptreeNode = res.second;

    return StatePair(trueState, falseState);
}

bool Executor::checkSpeculativeState(ExecutionState &state)
{
    //Check if the speculative condition satisfies the current path constraints
    Query query(state.constraints,state.speculativeCondition);
    bool truth;
    bool res = solver->solver->mustBeTrue(query.negateExpr(), truth);
    if (!res || truth) {
       return false;
    }

    return true;
}

bool Executor::resolveSpeculativeState(ExecutionState &state)
{
    assert(state.isSpeculative());

    //The speculative condition must satisfy the current path constraints
    if (!checkSpeculativeState(state)) {
        return false;
    }

    state.addConstraint(state.speculativeCondition);

    //Compute the values that satisfy the new set of path constraints.
    std::vector<const Array*> symbObjects;
    std::vector<std::vector<unsigned char> > concreteObjects;

    for (unsigned i=0; i<state.symbolics.size(); ++i) {
        symbObjects.push_back(state.symbolics[i].second);
    }

    if (!solver->getInitialValues(state, symbObjects, concreteObjects)) {
        return false;
    }

    //Add the concrete values to the current state
    for (unsigned i=0; i<symbObjects.size(); ++i) {
        state.concolics.add(symbObjects[i], concreteObjects[i]);
    }

    state.speculative = false;

    return true;
}


void Executor::notifyFork(ExecutionState &originalState, ref<Expr> &condition,
                          Executor::StatePair &targets)
{
    //Should not get here
    assert(false && "Must go through S2E");
}

Executor::StatePair 
Executor::fork(ExecutionState &current, ref<Expr> condition, bool isInternal) {
  condition = simplifyExpr(current, condition);

  Solver::Validity res;
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it = 
    seedMap.find(&current);
  bool isSeeding = it != seedMap.end();

  if (!isSeeding && !isa<ConstantExpr>(condition) && 
      (MaxStaticForkPct!=1. || MaxStaticSolvePct != 1. ||
       MaxStaticCPForkPct!=1. || MaxStaticCPSolvePct != 1.) &&
      statsTracker->elapsed() > 60.) {
    StatisticManager &sm = *theStatisticManager;
    CallPathNode *cpn = current.stack.back().callPathNode;
    if ((MaxStaticForkPct<1. &&
         sm.getIndexedValue(stats::forks, sm.getIndex()) > 
         stats::forks*MaxStaticForkPct) ||
        (MaxStaticCPForkPct<1. &&
         cpn && (cpn->statistics.getValue(stats::forks) > 
                 stats::forks*MaxStaticCPForkPct)) ||
        (MaxStaticSolvePct<1 &&
         sm.getIndexedValue(stats::solverTime, sm.getIndex()) > 
         stats::solverTime*MaxStaticSolvePct) ||
        (MaxStaticCPForkPct<1. &&
         cpn && (cpn->statistics.getValue(stats::solverTime) > 
                 stats::solverTime*MaxStaticCPSolvePct))) {
      ref<ConstantExpr> value; 
      bool success = solver->getValue(current, condition, value);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
      addConstraint(current, EqExpr::create(value, condition));
      condition = value;
    }      
  }

  double timeout = stpTimeout;
  if (isSeeding)
    timeout *= it->second.size();
  solver->setTimeout(timeout);
  bool success = solver->evaluate(current, condition, res);
  solver->setTimeout(0);
  if (!success) {
    current.pc = current.prevPC;
    std::stringstream ss;
    ss << "Query timed out on condition " << condition;
    terminateStateEarly(current, ss.str());
    return StatePair(0, 0);
  }

  if (!isSeeding) {
    if (replayPath && !isInternal) {
      assert(replayPosition<replayPath->size() &&
             "ran out of branches in replay path mode");
      bool branch = (*replayPath)[replayPosition++];
      
      if (res==Solver::True) {
        assert(branch && "hit invalid branch in replay path mode");
      } else if (res==Solver::False) {
        assert(!branch && "hit invalid branch in replay path mode");
      } else {
        // add constraints
        if(branch) {
          res = Solver::True;
          addConstraint(current, condition);
        } else  {
          res = Solver::False;
          addConstraint(current, Expr::createIsZero(condition));
        }
      }
    } else if (res==Solver::Unknown) {
      assert(!replayOut && "in replay mode, only one branch can be true.");
      
      if ((MaxMemoryInhibit && atMemoryLimit) || 
          current.forkDisabled ||
          inhibitForking || 
          (MaxForks!=~0u && stats::forks >= MaxForks)) {

	if (MaxMemoryInhibit && atMemoryLimit)
	  klee_warning_once(0, "skipping fork (memory cap exceeded)");
	else if (current.forkDisabled)
	  klee_warning_once(0, "skipping fork (fork disabled on current path)");
	else if (inhibitForking)
	  klee_warning_once(0, "skipping fork (fork disabled globally)");
	else 
	  klee_warning_once(0, "skipping fork (max-forks reached)");

        TimerStatIncrementer timer(stats::forkTime);
        if (theRNG.getBool()) {
          addConstraint(current, condition);
          res = Solver::True;        
        } else {
          addConstraint(current, Expr::createIsZero(condition));
          res = Solver::False;
        }
      }
    }
  }

  // Fix branch in only-replay-seed mode, if we don't have both true
  // and false seeds.
  if (isSeeding && 
      (current.forkDisabled || OnlyReplaySeeds) && 
      res == Solver::Unknown) {
    bool trueSeed=false, falseSeed=false;
    // Is seed extension still ok here?
    for (std::vector<SeedInfo>::iterator siit = it->second.begin(), 
           siie = it->second.end(); siit != siie; ++siit) {
      ref<ConstantExpr> res;
      bool success = 
        solver->getValue(current, siit->assignment.evaluate(condition), res);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
      if (res->isTrue()) {
        trueSeed = true;
      } else {
        falseSeed = true;
      }
      if (trueSeed && falseSeed)
        break;
    }
    if (!(trueSeed && falseSeed)) {
      assert(trueSeed || falseSeed);
      
      res = trueSeed ? Solver::True : Solver::False;
      addConstraint(current, trueSeed ? condition : Expr::createIsZero(condition));
    }
  }


  // XXX - even if the constraint is provable one way or the other we
  // can probably benefit by adding this constraint and allowing it to
  // reduce the other constraints. For example, if we do a binary
  // search on a particular value, and then see a comparison against
  // the value it has been fixed at, we should take this as a nice
  // hint to just use the single constraint instead of all the binary
  // search ones. If that makes sense.
  if (res==Solver::True) {
    if (!isInternal) {
      if (pathWriter) {
        current.pathOS << "1";
      }
    }

    return StatePair(&current, 0);
  } else if (res==Solver::False) {
    if (!isInternal) {
      if (pathWriter) {
        current.pathOS << "0";
      }
    }

    return StatePair(0, &current);
  } else {
    TimerStatIncrementer timer(stats::forkTime);
    ExecutionState *falseState, *trueState = &current;

    ++stats::forks;

    notifyBranch(*trueState);
    falseState = trueState->branch();
    addedStates.insert(falseState);

    if (RandomizeFork && theRNG.getBool())
      std::swap(trueState, falseState);

    if (it != seedMap.end()) {
      std::vector<SeedInfo> seeds = it->second;
      it->second.clear();
      std::vector<SeedInfo> &trueSeeds = seedMap[trueState];
      std::vector<SeedInfo> &falseSeeds = seedMap[falseState];
      for (std::vector<SeedInfo>::iterator siit = seeds.begin(), 
             siie = seeds.end(); siit != siie; ++siit) {
        ref<ConstantExpr> res;
        bool success = 
          solver->getValue(current, siit->assignment.evaluate(condition), res);
        assert(success && "FIXME: Unhandled solver failure");
        (void) success;
        if (res->isTrue()) {
          trueSeeds.push_back(*siit);
        } else {
          falseSeeds.push_back(*siit);
        }
      }
      
      bool swapInfo = false;
      if (trueSeeds.empty()) {
        if (&current == trueState) swapInfo = true;
        seedMap.erase(trueState);
      }
      if (falseSeeds.empty()) {
        if (&current == falseState) swapInfo = true;
        seedMap.erase(falseState);
      }
      if (swapInfo) {
        std::swap(trueState->coveredNew, falseState->coveredNew);
        std::swap(trueState->coveredLines, falseState->coveredLines);
      }
    }

    current.ptreeNode->data = 0;
    std::pair<PTree::Node*, PTree::Node*> res =
      processTree->split(current.ptreeNode, falseState, trueState);
    falseState->ptreeNode = res.first;
    trueState->ptreeNode = res.second;

    if (!isInternal) {
      if (pathWriter) {
        falseState->pathOS = pathWriter->open(current.pathOS);
        trueState->pathOS << "1";
        falseState->pathOS << "0";
      }      
      if (symPathWriter) {
        falseState->symPathOS = symPathWriter->open(current.symPathOS);
        trueState->symPathOS << "1";
        falseState->symPathOS << "0";
      }
    }

    addConstraint(*trueState, condition);
    addConstraint(*falseState, Expr::createIsZero(condition));

    // Kinda gross, do we even really still want this option?
    if (MaxDepth && MaxDepth<=trueState->depth) {
      terminateStateEarly(*trueState, "max-depth exceeded");
      terminateStateEarly(*falseState, "max-depth exceeded");
      return StatePair(0, 0);
    }

    return StatePair(trueState, falseState);
  }
}

bool Executor::merge(ExecutionState &base, ExecutionState &other)
{
    return base.merge(other);
}

void Executor::addConstraint(ExecutionState &state, ref<Expr> condition) {
  condition = simplifyExpr(state, condition);
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(condition)) {
    assert(CE->isTrue() && "attempt to add invalid constraint");
    return;
  }

  // Check to see if this constraint violates seeds.
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it = 
    seedMap.find(&state);
  if (it != seedMap.end()) {
    bool warn = false;
    for (std::vector<SeedInfo>::iterator siit = it->second.begin(), 
           siie = it->second.end(); siit != siie; ++siit) {
      bool res;
      bool success = 
        solver->mustBeFalse(state, siit->assignment.evaluate(condition), res);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
      if (res) {
        siit->patchSeed(state, condition, solver);
        warn = true;
      }
    }
    if (warn)
      klee_warning("seeds patched for violating constraint"); 
  }

  state.addConstraint(condition);
  if (ivcEnabled)
    doImpliedValueConcretization(state, condition, 
                                 ConstantExpr::alloc(1, Expr::Bool));
}

ref<klee::ConstantExpr> Executor::evalConstant(Constant *c) {
  if (llvm::ConstantExpr *ce = dyn_cast<llvm::ConstantExpr>(c)) {
    return evalConstantExpr(ce);
  } else {
    if (const ConstantInt *ci = dyn_cast<ConstantInt>(c)) {
      return ConstantExpr::alloc(ci->getValue());
    } else if (const ConstantFP *cf = dyn_cast<ConstantFP>(c)) {      
      return ConstantExpr::alloc(cf->getValueAPF().bitcastToAPInt());
    } else if (const GlobalValue *gv = dyn_cast<GlobalValue>(c)) {
        std::map<const llvm::GlobalValue*, ref<ConstantExpr> >::iterator
                it = globalAddresses.find(gv);
        assert(it != globalAddresses.end());
        return it->second;
    } else if (isa<ConstantPointerNull>(c)) {
      return Expr::createPointer(0);
    } else if (isa<UndefValue>(c) || isa<ConstantAggregateZero>(c)) {
      return ConstantExpr::create(0, getWidthForLLVMType(c->getType()));
    } else if (const ConstantDataSequential *cds =
                 dyn_cast<ConstantDataSequential>(c)) {
      std::vector<ref<Expr> > kids;
      for (unsigned i = 0, e = cds->getNumElements(); i != e; ++i) {
        ref<Expr> kid = evalConstant(cds->getElementAsConstant(i));
        kids.push_back(kid);
      }
      ref<Expr> res = ConcatExpr::createN(kids.size(), kids.data());
      return cast<ConstantExpr>(res);
    } else if (const ConstantArray *ca =
                 dyn_cast<ConstantArray>(c)) {
      std::vector<ref<Expr> > kids;
      for (unsigned i = 0, e = ca->getNumOperands(); i != e; ++i) {
        ref<Expr> kid = evalConstant(ca->getOperand(i));
        kids.push_back(kid);
      }
      ref<Expr> res = ConcatExpr::createN(kids.size(), kids.data());
      return cast<ConstantExpr>(res);
    } else if (const ConstantStruct *cs = dyn_cast<ConstantStruct>(c)) {
      const StructLayout *sl = kmodule->targetData->getStructLayout(cs->getType());
      llvm::SmallVector<ref<Expr>, 4> kids;
      for (unsigned i = cs->getNumOperands(); i != 0; --i) {
        unsigned op = i-1;
        ref<Expr> kid = evalConstant(cs->getOperand(op));

        uint64_t thisOffset = sl->getElementOffsetInBits(op),
                 nextOffset = (op == cs->getNumOperands() - 1)
                              ? sl->getSizeInBits()
                              : sl->getElementOffsetInBits(op+1);
        if (nextOffset-thisOffset > kid->getWidth()) {
          uint64_t paddingWidth = nextOffset-thisOffset-kid->getWidth();
          kids.push_back(ConstantExpr::create(0, paddingWidth));
        }

        kids.push_back(kid);
      }
      ref<Expr> res = ConcatExpr::createN(kids.size(), kids.data());
      return cast<ConstantExpr>(res);
    } else {
      // Constant{Vector}
      assert(0 && "invalid argument to evalConstant()");
    }
  }
}

const Cell& Executor::eval(KInstruction *ki, unsigned index, 
                           ExecutionState &state) const {
  assert(index < ki->inst->getNumOperands());
  int vnumber = ki->operands[index];

  assert(vnumber != -1 &&
         "Invalid operand to eval(), not a value or constant!");

  // Determine if this is a constant or not.
  if (vnumber < 0) {
    unsigned index = -vnumber - 2;
    return kmodule->constantTable[index];
  } else {
    unsigned index = vnumber;
    StackFrame &sf = state.stack.back();
    //*klee_warning_stream << "op idx=" << std::dec << index << '\n';
    return sf.locals[index];
  }
}

void Executor::bindLocal(KInstruction *target, ExecutionState &state, 
                         ref<Expr> value) {

    getDestCell(state, target).value = simplifyExpr(state, value);
}

void Executor::bindArgument(KFunction *kf, unsigned index, 
                            ExecutionState &state, ref<Expr> value) {
  getArgumentCell(state, kf, index).value = simplifyExpr(state, value);
}

ref<Expr> Executor::toUnique(const ExecutionState &state, ref<Expr> &e)
{
    e = simplifyExpr(state, e);
    ref<Expr> result = e;

    if (isa<ConstantExpr>(e)) {
        return result;
    }

    ref<ConstantExpr> value;
    bool isTrue = false;

    solver->setTimeout(stpTimeout);

    if (concolicMode) {
        ref<Expr> evalResult = state.concolics.evaluate(e);
        assert(isa<ConstantExpr>(evalResult) && "Must be concrete");
        value = dyn_cast<ConstantExpr>(evalResult);
    } else {
        if (!solver->getValue(state, e, value)) {
            return result;
        }
    }

    bool success = solver->mustBeTrue(state, simplifyExpr(state, EqExpr::create(e, value)), isTrue);

    if (success && isTrue) {
        result = value;
    }

    solver->setTimeout(0);
  
    return result;
}


/* Concretize the given expression, and return a possible constant value. 
   'reason' is just a documentation string stating the reason for concretization. */
ref<klee::ConstantExpr> 
Executor::toConstant(ExecutionState &state, 
                     ref<Expr> e,
                     const char *reason) {
  e = simplifyExpr(state, e);
  e = state.constraints.simplifyExpr(e);
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e))
    return CE;

  ref<ConstantExpr> value;

  if (concolicMode) {
      ref<Expr> evalResult = state.concolics.evaluate(e);
      assert(isa<ConstantExpr>(evalResult) && "Must be concrete");
      value = dyn_cast<ConstantExpr>(evalResult);
  } else {
      bool success = solver->getValue(state, e, value);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
  }
    
  std::ostringstream os;
  os << "silently concretizing (reason: " << reason << ") expression " << e 
     << " to value " << value 
     << " (" << (*(state.pc)).info->file << ":" << (*(state.pc)).info->line << ")";
  state.m_silently_concretized = true;//epeius. We add here to avoid stuck
  klee_warning_external(reason, "%s", os.str().c_str());

  addConstraint(state, EqExpr::create(e, value));
    
  return value;
}

ref<klee::ConstantExpr>
Executor::toConstantSilent(ExecutionState &state,
                     ref<Expr> e) {
  e = simplifyExpr(state, e);
  e = state.constraints.simplifyExpr(e);
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e))
    return CE;

  ref<ConstantExpr> value;

  if (concolicMode) {
      ref<Expr> evalResult = state.concolics.evaluate(e);
      assert(isa<ConstantExpr>(evalResult) && "Must be concrete");
      value = dyn_cast<ConstantExpr>(evalResult);
  } else {
      bool success = solver->getValue(state, e, value);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
  }

  return value;
}

void Executor::executeGetValue(ExecutionState &state,
                               ref<Expr> e,
                               KInstruction *target) {
  e = state.constraints.simplifyExpr(e);
  assert(seedMap.empty() && "S2E does not support seeding");

  e = toConstant(state, e, "klee_get_value()");
  bindLocal(target, state, e);
}

void Executor::stepInstruction(ExecutionState &state) {
  if (DebugPrintInstructions) {
    printFileLine(state, state.pc);
    llvm::errs() << stats::instructions << " ";
    llvm::errs() << *(state.pc->inst)  << "\n";
  }

  if (statsTracker)
    statsTracker->stepInstruction(state);

  ++stats::instructions;
  state.prevPC = state.pc;
  ++state.pc;

  if (stats::instructions==StopAfterNInstructions)
    haltExecution = true;
}

void Executor::executeCall(ExecutionState &state, 
                           KInstruction *ki,
                           Function *f,
                           std::vector< ref<Expr> > &arguments) {
  Instruction *i = ki->inst;

  if (f && overridenInternalFunctions.find(f) != overridenInternalFunctions.end()) {
      callExternalFunction(state, ki, f, arguments);
  } else

  if (f && f->isDeclaration()) {
    switch(f->getIntrinsicID()) {
    case Intrinsic::not_intrinsic:
      // state may be destroyed by this call, cannot touch
      callExternalFunction(state, ki, f, arguments);
      break;
        
      // va_arg is handled by caller and intrinsic lowering, see comment for
      // ExecutionState::varargs
    case Intrinsic::vastart:  {
      StackFrame &sf = state.stack.back();
      assert(sf.varargs && 
             "vastart called in function with no vararg object");

      // FIXME: This is really specific to the architecture, not the pointer
      // size. This happens to work fir x86-32 and x86-64, however.
      Expr::Width WordSize = Context::get().getPointerWidth();
      if (WordSize == Expr::Int32) {
        executeMemoryOperation(state, true, arguments[0], 
                               sf.varargs->getBaseExpr(), 0);
      } else {
        assert(WordSize == Expr::Int64 && "Unknown word size!");

        // X86-64 has quite complicated calling convention. However,
        // instead of implementing it, we can do a simple hack: just
        // make a function believe that all varargs are on stack.
        executeMemoryOperation(state, true, arguments[0],
                               ConstantExpr::create(48, 32), 0); // gp_offset
        executeMemoryOperation(state, true,
                               AddExpr::create(arguments[0], 
                                               ConstantExpr::create(4, 64)),
                               ConstantExpr::create(304, 32), 0); // fp_offset
        executeMemoryOperation(state, true,
                               AddExpr::create(arguments[0], 
                                               ConstantExpr::create(8, 64)),
                               sf.varargs->getBaseExpr(), 0); // overflow_arg_area
        executeMemoryOperation(state, true,
                               AddExpr::create(arguments[0], 
                                               ConstantExpr::create(16, 64)),
                               ConstantExpr::create(0, 64), 0); // reg_save_area
      }
      break;
    }
    case Intrinsic::vaend:
      // va_end is a noop for the interpreter.
      //
      // FIXME: We should validate that the target didn't do something bad
      // with vaeend, however (like call it twice).
      break;
        
    case Intrinsic::vacopy:
      // va_copy should have been lowered.
      //
      // FIXME: It would be nice to check for errors in the usage of this as
      // well.
    default:
      klee_error("unknown intrinsic: %s", f->getName().data());
    }

    if (InvokeInst *ii = dyn_cast<InvokeInst>(i))
      transferToBasicBlock(ii->getNormalDest(), i->getParent(), state);
  } else {
    // FIXME: I'm not really happy about this reliance on prevPC but it is ok, I
    // guess. This just done to avoid having to pass KInstIterator everywhere
    // instead of the actual instruction, since we can't make a KInstIterator
    // from just an instruction (unlike LLVM).
    KFunction *kf = kmodule->functionMap[f];
    state.pushFrame(state.prevPC, kf);
    state.pc = kf->instructions;
        
    if (statsTracker)
      statsTracker->framePushed(state, &state.stack[state.stack.size()-2]);
 
     // TODO: support "byval" parameter attribute
     // TODO: support zeroext, signext, sret attributes
        
    unsigned callingArgs = arguments.size();
    unsigned funcArgs = f->arg_size();
    if (!f->isVarArg()) {
      if (callingArgs > funcArgs) {
        klee_warning_once(f, "calling %s with extra arguments.", 
                          f->getName().data());
      } else if (callingArgs < funcArgs) {
        terminateStateOnError(state, "calling function with too few arguments", 
                              "user.err");
        return;
      }
    } else {
      if (callingArgs < funcArgs) {
        terminateStateOnError(state, "calling function with too few arguments", 
                              "user.err");
        return;
      }
            
      StackFrame &sf = state.stack.back();
      unsigned size = 0;
      for (unsigned i = funcArgs; i < callingArgs; i++) {
        // FIXME: This is really specific to the architecture, not the pointer
        // size. This happens to work fir x86-32 and x86-64, however.
        Expr::Width WordSize = Context::get().getPointerWidth();
        if (WordSize == Expr::Int32) {
          size += Expr::getMinBytesForWidth(arguments[i]->getWidth());
        } else {
          size += llvm::RoundUpToAlignment(arguments[i]->getWidth(), 
                                           WordSize) / 8;
        }
      }

      MemoryObject *mo = sf.varargs = memory->allocate(size, true, false, 
                                                       state.prevPC->inst);
      if (!mo) {
        terminateStateOnExecError(state, "out of memory (varargs)");
        return;
      }
      ObjectState *os = bindObjectInState(state, mo, true);
      unsigned offset = 0;
      for (unsigned i = funcArgs; i < callingArgs; i++) {
        // FIXME: This is really specific to the architecture, not the pointer
        // size. This happens to work fir x86-32 and x86-64, however.
        Expr::Width WordSize = Context::get().getPointerWidth();
        if (WordSize == Expr::Int32) {
          os->write(offset, arguments[i]);
          offset += Expr::getMinBytesForWidth(arguments[i]->getWidth());
        } else {
          assert(WordSize == Expr::Int64 && "Unknown word size!");
          os->write(offset, arguments[i]);
          offset += llvm::RoundUpToAlignment(arguments[i]->getWidth(), 
                                             WordSize) / 8;
        }
      }
    }

    unsigned numFormals = f->arg_size();
    for (unsigned i=0; i<numFormals; ++i) 
      bindArgument(kf, i, state, arguments[i]);
  }
}

void Executor::transferToBasicBlock(BasicBlock *dst, BasicBlock *src, 
                                    ExecutionState &state) {
  // Note that in general phi nodes can reuse phi values from the same
  // block but the incoming value is the eval() result *before* the
  // execution of any phi nodes. this is pathological and doesn't
  // really seem to occur, but just in case we run the PhiCleanerPass
  // which makes sure this cannot happen and so it is safe to just
  // eval things in order. The PhiCleanerPass also makes sure that all
  // incoming blocks have the same order for each PHINode so we only
  // have to compute the index once.
  //
  // With that done we simply set an index in the state so that PHI
  // instructions know which argument to eval, set the pc, and continue.
  
  // XXX this lookup has to go ?
  KFunction *kf = state.stack.back().kf;
  unsigned entry = kf->basicBlockEntry[dst];
  state.pc = &kf->instructions[entry];
  if (state.pc->inst->getOpcode() == Instruction::PHI) {
    PHINode *first = static_cast<PHINode*>(state.pc->inst);
    state.incomingBBIndex = first->getBasicBlockIndex(src);
  }
}

void Executor::printFileLine(ExecutionState &state, KInstruction *ki) {
  const InstructionInfo &ii = *ki->info;
  if (ii.file != "") 
    llvm::errs() << "     " << ii.file << ":" << ii.line << ":";
  else
    llvm::errs() << "     [no debug info]:";
}


Function* Executor::getCalledFunction(CallSite &cs, ExecutionState &state) {
  Function *f = cs.getCalledFunction();
  
  if (f) {
    std::string alias = state.getFnAlias(f->getName());
    if (alias != "") {
      llvm::Module* currModule = kmodule->module;
      Function* old_f = f;
      f = currModule->getFunction(alias);
      if (!f) {
	llvm::errs() << "Function " << alias << "(), alias for " 
                     << old_f->getName() << " not found!\n";
	assert(f && "function alias not found");
      }
    }
  }
  
  return f;
}

static bool isDebugIntrinsic(const Function *f, KModule *KM) {
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  // Fast path, getIntrinsicID is slow.
  if (f == KM->dbgStopPointFn)
    return true;

  switch (f->getIntrinsicID()) {
  case Intrinsic::dbg_stoppoint:
  case Intrinsic::dbg_region_start:
  case Intrinsic::dbg_region_end:
  case Intrinsic::dbg_func_start:
  case Intrinsic::dbg_declare:
    return true;

  default:
    return false;
  }
#else
  return false;
#endif
}

void Executor::executeInstruction(ExecutionState &state, KInstruction *ki) {
  Instruction *i = ki->inst;
  switch (i->getOpcode()) {
    // Control flow
  case Instruction::Ret: {
    ReturnInst *ri = cast<ReturnInst>(i);
    KInstIterator kcaller = state.stack.back().caller;
    Instruction *caller = kcaller ? kcaller->inst : 0;
    bool isVoidReturn = (ri->getNumOperands() == 0);
    ref<Expr> result = ConstantExpr::alloc(0, Expr::Bool);
    
    if (!isVoidReturn) {
      result = eval(ki, 0, state).value;
    }
    
    if (state.stack.size() <= 1) {
      assert(!caller && "caller set on initial stack frame");
      terminateStateOnExit(state);
    } else {
      state.popFrame();

      if (statsTracker)
        statsTracker->framePopped(state);

      if (InvokeInst *ii = dyn_cast<InvokeInst>(caller)) {
        transferToBasicBlock(ii->getNormalDest(), caller->getParent(), state);
      } else {
        state.pc = kcaller;
        ++state.pc;
      }

      if (!isVoidReturn) {
        Type *t = caller->getType();
        if (t != Type::getVoidTy(getGlobalContext())) {
          // may need to do coercion due to bitcasts
          Expr::Width from = result->getWidth();
          Expr::Width to = getWidthForLLVMType(t);
            
          if (from != to) {
            CallSite cs = (isa<InvokeInst>(caller) ? CallSite(cast<InvokeInst>(caller)) : 
                           CallSite(cast<CallInst>(caller)));

            // XXX need to check other param attrs ?
            if (cs.paramHasAttr(0, llvm::Attributes::SExt)) {
              result = SExtExpr::create(result, to);
            } else {
              result = ZExtExpr::create(result, to);
            }
          }

          bindLocal(kcaller, state, result);
        }
      } else {
        // We check that the return value has no users instead of
        // checking the type, since C defaults to returning int for
        // undeclared functions.
        if (!caller->use_empty()) {
          terminateStateOnExecError(state, "return void when caller expected a result");
        }
      }
    }      
    break;
  }

  case Instruction::Br: {
    BranchInst *bi = cast<BranchInst>(i);
    if (bi->isUnconditional()) {
      transferToBasicBlock(bi->getSuccessor(0), bi->getParent(), state);
    } else {
      // FIXME: Find a way that we don't have this hidden dependency.
      assert(bi->getCondition() == bi->getOperand(0) &&
             "Wrong operand index!");
      ref<Expr> cond = eval(ki, 0, state).value;
      Executor::StatePair branches = fork(state, cond, false);

      // NOTE: There is a hidden dependency here, markBranchVisited
      // requires that we still be in the context of the branch
      // instruction (it reuses its statistic id). Should be cleaned
      // up with convenient instruction specific data.
      if (statsTracker && state.stack.back().kf->trackCoverage)
        statsTracker->markBranchVisited(branches.first, branches.second);

      if (branches.first)
        transferToBasicBlock(bi->getSuccessor(0), bi->getParent(), *branches.first);
      if (branches.second)
        transferToBasicBlock(bi->getSuccessor(1), bi->getParent(), *branches.second);

      notifyFork(state, cond, branches);
    }
    break;
  }
  case Instruction::Switch: {
    SwitchInst *si = cast<SwitchInst>(i);
    ref<Expr> cond = eval(ki, 0, state).value;
    BasicBlock *bb = si->getParent();

    cond = simplifyExpr(state, toUnique(state, cond));
    if (!isa<ConstantExpr>(cond)) {
        // TODO: proper support for symbolic switches
        cond = toConstant(state, cond, "Symbolic switch condition");
    }

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(cond)) {
      // Somewhat gross to create these all the time, but fine till we
      // switch to an internal rep.
      llvm::IntegerType *Ty =
        cast<IntegerType>(si->getCondition()->getType());
      ConstantInt *ci = ConstantInt::get(Ty, CE->getZExtValue());
      unsigned index = si->findCaseValue(ci).getSuccessorIndex();
      transferToBasicBlock(si->getSuccessor(index), si->getParent(), state);
    } else {
      std::map<BasicBlock*, ref<Expr> > targets;
      ref<Expr> isDefault = ConstantExpr::alloc(1, Expr::Bool);
      for (SwitchInst::CaseIt i = si->case_begin(), e = si->case_end();
           i != e; ++i) {
        ref<Expr> value = evalConstant(i.getCaseValue());
        ref<Expr> match = EqExpr::create(cond, value);
        isDefault = simplifyExpr(state, AndExpr::create(isDefault,
                                    Expr::createIsZero(match)));
        bool result;
        assert(!concolicMode && "Not tested in concolic mode");
        bool success = solver->mayBeTrue(state, match, result);
        assert(success && "FIXME: Unhandled solver failure");
        (void) success;
        if (result) {
          std::map<BasicBlock*, ref<Expr> >::iterator it =
            targets.insert(std::make_pair(i.getCaseSuccessor(),
                                          ConstantExpr::alloc(0, Expr::Bool))).first;
          it->second = OrExpr::create(match, it->second);
        }
      }
      bool res;
      assert(!concolicMode && "Not tested in concolic mode");
      bool success = solver->mayBeTrue(state, isDefault, res);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
      if (res)
        targets.insert(std::make_pair(si->getSuccessor(0), isDefault));
      
      std::vector< ref<Expr> > conditions;
      for (std::map<BasicBlock*, ref<Expr> >::iterator it = 
             targets.begin(), ie = targets.end();
           it != ie; ++it)
        conditions.push_back(it->second);
      
      std::vector<ExecutionState*> branches;
      branch(state, conditions, branches);
        
      std::vector<ExecutionState*>::iterator bit = branches.begin();
      for (std::map<BasicBlock*, ref<Expr> >::iterator it = 
             targets.begin(), ie = targets.end();
           it != ie; ++it) {
        ExecutionState *es = *bit;
        if (es)
          transferToBasicBlock(it->first, bb, *es);
        ++bit;
      }
    }
    break;
 }
  case Instruction::Unreachable:
    // Note that this is not necessarily an internal bug, llvm will
    // generate unreachable instructions in cases where it knows the
    // program will crash. So it is effectively a SEGV or internal
    // error.
    terminateStateOnExecError(state, "reached \"unreachable\" instruction");
    break;

  case Instruction::Invoke:
  case Instruction::Call: {
    CallSite cs;
    unsigned argStart;
    if (i->getOpcode()==Instruction::Call) {
      cs = CallSite(cast<CallInst>(i));
      argStart = 1;
    } else {
      cs = CallSite(cast<InvokeInst>(i));
      argStart = 3;
    }

    unsigned numArgs = cs.arg_size();
    Function *f = getCalledFunction(cs, state);

    // Skip debug intrinsics, we can't evaluate their metadata arguments.
    if (f && isDebugIntrinsic(f, kmodule))
      break;

    // evaluate arguments
    std::vector< ref<Expr> > arguments;
    arguments.reserve(numArgs);

    for (unsigned j=0; j<numArgs; ++j)
      arguments.push_back(eval(ki, argStart+j, state).value);

    if (!f) {
      // special case the call with a bitcast case
      Value *fp = cs.getCalledValue();
      llvm::ConstantExpr *ce = dyn_cast<llvm::ConstantExpr>(fp);
        
      if (ce && ce->getOpcode()==Instruction::BitCast) {
        f = dyn_cast<Function>(ce->getOperand(0));
        assert(f && "XXX unrecognized constant expression in call");
        const FunctionType *fType = 
          dyn_cast<FunctionType>(cast<PointerType>(f->getType())->getElementType());
        const FunctionType *ceType =
          dyn_cast<FunctionType>(cast<PointerType>(ce->getType())->getElementType());
        assert(fType && ceType && "unable to get function type");

        // XXX check result coercion

        // XXX this really needs thought and validation
        unsigned i=0;
        for (std::vector< ref<Expr> >::iterator
               ai = arguments.begin(), ie = arguments.end();
             ai != ie; ++ai) {
          Expr::Width to, from = (*ai)->getWidth();
            
          if (i<fType->getNumParams()) {
            to = getWidthForLLVMType(fType->getParamType(i));

            if (from != to) {
              // XXX need to check other param attrs ?
              if (cs.paramHasAttr(i+1, llvm::Attributes::SExt)) {
                arguments[i] = SExtExpr::create(arguments[i], to);
              } else {
                arguments[i] = ZExtExpr::create(arguments[i], to);
              }
            }
          }
            
          i++;
        }
      } else if (isa<InlineAsm>(fp)) {
        terminateStateOnExecError(state, "inline assembly is unsupported");
        break;
      }
    }

    if (f) {
      executeCall(state, ki, f, arguments);
    } else {
      ref<Expr> v = eval(ki, 0, state).value;

      ExecutionState *free = &state;
      bool hasInvalid = false, first = true;

      /* XXX This is wasteful, no need to do a full evaluate since we
         have already got a value. But in the end the caches should
         handle it for us, albeit with some overhead. */
      do {
        ref<ConstantExpr> value;
        assert(!concolicMode && "Not tested in concolic mode");
        bool success = solver->getValue(*free, v, value);
        assert(success && "FIXME: Unhandled solver failure");
        (void) success;
        ref<Expr> cond = EqExpr::create(v, value);
        StatePair res = fork(*free, cond, true);
        notifyFork(*free, cond, res);
        if (res.first) {
          uint64_t addr = value->getZExtValue();
          if (legalFunctions.count(addr)) {
            f = (Function*) addr;

            // Don't give warning on unique resolution
            if (res.second || !first)
              klee_warning_once((void*) (uintptr_t) addr, 
                                "resolved symbolic function pointer to: %s",
                                f->getName().data());

            executeCall(*res.first, ki, f, arguments);
          } else {
            if (!hasInvalid) {
              terminateStateOnExecError(state, "invalid function pointer");
              hasInvalid = true;
            }
          }
        }

        first = false;
        free = res.second;
      } while (free);
    }
    break;
  }
  case Instruction::PHI: {
    ref<Expr> result = eval(ki, state.incomingBBIndex, state).value;
    bindLocal(ki, state, result);
    break;
  }

    // Special instructions
  case Instruction::Select: {
    SelectInst *SI = cast<SelectInst>(ki->inst);
    assert(SI->getCondition() == SI->getOperand(0) &&
           "Wrong operand index!");
    ref<Expr> cond = eval(ki, 0, state).value;
    ref<Expr> tExpr = eval(ki, 1, state).value;
    ref<Expr> fExpr = eval(ki, 2, state).value;
    ref<Expr> result = SelectExpr::create(cond, tExpr, fExpr);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::VAArg:
    terminateStateOnExecError(state, "unexpected VAArg instruction");
    break;

    // Arithmetic / logical

  case Instruction::Add: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    bindLocal(ki, state, AddExpr::create(left, right));
    break;
  }

  case Instruction::Sub: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    bindLocal(ki, state, SubExpr::create(left, right));
    break;
  }
 
  case Instruction::Mul: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    bindLocal(ki, state, MulExpr::create(left, right));
    break;
  }

  case Instruction::UDiv: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = UDivExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::SDiv: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = SDivExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::URem: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = URemExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }
 
  case Instruction::SRem: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = SRemExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::And: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = AndExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::Or: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = OrExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::Xor: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = XorExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::Shl: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = ShlExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::LShr: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = LShrExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::AShr: {
    ref<Expr> left = eval(ki, 0, state).value;
    ref<Expr> right = eval(ki, 1, state).value;
    ref<Expr> result = AShrExpr::create(left, right);
    bindLocal(ki, state, result);
    break;
  }

    // Compare

  case Instruction::ICmp: {
    CmpInst *ci = cast<CmpInst>(i);
    ICmpInst *ii = cast<ICmpInst>(ci);
 
    switch(ii->getPredicate()) {
    case ICmpInst::ICMP_EQ: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = EqExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_NE: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = NeExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_UGT: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = UgtExpr::create(left, right);
      bindLocal(ki, state,result);
      break;
    }

    case ICmpInst::ICMP_UGE: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = UgeExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_ULT: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = UltExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_ULE: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = UleExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_SGT: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = SgtExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_SGE: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = SgeExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_SLT: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = SltExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    case ICmpInst::ICMP_SLE: {
      ref<Expr> left = eval(ki, 0, state).value;
      ref<Expr> right = eval(ki, 1, state).value;
      ref<Expr> result = SleExpr::create(left, right);
      bindLocal(ki, state, result);
      break;
    }

    default:
      terminateStateOnExecError(state, "invalid ICmp predicate");
    }
    break;
  }
 
    // Memory instructions...
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  case Instruction::Malloc:
  case Instruction::Alloca: {
    AllocationInst *ai = cast<AllocationInst>(i);
#else
  case Instruction::Alloca: {
    AllocaInst *ai = cast<AllocaInst>(i);
#endif
    unsigned elementSize = 
      kmodule->targetData->getTypeStoreSize(ai->getAllocatedType());
    ref<Expr> size = Expr::createPointer(elementSize);
    if (ai->isArrayAllocation()) {
      ref<Expr> count = eval(ki, 0, state).value;
      count = Expr::createCoerceToPointerType(count);
      size = MulExpr::create(size, count);
    }
    bool isLocal = i->getOpcode()==Instruction::Alloca;
    executeAlloc(state, size, isLocal, ki);
    break;
  }
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  case Instruction::Free: {
    executeFree(state, eval(ki, 0, state).value);
    break;
  }
#endif

  case Instruction::Load: {
    ref<Expr> base = eval(ki, 0, state).value;
    executeMemoryOperation(state, false, base, 0, ki);
    break;
  }
  case Instruction::Store: {
    ref<Expr> base = eval(ki, 1, state).value;
    ref<Expr> value = eval(ki, 0, state).value;
    executeMemoryOperation(state, true, base, value, 0);
    break;
  }

  case Instruction::GetElementPtr: {
    KGEPInstruction *kgepi = static_cast<KGEPInstruction*>(ki);
    ref<Expr> base = eval(ki, 0, state).value;

    for (std::vector< std::pair<unsigned, uint64_t> >::iterator 
           it = kgepi->indices.begin(), ie = kgepi->indices.end(); 
         it != ie; ++it) {
      uint64_t elementSize = it->second;
      ref<Expr> index = eval(ki, it->first, state).value;
      base = AddExpr::create(base,
                             MulExpr::create(Expr::createCoerceToPointerType(index),
                                             Expr::createPointer(elementSize)));
    }
    if (kgepi->offset)
      base = AddExpr::create(base,
                             Expr::createPointer(kgepi->offset));
    bindLocal(ki, state, base);
    break;
  }

    // Conversion
  case Instruction::Trunc: {
    CastInst *ci = cast<CastInst>(i);
    ref<Expr> result = ExtractExpr::create(eval(ki, 0, state).value,
                                           0,
                                           getWidthForLLVMType(ci->getType()));
    bindLocal(ki, state, result);
    break;
  }
  case Instruction::ZExt: {
    CastInst *ci = cast<CastInst>(i);
    ref<Expr> result = ZExtExpr::create(eval(ki, 0, state).value,
                                        getWidthForLLVMType(ci->getType()));
    bindLocal(ki, state, result);
    break;
  }
  case Instruction::SExt: {
    CastInst *ci = cast<CastInst>(i);
    ref<Expr> result = SExtExpr::create(eval(ki, 0, state).value,
                                        getWidthForLLVMType(ci->getType()));
    bindLocal(ki, state, result);
    break;
  }

  case Instruction::IntToPtr: {
    CastInst *ci = cast<CastInst>(i);
    Expr::Width pType = getWidthForLLVMType(ci->getType());
    ref<Expr> arg = eval(ki, 0, state).value;
    bindLocal(ki, state, ZExtExpr::create(arg, pType));
    break;
  } 
  case Instruction::PtrToInt: {
    CastInst *ci = cast<CastInst>(i);
    Expr::Width iType = getWidthForLLVMType(ci->getType());
    ref<Expr> arg = eval(ki, 0, state).value;
    bindLocal(ki, state, ZExtExpr::create(arg, iType));
    break;
  }

  case Instruction::BitCast: {
    ref<Expr> result = eval(ki, 0, state).value;
    bindLocal(ki, state, result);
    break;
  }

    // Floating point instructions

  case Instruction::FAdd: {
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    llvm::APFloat Res(left->getAPValue());
    Res.add(APFloat(right->getAPValue()), APFloat::rmNearestTiesToEven);
    bindLocal(ki, state, ConstantExpr::alloc(Res.bitcastToAPInt()));
    break;
  }

  case Instruction::FSub: {
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    llvm::APFloat Res(left->getAPValue());
    Res.subtract(APFloat(right->getAPValue()), APFloat::rmNearestTiesToEven);
    bindLocal(ki, state, ConstantExpr::alloc(Res.bitcastToAPInt()));
    break;
  }
 
  case Instruction::FMul: {
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    llvm::APFloat Res(left->getAPValue());
    Res.multiply(APFloat(right->getAPValue()), APFloat::rmNearestTiesToEven);
    bindLocal(ki, state, ConstantExpr::alloc(Res.bitcastToAPInt()));
    break;
  }

  case Instruction::FDiv: {
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    llvm::APFloat Res(left->getAPValue());
    Res.divide(APFloat(right->getAPValue()), APFloat::rmNearestTiesToEven);
    bindLocal(ki, state, ConstantExpr::alloc(Res.bitcastToAPInt()));
    break;
  }

  case Instruction::FRem: {
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    llvm::APFloat Res(left->getAPValue());
    Res.mod(APFloat(right->getAPValue()), APFloat::rmNearestTiesToEven);
    bindLocal(ki, state, ConstantExpr::alloc(Res.bitcastToAPInt()));
    break;
  }

  case Instruction::FPTrunc: {
    FPTruncInst *fi = cast<FPTruncInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported FPTrunc operation");
    uint64_t value = floats::trunc(arg->getZExtValue(),
                                   resultType,
                                   arg->getWidth());
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::FPExt: {
    FPExtInst *fi = cast<FPExtInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported FPExt operation");
    uint64_t value = floats::ext(arg->getZExtValue(),
                                 resultType,
                                 arg->getWidth());
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::FPToUI: {
    FPToUIInst *fi = cast<FPToUIInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported FPToUI operation");
    uint64_t value = floats::toUnsignedInt(arg->getZExtValue(),
                                           resultType,
                                           arg->getWidth());
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::FPToSI: {
    FPToSIInst *fi = cast<FPToSIInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported FPToSI operation");
    uint64_t value = floats::toSignedInt(arg->getZExtValue(),
                                         resultType,
                                         arg->getWidth());
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::UIToFP: {
    UIToFPInst *fi = cast<UIToFPInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported UIToFP operation");
    uint64_t value = floats::UnsignedIntToFP(arg->getZExtValue(),
                                             resultType);
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::SIToFP: {
    SIToFPInst *fi = cast<SIToFPInst>(i);
    Expr::Width resultType = getWidthForLLVMType(fi->getType());
    ref<ConstantExpr> arg = toConstant(state, eval(ki, 0, state).value,
                                       "floating point");
    if (arg->getWidth() > 64)
      return terminateStateOnExecError(state, "Unsupported SIToFP operation");
    uint64_t value = floats::SignedIntToFP(arg->getZExtValue(),
                                           resultType,
                                           arg->getWidth());
    bindLocal(ki, state, ConstantExpr::alloc(value, resultType));
    break;
  }

  case Instruction::FCmp: {
    FCmpInst *fi = cast<FCmpInst>(i);
    ref<ConstantExpr> left = toConstant(state, eval(ki, 0, state).value,
                                        "floating point");
    ref<ConstantExpr> right = toConstant(state, eval(ki, 1, state).value,
                                         "floating point");
    APFloat LHS(left->getAPValue());
    APFloat RHS(right->getAPValue());
    APFloat::cmpResult CmpRes = LHS.compare(RHS);

    bool Result = false;
    switch( fi->getPredicate() ) {
      // Predicates which only care about whether or not the operands are NaNs.
    case FCmpInst::FCMP_ORD:
      Result = CmpRes != APFloat::cmpUnordered;
      break;

    case FCmpInst::FCMP_UNO:
      Result = CmpRes == APFloat::cmpUnordered;
      break;

      // Ordered comparisons return false if either operand is NaN.  Unordered
      // comparisons return true if either operand is NaN.
    case FCmpInst::FCMP_UEQ:
      if (CmpRes == APFloat::cmpUnordered) {
        Result = true;
        break;
      }
    case FCmpInst::FCMP_OEQ:
      Result = CmpRes == APFloat::cmpEqual;
      break;

    case FCmpInst::FCMP_UGT:
      if (CmpRes == APFloat::cmpUnordered) {
        Result = true;
        break;
      }
    case FCmpInst::FCMP_OGT:
      Result = CmpRes == APFloat::cmpGreaterThan;
      break;

    case FCmpInst::FCMP_UGE:
      if (CmpRes == APFloat::cmpUnordered) {
        Result = true;
        break;
      }
    case FCmpInst::FCMP_OGE:
      Result = CmpRes == APFloat::cmpGreaterThan || CmpRes == APFloat::cmpEqual;
      break;

    case FCmpInst::FCMP_ULT:
      if (CmpRes == APFloat::cmpUnordered) {
        Result = true;
        break;
      }
    case FCmpInst::FCMP_OLT:
      Result = CmpRes == APFloat::cmpLessThan;
      break;

    case FCmpInst::FCMP_ULE:
      if (CmpRes == APFloat::cmpUnordered) {
        Result = true;
        break;
      }
    case FCmpInst::FCMP_OLE:
      Result = CmpRes == APFloat::cmpLessThan || CmpRes == APFloat::cmpEqual;
      break;

    case FCmpInst::FCMP_UNE:
      Result = CmpRes == APFloat::cmpUnordered || CmpRes != APFloat::cmpEqual;
      break;
    case FCmpInst::FCMP_ONE:
      Result = CmpRes != APFloat::cmpUnordered && CmpRes != APFloat::cmpEqual;
      break;

    default:
      assert(0 && "Invalid FCMP predicate!");
    case FCmpInst::FCMP_FALSE:
      Result = false;
      break;
    case FCmpInst::FCMP_TRUE:
      Result = true;
      break;
    }

    bindLocal(ki, state, ConstantExpr::alloc(Result, Expr::Bool));
    break;
  }
 
  case Instruction::InsertValue: {
      KGEPInstruction *kgepi = static_cast<KGEPInstruction*>(ki);

      ref<Expr> agg = eval(ki, 0, state).value;
      ref<Expr> val = eval(ki, 1, state).value;

      ref<Expr> l = NULL, r = NULL;
      unsigned lOffset = kgepi->offset*8, rOffset = kgepi->offset*8 + val->getWidth();

      if (lOffset > 0)
        l = ExtractExpr::create(agg, 0, lOffset);
      if (rOffset < agg->getWidth())
        r = ExtractExpr::create(agg, rOffset, agg->getWidth() - rOffset);

      ref<Expr> result;
      if (!l.isNull() && !r.isNull())
        result = ConcatExpr::create(r, ConcatExpr::create(val, l));
      else if (!l.isNull())
        result = ConcatExpr::create(val, l);
      else if (!r.isNull())
        result = ConcatExpr::create(r, val);
      else
        result = val;

      bindLocal(ki, state, result);
      break;
    }
    case Instruction::ExtractValue: {
      KGEPInstruction *kgepi = static_cast<KGEPInstruction*>(ki);

      ref<Expr> agg = eval(ki, 0, state).value;

      ref<Expr> result = ExtractExpr::create(agg, kgepi->offset*8, getWidthForLLVMType(i->getType()));

      bindLocal(ki, state, result);
      break;
    }

  case Instruction::ExtractElement: {
      ExtractElementInst *eei = cast<ExtractElementInst>(i);
      ref<Expr> vec = eval(ki, 0, state).value;
      ref<Expr> idx = eval(ki, 1, state).value;

      assert(isa<ConstantExpr>(idx) && "symbolic index unsupported");
      ConstantExpr *cIdx = cast<ConstantExpr>(idx);
      uint64_t iIdx = cIdx->getZExtValue();

      const llvm::VectorType *vt = eei->getVectorOperandType();
      unsigned EltBits = getWidthForLLVMType(vt->getElementType());

      ref<Expr> Result = ExtractExpr::create(vec, EltBits*iIdx, EltBits);

      bindLocal(ki, state, Result);
      break;
    }
    case Instruction::InsertElement: {
      InsertElementInst *iei = cast<InsertElementInst>(i);
      ref<Expr> vec = eval(ki, 0, state).value;
      ref<Expr> newElt = eval(ki, 1, state).value;
      ref<Expr> idx = eval(ki, 2, state).value;

      assert(isa<ConstantExpr>(idx) && "symbolic index unsupported");
      ConstantExpr *cIdx = cast<ConstantExpr>(idx);
      uint64_t iIdx = cIdx->getZExtValue();

      const llvm::VectorType *vt = iei->getType();
      unsigned EltBits = getWidthForLLVMType(vt->getElementType());

      unsigned ElemCount = vt->getNumElements();
      ref<Expr> *elems = new ref<Expr>[vt->getNumElements()];
      for (unsigned i = 0; i < ElemCount; ++i)
        elems[ElemCount-i-1] = i == iIdx
                               ? newElt
                               : ExtractExpr::create(vec, EltBits*i, EltBits);

      ref<Expr> Result = ConcatExpr::createN(ElemCount, elems);
      delete[] elems;

      bindLocal(ki, state, Result);
      break;
    }

    // Other instructions...
    // Unhandled
  case Instruction::ShuffleVector:
    terminateStateOnError(state, "XXX vector instructions unhandled",
                          "xxx.err");
    break;
 
  default:
    {
        std::string errstr;
        llvm::raw_string_ostream err(errstr);
        err << *i;
        terminateStateOnExecError(state, "illegal instruction " + errstr);
    }

    break;
  }
}

void Executor::updateStates(ExecutionState *current) {
  if (searcher) {
    searcher->update(current, addedStates, removedStates);
  }
  
  states.insert(addedStates.begin(), addedStates.end());
  addedStates.clear();
  
  for (std::set<ExecutionState*>::iterator
         it = removedStates.begin(), ie = removedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    std::set<ExecutionState*>::iterator it2 = states.find(es);
    assert(it2!=states.end());
    states.erase(it2);
    std::map<ExecutionState*, std::vector<SeedInfo> >::iterator it3 = 
      seedMap.find(es);
    if (it3 != seedMap.end())
      seedMap.erase(it3);
    deleteState(es);
  }
  removedStates.clear();
}

void Executor::bindInstructionConstants(KInstruction *KI) {
  GetElementPtrInst *gepi = dyn_cast<GetElementPtrInst>(KI->inst);
  if (!gepi)
    return;

  KGEPInstruction *kgepi = static_cast<KGEPInstruction*>(KI);
  ref<ConstantExpr> constantOffset =
    ConstantExpr::alloc(0, Context::get().getPointerWidth());
  uint64_t index = 1;
  for (gep_type_iterator ii = gep_type_begin(gepi), ie = gep_type_end(gepi);
       ii != ie; ++ii) {
    if (StructType *st1 = dyn_cast<StructType>(*ii)) {
      const StructLayout *sl = kmodule->targetData->getStructLayout(st1);
      ConstantInt *ci = cast<ConstantInt>(ii.getOperand());
      uint64_t addend = sl->getElementOffset((unsigned) ci->getZExtValue());
      constantOffset = constantOffset->Add(ConstantExpr::alloc(addend,
                                                               Context::get().getPointerWidth()));
    } else {
      const SequentialType *st = cast<SequentialType>(*ii);
      uint64_t elementSize = 
        kmodule->targetData->getTypeStoreSize(st->getElementType());
      Value *operand = ii.getOperand();
      if (Constant *c = dyn_cast<Constant>(operand)) {
        ref<ConstantExpr> index = 
          evalConstant(c)->ZExt(Context::get().getPointerWidth());
        ref<ConstantExpr> addend = 
          index->Mul(ConstantExpr::alloc(elementSize,
                                         Context::get().getPointerWidth()));
        constantOffset = constantOffset->Add(addend);
      } else {
        kgepi->indices.push_back(std::make_pair(index, elementSize));
      }
    }
    index++;
  }
  kgepi->offset = constantOffset->getZExtValue();
}

void Executor::bindModuleConstants() {
  for (std::vector<KFunction*>::iterator it = kmodule->functions.begin(), 
         ie = kmodule->functions.end(); it != ie; ++it) {
    KFunction *kf = *it;
    for (unsigned i=0; i<kf->numInstructions; ++i)
      bindInstructionConstants(kf->instructions[i]);
  }

  kmodule->constantTable.resize(kmodule->constants.size());
  for (unsigned i=0; i<kmodule->constants.size(); ++i) {
    Cell &c = kmodule->constantTable[i];
    c.value = evalConstant(kmodule->constants[i]);
  }
}

void Executor::run(ExecutionState &initialState) {
  bindModuleConstants();

  // Delay init till now so that ticks don't accrue during
  // optimization and such.
  initTimers();

  states.insert(&initialState);

  if (usingSeeds) {
    std::vector<SeedInfo> &v = seedMap[&initialState];
    
    for (std::vector<KTest*>::const_iterator it = usingSeeds->begin(), 
           ie = usingSeeds->end(); it != ie; ++it)
      v.push_back(SeedInfo(*it));

    int lastNumSeeds = usingSeeds->size()+10;
    double lastTime, startTime = lastTime = util::getWallTime();
    ExecutionState *lastState = 0;
    while (!seedMap.empty()) {
      if (haltExecution) goto dump;

      std::map<ExecutionState*, std::vector<SeedInfo> >::iterator it = 
        seedMap.upper_bound(lastState);
      if (it == seedMap.end())
        it = seedMap.begin();
      lastState = it->first;
      unsigned numSeeds = it->second.size();
      ExecutionState &state = *lastState;
      KInstruction *ki = state.pc;
      stepInstruction(state);

      executeInstruction(state, ki);
      processTimers(&state, MaxInstructionTime * numSeeds);
      updateStates(&state);

      if ((stats::instructions % 1000) == 0) {
        int numSeeds = 0, numStates = 0;
        for (std::map<ExecutionState*, std::vector<SeedInfo> >::iterator
               it = seedMap.begin(), ie = seedMap.end();
             it != ie; ++it) {
          numSeeds += it->second.size();
          numStates++;
        }
        double time = util::getWallTime();
        if (SeedTime>0. && time > startTime + SeedTime) {
          klee_warning("seed time expired, %d seeds remain over %d states",
                       numSeeds, numStates);
          break;
        } else if (numSeeds<=lastNumSeeds-10 ||
                   time >= lastTime+10) {
          lastTime = time;
          lastNumSeeds = numSeeds;          
          klee_message("%d seeds remaining over: %d states", 
                       numSeeds, numStates);
        }
      }
    }

    klee_message("seeding done (%d states remain)", (int) states.size());

    // XXX total hack, just because I like non uniform better but want
    // seed results to be equally weighted.
    for (std::set<ExecutionState*>::iterator
           it = states.begin(), ie = states.end();
         it != ie; ++it) {
      (*it)->weight = 1.;
    }

    if (OnlySeed)
      goto dump;
  }

  searcher = constructUserSearcher(*this);

  searcher->update(0, states, std::set<ExecutionState*>());

  while (!states.empty() && !haltExecution) {
    ExecutionState &state = searcher->selectState();
    KInstruction *ki = state.pc;
    stepInstruction(state);

    executeInstruction(state, ki);
    processTimers(&state, MaxInstructionTime);

    if (MaxMemory) {
      if ((stats::instructions & 0xFFFF) == 0) {
        // We need to avoid calling GetMallocUsage() often because it
        // is O(elts on freelist). This is really bad since we start
        // to pummel the freelist once we hit the memory cap.
        unsigned mbs = sys::Process::GetTotalMemoryUsage() >> 20;
        
        if (mbs > MaxMemory) {
          if (mbs > MaxMemory + 100) {
            // just guess at how many to kill
            unsigned numStates = states.size();
            unsigned toKill = std::max(1U, numStates - numStates*MaxMemory/mbs);

            if (MaxMemoryInhibit)
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

    updateStates(&state);
  }

  delete searcher;
  searcher = 0;
  
 dump:
  if (DumpStatesOnHalt && !states.empty()) {
    llvm::errs() << "KLEE: halting execution, dumping remaining states\n";
    for (std::set<ExecutionState*>::iterator
           it = states.begin(), ie = states.end();
         it != ie; ++it) {
      ExecutionState &state = **it;
      stepInstruction(state); // keep stats rolling
      terminateStateEarly(state, "execution halting");
    }
    updateStates(0);
  }
}

std::string Executor::getAddressInfo(ExecutionState &state, 
                                     ref<Expr> address) const{
  std::ostringstream info;
  uint64_t example;
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(address)) {
    example = CE->getZExtValue();
    info << "\taddress: 0x" << std::hex << example << std::dec << "\n";
  } else {
    info << "\taddress: " << address << "\n";
    ref<ConstantExpr> value;
    assert(!concolicMode && "Not tested in concolic mode");
    bool success = solver->getValue(state, address, value);
    assert(success && "FIXME: Unhandled solver failure");
    (void) success;
    example = value->getZExtValue();
    info << "\texample: 0x" << std::hex << example << std::dec << "\n";
    std::pair< ref<Expr>, ref<Expr> > res = solver->getRange(state, address);
    info << "\trange: [0x" << std::hex << res.first << ", 0x"
         << res.second << std::dec << "]\n";
  }
  
  MemoryObject hack((unsigned) example);    
  MemoryMap::iterator lower = state.addressSpace.objects.upper_bound(&hack);
  info << "\tnext: ";
  if (lower==state.addressSpace.objects.end()) {
    info << "none\n";
  } else {
    const MemoryObject *mo = lower->first;
    std::string alloc_info;
    mo->getAllocInfo(alloc_info);
    info << "object at 0x" << std::hex << mo->address
         << " of size 0x" << mo->size << std::dec << "\n"
         << "\t\t" << alloc_info << std::dec << "\n";
  }
  if (lower!=state.addressSpace.objects.begin()) {
    --lower;
    info << "\tprev: ";
    if (lower==state.addressSpace.objects.end()) {
      info << "none\n";
    } else {
      const MemoryObject *mo = lower->first;
      std::string alloc_info;
      mo->getAllocInfo(alloc_info);
      info << "object at 0x" << std::hex << mo->address
           << " of size 0x" << mo->size << std::dec << "\n"
           << "\t\t" << alloc_info << std::dec << "\n";
    }
  }

  return info.str();
}

void Executor::deleteState(ExecutionState *state)
{
    processTree->remove(state->ptreeNode);
    delete state;
}

void Executor::terminateState(ExecutionState &state) {
  if (replayOut && replayPosition!=replayOut->numObjects) {
    klee_warning_once(replayOut, 
                      "replay did not consume all objects in test input.");
  }

  interpreterHandler->incPathsExplored();

  std::set<ExecutionState*>::iterator it = addedStates.find(&state);
  if (it==addedStates.end()) {
    // XXX: the following line makes delayed state termination impossible
    //state.pc = state.prevPC;

    removedStates.insert(&state);
  } else {
    // never reached searcher, just delete immediately
    std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it3 = 
      seedMap.find(&state);
    if (it3 != seedMap.end())
      seedMap.erase(it3);
    addedStates.erase(it);
    deleteState(&state);
  }
}

void Executor::terminateStateEarly(ExecutionState &state, 
                                   const Twine &message) {
  if (!OnlyOutputStatesCoveringNew || state.coveredNew ||
      (AlwaysOutputSeeds && seedMap.count(&state)))
    interpreterHandler->processTestCase(state, (message).str().c_str(),
                                        "early");
  terminateState(state);
}

void Executor::terminateStateOnExit(ExecutionState &state) {
  if (!OnlyOutputStatesCoveringNew || state.coveredNew || 
      (AlwaysOutputSeeds && seedMap.count(&state)))
    interpreterHandler->processTestCase(state, 0, 0);
  terminateState(state);
}

void Executor::printStack(ExecutionState &state, KInstruction *target, std::stringstream &msg)
{
    msg << "Stack: \n";
    unsigned idx = 0;
    for (ExecutionState::stack_ty::reverse_iterator
           it = state.stack.rbegin(), ie = state.stack.rend();
         it != ie; ++it) {
      StackFrame &sf = *it;
      Function *f = sf.kf->function;

      unsigned assemblyLine = 0;
      const InstructionInfo *ii = NULL;
      if (target) {
          ii = target->info;
          assemblyLine = ii->assemblyLine;
      }

      msg << "\t#" << idx++
          << " " << std::setw(8) << std::setfill('0') << assemblyLine
          << " in " << f->getName().str() << " (";

      // Yawn, we could go up and print varargs if we wanted to.
      unsigned index = 0;
      for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
           ai != ae; ++ai) {
        if (ai!=f->arg_begin()) msg << ", ";

        msg << ai->getName().str();
        // XXX should go through function
        ref<Expr> value = sf.locals[sf.kf->getArgRegister(index++)].value;
        //if (isa<ConstantExpr>(value))
        if (concolicMode) {
            msg << " [" << state.concolics.evaluate(value) << "]";
        } else {
            msg << "=" << value;
        }
      }
      msg << ")";

      if (ii) {
          if (ii->file != "")
            msg << " at " << ii->file << ":" << ii->line;
      }

      msg << "\n";

      target = sf.caller;
    }
}

void Executor::terminateStateOnError(ExecutionState &state,
                                     const llvm::Twine &messaget,
                                     const char *suffix,
                                     const llvm::Twine &info) {
  std::string message = messaget.str();
  static std::set< std::pair<Instruction*, std::string> > emittedErrors;
  const InstructionInfo &ii = *state.prevPC->info;
  
  if (EmitAllErrors ||
      emittedErrors.insert(std::make_pair(state.prevPC->inst, message)).second) {
    if (ii.file != "") {
      klee_message("ERROR: %s:%d: %s", ii.file.c_str(), ii.line, message.c_str());
    } else {
      klee_message("ERROR: %s", message.c_str());
    }
    if (!EmitAllErrors)
      klee_message("NOTE: now ignoring this error at this location");
    
    std::stringstream msg;
    msg << "Error: " << message << "\n";
    if (ii.file != "") {
      msg << "File: " << ii.file << "\n";
      msg << "Line: " << ii.line << "\n";
    }

    printStack(state, state.prevPC, msg);

    std::string info_str = info.str();
    if (info_str != "")
      msg << "Info: \n" << info_str;
    interpreterHandler->processTestCase(state, msg.str().c_str(), suffix);
  }
    
  terminateState(state);
}

// XXX shoot me
static const char *okExternalsList[] = { "printf", 
                                         "fprintf", 
                                         "puts",
                                         "getpid" };
static std::set<std::string> okExternals(okExternalsList,
                                         okExternalsList + 
                                         (sizeof(okExternalsList)/sizeof(okExternalsList[0])));

void Executor::callExternalFunction(ExecutionState &state,
                                    KInstruction *target,
                                    Function *function,
                                    std::vector< ref<Expr> > &arguments) {
  // check if specialFunctionHandler wants it
  if (specialFunctionHandler->handle(state, function, target, arguments))
    return;
  
  if (NoExternals && !okExternals.count(function->getName())) {
    llvm::errs() << "KLEE:ERROR: Calling not-OK external function : "
               << function->getName().str() << "\n";
    terminateStateOnError(state, "externals disallowed", "user.err");
    return;
  }

  // normal external function handling path
  uint64_t *args = (uint64_t*) alloca(sizeof(*args) * (arguments.size() + 1));
  memset(args, 0, sizeof(*args) * (arguments.size() + 1));

  unsigned i = 1;
  for (std::vector<ref<Expr> >::iterator ai = arguments.begin(), 
         ae = arguments.end(); ai!=ae; ++ai, ++i) {
    if (AllowExternalSymCalls) { // don't bother checking uniqueness
      ref<ConstantExpr> ce;
      assert(!concolicMode && "Not tested in concolic mode");
      bool success = solver->getValue(state, *ai, ce);
      assert(success && "FIXME: Unhandled solver failure");
      (void) success;
      static_cast<ConstantExpr*>(ce.get())->toMemory((void*) &args[i]);
    } else {
      ref<Expr> arg = toUnique(state, *ai);
      if (ConstantExpr *CE = dyn_cast<ConstantExpr>(arg)) {
        // XXX kick toMemory functions from here
        CE->toMemory((void*) &args[i]);
      } else {
          std::stringstream ss;
           ss << "external call with symbolic argument " << std::dec <<
                  i << ": " << function->getName().str() << " - ";
            ss << arg;
            unsigned j=0;
            ss << "[";
            for (std::vector<ref<Expr> >::iterator ai1 = arguments.begin();
                   ai1!=ae; ++ai1, ++j) {
                ss << "arg" << j << ": " << *ai1 << '\n';
            }
            ss << "]";

            terminateStateOnExecError(state, ss.str());
        return;
      }
    }
  }

  copyOutConcretes(state);

  if (!SuppressExternalWarnings) {
    std::ostringstream os;
    os << "calling external: " << function->getName().str() << "(";
    for (unsigned i=0; i<arguments.size(); i++) {
        os << std::hex << arguments[i];
      if (i != arguments.size()-1)
	os << ", ";
    }
    os << ")" << std::dec;
    
    klee_warning_external(function, "%s", os.str().c_str());
  }
  
  bool success = externalDispatcher->executeCall(function, target->inst, args);
  if (!success) {
    terminateStateOnError(state, "failed external call: " + function->getName(),
                          "external.err");
    return;
  }

  if (!copyInConcretes(state)) {
    terminateStateOnError(state, "external modified read-only object",
                          "external.err");
    return;
  }

  Type *resultType = target->inst->getType();
  if (resultType != Type::getVoidTy(getGlobalContext())) {
    ref<Expr> e = ConstantExpr::fromMemory((void*) args, 
                                           getWidthForLLVMType(resultType));
    bindLocal(target, state, e);
  }
}

/***/

ref<Expr> Executor::replaceReadWithSymbolic(ExecutionState &state, 
                                            ref<Expr> e) {
  unsigned n = interpreterOpts.MakeConcreteSymbolic;
  if (!n || replayOut || replayPath)
    return e;

  // right now, we don't replace symbolics (is there any reason too?)
  if (!isa<ConstantExpr>(e))
    return e;

#ifdef __MINGW32__
  if (n != 1 && rand() %  n)
    return e;
#else
  if (n != 1 && random() %  n)
    return e;
#endif

  // create a new fresh location, assert it is equal to concrete value in e
  // and return it.
  
  static unsigned id;
  const Array *array = new Array("rrws_arr" + llvm::utostr(++id), 
                                 Expr::getMinBytesForWidth(e->getWidth()));
  ref<Expr> res = Expr::createTempRead(array, e->getWidth());
  ref<Expr> eq = NotOptimizedExpr::create(EqExpr::create(e, res));
  llvm::errs() << "Making symbolic: " << eq << "\n";
  state.addConstraint(eq);
  return res;
}

ObjectState *Executor::bindObjectInState(ExecutionState &state, 
                                         const MemoryObject *mo,
                                         bool isLocal,
                                         const Array *array) {
  ObjectState *os = array ? new ObjectState(mo, array) : new ObjectState(mo);
  state.addressSpace.bindObject(mo, os);

  // Its possible that multiple bindings of the same mo in the state
  // will put multiple copies on this list, but it doesn't really
  // matter because all we use this list for is to unbind the object
  // on function return.
  if (isLocal)
    state.stack.back().allocas.push_back(mo);

  return os;
}

void Executor::executeAlloc(ExecutionState &state,
                            ref<Expr> size,
                            bool isLocal,
                            KInstruction *target,
                            bool zeroMemory,
                            const ObjectState *reallocFrom) {
  size = toUnique(state, size);
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(size)) {
    MemoryObject *mo = memory->allocate(CE->getZExtValue(), isLocal, false, 
                                        state.prevPC->inst);
    if (!mo) {
      bindLocal(target, state, 
                ConstantExpr::alloc(0, Context::get().getPointerWidth()));
    } else {
      ObjectState *os = bindObjectInState(state, mo, isLocal);
      if (zeroMemory) {
        os->initializeToZero();
      } else {
        os->initializeToRandom();
      }
      bindLocal(target, state, mo->getBaseExpr());
      
      if (reallocFrom) {
        unsigned count = std::min(reallocFrom->size, os->size);
        for (unsigned i=0; i<count; i++)
          os->write(i, reallocFrom->read8(i));
        state.addressSpace.unbindObject(reallocFrom->getObject());
      }
    }
  } else {
    // XXX For now we just pick a size. Ideally we would support
    // symbolic sizes fully but even if we don't it would be better to
    // "smartly" pick a value, for example we could fork and pick the
    // min and max values and perhaps some intermediate (reasonable
    // value).
    // 
    // It would also be nice to recognize the case when size has
    // exactly two values and just fork (but we need to get rid of
    // return argument first). This shows up in pcre when llvm
    // collapses the size expression with a select.

    ref<ConstantExpr> example;
    assert(!concolicMode && "Not tested in concolic mode");
    bool success = solver->getValue(state, size, example);
    assert(success && "FIXME: Unhandled solver failure");
    (void) success;
    
    // Try and start with a small example.
    Expr::Width W = example->getWidth();
    while (example->Ugt(ConstantExpr::alloc(128, W))->isTrue()) {
      ref<ConstantExpr> tmp = example->LShr(ConstantExpr::alloc(1, W));
      bool res;
      bool success = solver->mayBeTrue(state, EqExpr::create(tmp, size), res);
      assert(success && "FIXME: Unhandled solver failure");      
      (void) success;
      if (!res)
        break;
      example = tmp;
    }

    ref<Expr> cond = EqExpr::create(example, size);
    StatePair fixedSize = fork(state, cond, true);
    notifyFork(state, cond, fixedSize);

    if (fixedSize.second) { 
      // Check for exactly two values
      ref<ConstantExpr> tmp;
      bool success = solver->getValue(*fixedSize.second, size, tmp);
      assert(success && "FIXME: Unhandled solver failure");      
      (void) success;
      bool res;
      success = solver->mustBeTrue(*fixedSize.second, 
                                   EqExpr::create(tmp, size),
                                   res);
      assert(success && "FIXME: Unhandled solver failure");      
      (void) success;
      if (res) {
        executeAlloc(*fixedSize.second, tmp, isLocal,
                     target, zeroMemory, reallocFrom);
      } else {
        // See if a *really* big value is possible. If so assume
        // malloc will fail for it, so lets fork and return 0.
        StatePair hugeSize = 
          fork(*fixedSize.second, 
               UltExpr::create(ConstantExpr::alloc(1<<31, W), size), 
               true);
        if (hugeSize.first) {
          klee_message("NOTE: found huge malloc, returing 0");
          bindLocal(target, *hugeSize.first, 
                    ConstantExpr::alloc(0, Context::get().getPointerWidth()));
        }
        
        if (hugeSize.second) {
          std::string ss;
            llvm::raw_string_ostream info(ss);
          ExprPPrinter::printOne(info, "  size expr", size);
          info << "  concretization : " << example << "\n";
          info << "  unbound example: " << tmp << "\n";
          terminateStateOnError(*hugeSize.second, 
                                "concretized symbolic size", 
                                "model.err", 
                                info.str());
        }
      }
    }

    if (fixedSize.first) // can be zero when fork fails
      executeAlloc(*fixedSize.first, example, isLocal, 
                   target, zeroMemory, reallocFrom);
  }
}

void Executor::executeFree(ExecutionState &state,
                           ref<Expr> address,
                           KInstruction *target) {
  ref<Expr> cond = Expr::createIsZero(address);
  StatePair zeroPointer = fork(state, cond, true);

  if (zeroPointer.first) {
    if (target)
      bindLocal(target, *zeroPointer.first, Expr::createPointer(0));
  }
  if (zeroPointer.second) { // address != 0
    ExactResolutionList rl;
    resolveExact(*zeroPointer.second, address, rl, "free");
    
    for (Executor::ExactResolutionList::iterator it = rl.begin(), 
           ie = rl.end(); it != ie; ++it) {
      const MemoryObject *mo = it->first.first;
      if (mo->isLocal) {
        terminateStateOnError(*it->second, 
                              "free of alloca", 
                              "free.err",
                              getAddressInfo(*it->second, address));
      } else if (mo->isGlobal) {
        terminateStateOnError(*it->second, 
                              "free of global", 
                              "free.err",
                              getAddressInfo(*it->second, address));
      } else {
        it->second->addressSpace.unbindObject(mo);
        if (target)
          bindLocal(target, *it->second, Expr::createPointer(0));
      }
    }
  }


  notifyFork(state, cond, zeroPointer);
}

void Executor::resolveExact(ExecutionState &state,
                            ref<Expr> p,
                            ExactResolutionList &results, 
                            const std::string &name) {
  p = simplifyExpr(state, p);
  // XXX we may want to be capping this?
  ResolutionList rl;
  state.addressSpace.resolve(state, solver, p, rl);
  
  ExecutionState *unbound = &state;
  for (ResolutionList::iterator it = rl.begin(), ie = rl.end(); 
       it != ie; ++it) {
    ref<Expr> inBounds = EqExpr::create(p, it->first->getBaseExpr());
    
    StatePair branches = fork(*unbound, inBounds, true);
    notifyFork(*unbound, inBounds, branches);
    
    if (branches.first)
      results.push_back(std::make_pair(*it, branches.first));

    unbound = branches.second;
    if (!unbound) // Fork failure
      break;
  }

  if (unbound) {
    terminateStateOnError(*unbound,
                          "memory error: invalid pointer: " + name,
                          "ptr.err",
                          getAddressInfo(*unbound, p));
  }
}

void Executor::executeMemoryOperation(ExecutionState &state,
                                      bool isWrite,
                                      ref<Expr> address,
                                      ref<Expr> value /* undef if read */,
                                      KInstruction *target /* undef if write */) {
  Expr::Width type = (isWrite ? value->getWidth() : 
                     getWidthForLLVMType(target->inst->getType()));
  unsigned bytes = Expr::getMinBytesForWidth(type);

  if (SimplifySymIndices) {
    if (!isa<ConstantExpr>(address))
      address = state.constraints.simplifyExpr(address);
    if (isWrite && !isa<ConstantExpr>(value))
      value = state.constraints.simplifyExpr(value);
  }

  // fast path: single in-bounds resolution
  ObjectPair op;
  bool success;
  bool fastInBounds = false;

  //Fast pattern-matching of addresses
  //Avoids calling the constraint solver for simple cases
  success = state.addressSpace.resolveOneFast(*exprSimplifier, address, type, op, &fastInBounds);

  if (!success || !fastInBounds) {
      if (concolicMode) {
          klee::ref<klee::Expr> concreteAddress;
          concreteAddress = state.concolics.evaluate(address);
          assert(dyn_cast<klee::ConstantExpr>(concreteAddress) && "Could not evaluate address");

          //Retrieve the memory object for that concrete address
          success = state.addressSpace.resolveOneFast(*exprSimplifier, concreteAddress, type, op, &fastInBounds);
          assert(success && "Could not resolve concrete memory address");

          assert(fastInBounds && "Memory access cannot span multiple objects here");

          //We need to keep the address symbolic to avoid blowup.
          //For that, add a constraint that will ensure that next time, we get a different memory object
          klee::ref<klee::Expr> condition =
                  AndExpr::create(UgeExpr::create(address, op.first->getBaseExpr()),
                                  UltExpr::create(address, AddExpr::create(op.first->getBaseExpr(), op.first->getSizeExpr())));

          StatePair branches = fork(state, condition, true);
          assert(branches.first == &state);
          if (branches.second) {
            //The forked state will have to re-execute the memory op
            branches.second->pc = branches.second->prevPC;
          }

          notifyFork(state, condition, branches);

      } else {
          solver->setTimeout(stpTimeout);
          if (!state.addressSpace.resolveOne(state, solver, address, op, success)) {
            address = toConstant(state, address, "resolveOne failure");
            success = state.addressSpace.resolveOne(cast<ConstantExpr>(address), op);
          }
          solver->setTimeout(0);
      }
  }

  if (success) {
    const MemoryObject *mo = op.first;

    if (MaxSymArraySize && mo->size>=MaxSymArraySize) {
      address = toConstant(state, address, "max-sym-array-size");
    }
    
    ref<Expr> offset = mo->getOffsetExpr(address);

    bool inBounds, success;

    if (!fastInBounds) {
        solver->setTimeout(stpTimeout);
        success = solver->mustBeTrue(state,
                                      mo->getBoundsCheckOffset(offset, bytes),
                                      inBounds);
        solver->setTimeout(0);
    } else {
        inBounds = true;
        success = true;
    }

    if (!success) {
      state.pc = state.prevPC;
      std::stringstream ss;
      ss << "Query timed out on symbolic address " << std::hex << address <<
              " - offset " << offset;
      terminateStateEarly(state, ss.str());
      return;
    }

    if (inBounds) {
      const ObjectState *os = op.second;
      if (isWrite) {
        if (os->readOnly) {
          terminateStateOnError(state,
                                "memory error: object read only",
                                "readonly.err");
        } else {
          ObjectState *wos = state.addressSpace.getWriteable(mo, os);
          if(mo->isSharedConcrete) {
              if (!dyn_cast<ConstantExpr>(offset) || !dyn_cast<ConstantExpr>(value)) {
                  if(mo->isValueIgnored) {
                      offset = toConstantSilent(state, offset);
                      value  = toConstantSilent(state,  value);
                  } else {
                      std::stringstream ss;
                      ss << "write to always concrete memory name:" << mo->name <<
                              " offset=" << offset << " value=" << value;

                      offset = toConstant(state, offset, ss.str().c_str());
                      value  = toConstant(state,  value, ss.str().c_str());
                  }
              }
          }
          wos->write(offset, value);
        }          
      } else {
        if(mo->isSharedConcrete) {
            if (!dyn_cast<ConstantExpr>(offset)) {
                if(mo->isValueIgnored) {
                    offset = toConstantSilent(state, offset);
                } else {
                    std::stringstream ss;
                    ss << "Read from always concrete memory name:" << mo->name <<
                            " offset=" << offset;

                    offset = toConstant(state, offset, ss.str().c_str());
                }
            }
        }
        ref<Expr> result = os->read(offset, type);
        
        if (interpreterOpts.MakeConcreteSymbolic)
          result = replaceReadWithSymbolic(state, result);
        
        bindLocal(target, state, result);
      }
      return;
    }
  } 

  // we are on an error path (no resolution, multiple resolution, one
  // resolution with out of bounds)
  
  ResolutionList rl;  
  solver->setTimeout(stpTimeout);
  bool incomplete = state.addressSpace.resolve(state, solver, address, rl,
                                               0, stpTimeout);
  solver->setTimeout(0);
  
  // XXX there is some query wasteage here. who cares?
  ExecutionState *unbound = &state;
  
  for (ResolutionList::iterator i = rl.begin(), ie = rl.end(); i != ie; ++i) {
    const MemoryObject *mo = i->first;
    const ObjectState *os = i->second;
    ref<Expr> inBounds = mo->getBoundsCheckPointer(address, bytes);
    
    StatePair branches = fork(*unbound, inBounds, true);
    ExecutionState *bound = branches.first;

    // bound can be 0 on failure or overlapped 
    if (bound) {
      if (isWrite) {
        if (os->readOnly) {
          terminateStateOnError(*bound,
                                "memory error: object read only",
                                "readonly.err");
        } else {
          ObjectState *wos = bound->addressSpace.getWriteable(mo, os);
          ref<Expr> offset = mo->getOffsetExpr(address);
          if(mo->isSharedConcrete) {
              if(mo->isValueIgnored) {
                  offset = toConstantSilent(state, offset);
                  value  = toConstantSilent(state,  value);
              } else {
                  offset = toConstant(state, offset, "write to always concrete memory");
                  value  = toConstant(state,  value, "write to always concrete memory");
              }
          }
          wos->write(offset, value);
        }
      } else {
        ref<Expr> offset = mo->getOffsetExpr(address);
        if(mo->isSharedConcrete) {
            if(mo->isValueIgnored) {
                offset = toConstantSilent(state, offset);
            } else {
                offset = toConstant(state, offset, "read from always concrete memory");
            }
        }
        ref<Expr> result = os->read(offset, type);
        bindLocal(target, *bound, result);
      }
    }

    notifyFork(state, inBounds, branches);

    unbound = branches.second;
    if (!unbound)
      break;
  }
  
  // XXX should we distinguish out of bounds and overlapped cases?
  if (unbound) {
    if (incomplete) {
      terminateStateEarly(*unbound, "query timed out (resolve)");
    } else {
      terminateStateOnError(*unbound,
                            "memory error: out of bound pointer",
                            "ptr.err",
                            getAddressInfo(*unbound, address));
    }
  }
}

void Executor::executeMakeSymbolic(ExecutionState &state, 
                                   const MemoryObject *mo) {
  // Create a new object state for the memory object (instead of a copy).
  if (!replayOut) {
    static unsigned id = 0;
    const Array *array = new Array("arr" + llvm::utostr(++id) + mo->name,
                                   mo->size);
    bindObjectInState(state, mo, false, array);
    state.addSymbolic(mo, array);
    
    std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it = 
      seedMap.find(&state);
    if (it!=seedMap.end()) { // In seed mode we need to add this as a
                             // binding.
      for (std::vector<SeedInfo>::iterator siit = it->second.begin(), 
             siie = it->second.end(); siit != siie; ++siit) {
        SeedInfo &si = *siit;
        KTestObject *obj = si.getNextInput(mo, NamedSeedMatching);

        if (!obj) {
          if (ZeroSeedExtension) {
            std::vector<unsigned char> &values = si.assignment.bindings[array];
            values = std::vector<unsigned char>(mo->size, '\0');
          } else if (!AllowSeedExtension) {
            terminateStateOnError(state, 
                                  "ran out of inputs during seeding",
                                  "user.err");
            break;
          }
        } else {
          if (obj->numBytes != mo->size &&
              ((!(AllowSeedExtension || ZeroSeedExtension)
                && obj->numBytes < mo->size) ||
               (!AllowSeedTruncation && obj->numBytes > mo->size))) {
	    std::stringstream msg;
	    msg << "replace size mismatch: "
		<< mo->name << "[" << mo->size << "]"
		<< " vs " << obj->name << "[" << obj->numBytes << "]"
		<< " in test\n";

            terminateStateOnError(state,
                                  msg.str(),
                                  "user.err");
            break;
          } else {
            std::vector<unsigned char> &values = si.assignment.bindings[array];
            values.insert(values.begin(), obj->bytes, 
                          obj->bytes + std::min(obj->numBytes, mo->size));
            if (ZeroSeedExtension) {
              for (unsigned i=obj->numBytes; i<mo->size; ++i)
                values.push_back('\0');
            }
          }
        }
      }
    }
  } else {
    ObjectState *os = bindObjectInState(state, mo, false);
    if (replayPosition >= replayOut->numObjects) {
      terminateStateOnError(state, "replay count mismatch", "user.err");
    } else {
      KTestObject *obj = &replayOut->objects[replayPosition++];
      if (obj->numBytes != mo->size) {
        terminateStateOnError(state, "replay size mismatch", "user.err");
      } else {
        for (unsigned i=0; i<mo->size; i++)
          os->write8(i, obj->bytes[i]);
      }
    }
  }
}

/***/

void Executor::runFunctionAsMain(Function *f,
				 int argc,
				 char **argv,
				 char **envp) {
  std::vector<ref<Expr> > arguments;

  // force deterministic initialization of memory objects
  srand(1);
#ifndef __MINGW32__
  srandom(1);
#endif
  
  MemoryObject *argvMO = 0;

  // In order to make uclibc happy and be closer to what the system is
  // doing we lay out the environments at the end of the argv array
  // (both are terminated by a null). There is also a final terminating
  // null that uclibc seems to expect, possibly the ELF header?

  int envc;
  for (envc=0; envp[envc]; ++envc) ;

  unsigned NumPtrBytes = Context::get().getPointerWidth() / 8;
  KFunction *kf = kmodule->functionMap[f];
  assert(kf);
  Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
  if (ai!=ae) {
    arguments.push_back(ConstantExpr::alloc(argc, Expr::Int32));

    if (++ai!=ae) {
      argvMO = memory->allocate((argc+1+envc+1+1) * NumPtrBytes, false, true,
                                f->begin()->begin());
      
      arguments.push_back(argvMO->getBaseExpr());

      if (++ai!=ae) {
        uint64_t envp_start = argvMO->address + (argc+1)*NumPtrBytes;
        arguments.push_back(Expr::createPointer(envp_start));

        if (++ai!=ae)
          klee_error("invalid main function (expect 0-3 arguments)");
      }
    }
  }

  ExecutionState *state = new ExecutionState(kmodule->functionMap[f]);
  
  if (pathWriter) 
    state->pathOS = pathWriter->open();
  if (symPathWriter) 
    state->symPathOS = symPathWriter->open();


  if (statsTracker)
    statsTracker->framePushed(*state, 0);

  assert(arguments.size() == f->arg_size() && "wrong number of arguments");
  for (unsigned i = 0, e = f->arg_size(); i != e; ++i)
    bindArgument(kf, i, *state, arguments[i]);

  if (argvMO) {
    ObjectState *argvOS = bindObjectInState(*state, argvMO, false);

    for (int i=0; i<argc+1+envc+1+1; i++) {
      MemoryObject *arg;
      
      if (i==argc || i>=argc+1+envc) {
        arg = 0;
      } else {
        char *s = i<argc ? argv[i] : envp[i-(argc+1)];
        int j, len = strlen(s);
        
        arg = memory->allocate(len+1, false, true, state->pc->inst);
        ObjectState *os = bindObjectInState(*state, arg, false);
        for (j=0; j<len+1; j++)
          os->write8(j, s[j]);
      }

      if (arg) {
        argvOS->write(i * NumPtrBytes, arg->getBaseExpr());
      } else {
        argvOS->write(i * NumPtrBytes, Expr::createPointer(0));
      }
    }
  }
  
  initializeGlobals(*state);

  processTree = new PTree(state);
  state->ptreeNode = processTree->root;
  run(*state);
  delete processTree;
  processTree = 0;

  // hack to clear memory objects
  delete memory;
  memory = new MemoryManager();
  
  globalObjects.clear();
  globalAddresses.clear();

  if (statsTracker)
    statsTracker->done();

  if (theMMap) {
#ifdef __MINGW32__
    assert(false && "cannot munmap on windows");
#else
    munmap(theMMap, theMMapSize);
    theMMap = 0;
#endif
  }
}

unsigned Executor::getPathStreamID(const ExecutionState &state) {
  assert(pathWriter);
  return state.pathOS.getID();
}

unsigned Executor::getSymbolicPathStreamID(const ExecutionState &state) {
  assert(symPathWriter);
  return state.symPathOS.getID();
}

void Executor::getConstraintLog(const ExecutionState &state,
                                std::string &res,
                                bool asCVC) {
    if (asCVC) {
        Query query(state.constraints, ConstantExpr::alloc(0, Expr::Bool));
        char *log = solver->stpSolver->getConstraintLog(query);
        res = std::string(log);
        free(log);
    } else {
        std::string ss;
        llvm::raw_string_ostream info(ss);
        ExprPPrinter::printConstraints(info, state.constraints);
        res = info.str();
    }
}

bool Executor::getSymbolicSolution(const ExecutionState &state,
                                   std::vector< 
                                   std::pair<std::string,
                                   std::vector<unsigned char> > >
                                   &res) {
  solver->setTimeout(stpTimeout);

  ExecutionState tmp(state);
  tmp.addressSpace.state = &tmp;
  if (!NoPreferCex) {
    for (unsigned i = 0; i != state.symbolics.size(); ++i) {
      const MemoryObject *mo = state.symbolics[i].first;
      std::vector< ref<Expr> >::const_iterator pi = 
        mo->cexPreferences.begin(), pie = mo->cexPreferences.end();
      for (; pi != pie; ++pi) {
        bool mustBeTrue;
        bool success = solver->mustBeTrue(tmp, Expr::createIsZero(*pi), 
                                          mustBeTrue);
        if (!success) break;
        if (!mustBeTrue) tmp.addConstraint(*pi);
      }
      if (pi!=pie) break;
    }
  }

  std::vector< std::vector<unsigned char> > values;
  std::vector<const Array*> objects;
  for (unsigned i = 0; i != state.symbolics.size(); ++i)
    objects.push_back(state.symbolics[i].second);
  bool success = solver->getInitialValues(tmp, objects, values);
  solver->setTimeout(0);
  if (!success) {
    klee_warning("unable to compute initial values (invalid constraints?)!");
    ExprPPrinter::printQuery(llvm::errs(),
                             state.constraints, 
                             ConstantExpr::alloc(0, Expr::Bool));
    return false;
  }
  
  for (unsigned i = 0; i != state.symbolics.size(); ++i)
    res.push_back(std::make_pair(state.symbolics[i].first->name, values[i]));
  return true;
}

void Executor::getCoveredLines(const ExecutionState &state,
                               std::map<const std::string*, std::set<unsigned> > &res) {
  res = state.coveredLines;
}

void Executor::doImpliedValueConcretization(ExecutionState &state,
                                            ref<Expr> e,
                                            ref<ConstantExpr> value) {
  abort(); // FIXME: Broken until we sort out how to do the write back.

  if (DebugCheckForImpliedValues)
    ImpliedValue::checkForImpliedValues(solver->solver, e, value);

  ImpliedValueList results;
  ImpliedValue::getImpliedValues(e, value, results);
  for (ImpliedValueList::iterator it = results.begin(), ie = results.end();
       it != ie; ++it) {
    ReadExpr *re = it->first.get();
    
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(re->index)) {
      // FIXME: This is the sole remaining usage of the Array object
      // variable. Kill me.
      const MemoryObject *mo = 0; //re->updates.root->object;
      const ObjectState *os = state.addressSpace.findObject(mo);

      if (!os) {
        // object has been free'd, no need to concretize (although as
        // in other cases we would like to concretize the outstanding
        // reads, but we have no facility for that yet)
      } else {
        assert(!os->readOnly && 
               "not possible? read only object with static read?");
        ObjectState *wos = state.addressSpace.getWriteable(mo, os);
        wos->write(CE, it->second);
      }
    }
  }
}

void Executor::copyOutConcretes(ExecutionState &state)
{
    state.addressSpace.copyOutConcretes();
}

bool Executor::copyInConcretes(ExecutionState &state)
{
    return state.addressSpace.copyInConcretes();
}

void Executor::addSpecialFunctionHandler(Function* function,
                                         FunctionHandler handler)
{
    specialFunctionHandler->addUHandler(function, handler);
}

Solver *Executor::getSolver() const
{
    return solver->solver;
}

Expr::Width Executor::getWidthForLLVMType(llvm::Type *type) const {
  return kmodule->targetData->getTypeSizeInBits(type);
}

