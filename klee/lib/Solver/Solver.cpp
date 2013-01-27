//===-- Solver.cpp --------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Common.h"
#include "klee/Solver.h"
#include "klee/SolverImpl.h"

#include "klee/SolverStats.h"
#include "STPBuilder.h"

#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/TimerStatIncrementer.h"
#include "klee/util/Assignment.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"
#include "klee/Internal/Support/Timer.h"
#include "llvm/Support/CommandLine.h"

#define vc_bvBoolExtract IAMTHESPAWNOFSATAN

#include <cassert>
#include <cstdio>
#include <map>
#include <vector>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#ifndef __MINGW32__
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

using namespace klee;

namespace {
  llvm::cl::opt<bool>
  ReinstantiateSolver("reinstantiate-solver",
                      llvm::cl::init(false));
}

/***/

const char *Solver::validity_to_str(Validity v) {
  switch (v) {
  default:    return "Unknown";
  case True:  return "True";
  case False: return "False";
  }
}

Solver::~Solver() {
  delete impl;
}

SolverImpl::~SolverImpl() {
}

bool Solver::evaluate(const Query& query, Validity &result) {
  assert(query.expr->getWidth() == Expr::Bool && "Invalid expression type!");

  // Maintain invariants implementations expect.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(query.expr)) {
    result = CE->isTrue() ? True : False;
    return true;
  }

  return impl->computeValidity(query, result);
}

bool SolverImpl::computeValidity(const Query& query, Solver::Validity &result) {
  bool isTrue, isFalse;
  if (!computeTruth(query, isTrue))
    return false;
  if (isTrue) {
    result = Solver::True;
  } else {
    if (!computeTruth(query.negateExpr(), isFalse))
      return false;
    result = isFalse ? Solver::False : Solver::Unknown;
  }
  return true;
}

bool Solver::mustBeTrue(const Query& query, bool &result) {
  assert(query.expr->getWidth() == Expr::Bool && "Invalid expression type!");

  // Maintain invariants implementations expect.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(query.expr)) {
    result = CE->isTrue() ? true : false;
    return true;
  }

  return impl->computeTruth(query, result);
}

bool Solver::mustBeFalse(const Query& query, bool &result) {
  return mustBeTrue(query.negateExpr(), result);
}

bool Solver::mayBeTrue(const Query& query, bool &result) {
  bool res;
  if (!mustBeFalse(query, res))
    return false;
  result = !res;
  return true;
}

bool Solver::mayBeFalse(const Query& query, bool &result) {
  bool res;
  if (!mustBeTrue(query, res))
    return false;
  result = !res;
  return true;
}

bool Solver::getValue(const Query& query, ref<ConstantExpr> &result) {
  // Maintain invariants implementation expect.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(query.expr)) {
    result = CE;
    return true;
  }

  // FIXME: Push ConstantExpr requirement down.
  ref<Expr> tmp;
  if (!impl->computeValue(query, tmp))
    return false;

  result = cast<ConstantExpr>(tmp);
  return true;
}

bool
Solver::getInitialValues(const Query& query,
                         const std::vector<const Array*> &objects,
                         std::vector< std::vector<unsigned char> > &values) {
  bool hasSolution;
  bool success =
    impl->computeInitialValues(query, objects, values, hasSolution);
  // FIXME: Propogate this out.
  if (!hasSolution)
    return false;

  return success;
}

std::pair< ref<Expr>, ref<Expr> > Solver::getRange(const Query& query) {
  ref<Expr> e = query.expr;
  Expr::Width width = e->getWidth();
  uint64_t min, max;

  if (width==1) {
    Solver::Validity result;
    if (!evaluate(query, result))
      assert(0 && "computeValidity failed");
    switch (result) {
    case Solver::True:
      min = max = 1; break;
    case Solver::False:
      min = max = 0; break;
    default:
      min = 0, max = 1; break;
    }
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    min = max = CE->getZExtValue();
  } else {
    // binary search for # of useful bits
    uint64_t lo=0, hi=width, mid, bits=0;
    while (lo<hi) {
      mid = lo + (hi - lo)/2;
      bool res;
      bool success =
        mustBeTrue(query.withExpr(
                     EqExpr::create(LShrExpr::create(e,
                                                     ConstantExpr::create(mid,
                                                                          width)),
                                    ConstantExpr::create(0, width))),
                   res);

      assert(success && "FIXME: Unhandled solver failure");
      (void) success;

      if (res) {
        hi = mid;
      } else {
        lo = mid+1;
      }

      bits = lo;
    }

    // could binary search for training zeros and offset
    // min max but unlikely to be very useful

    // check common case
    bool res = false;
    bool success =
      mayBeTrue(query.withExpr(EqExpr::create(e, ConstantExpr::create(0,
                                                                      width))),
                res);

    assert(success && "FIXME: Unhandled solver failure");
    (void) success;

    if (res) {
      min = 0;
    } else {
      // binary search for min
      lo=0, hi=bits64::maxValueOfNBits(bits);
      while (lo<hi) {
        mid = lo + (hi - lo)/2;
        bool res = false;
        bool success =
          mayBeTrue(query.withExpr(UleExpr::create(e,
                                                   ConstantExpr::create(mid,
                                                                        width))),
                    res);

        assert(success && "FIXME: Unhandled solver failure");
        (void) success;

        if (res) {
          hi = mid;
        } else {
          lo = mid+1;
        }
      }

      min = lo;
    }

    // binary search for max
    lo=min, hi=bits64::maxValueOfNBits(bits);
    while (lo<hi) {
      mid = lo + (hi - lo)/2;
      bool res;
      bool success =
        mustBeTrue(query.withExpr(UleExpr::create(e,
                                                  ConstantExpr::create(mid,
                                                                       width))),
                   res);

      assert(success && "FIXME: Unhandled solver failure");
      (void) success;

      if (res) {
        hi = mid;
      } else {
        lo = mid+1;
      }
    }

    max = lo;
  }

  return std::make_pair(ConstantExpr::create(min, width),
                        ConstantExpr::create(max, width));
}

/***/

class ValidatingSolver : public SolverImpl {
private:
  Solver *solver, *oracle;

public:
  ValidatingSolver(Solver *_solver, Solver *_oracle)
    : solver(_solver), oracle(_oracle) {}
  ~ValidatingSolver() { delete solver; }

  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeTruth(const Query&, bool &isValid);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query&,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};

bool ValidatingSolver::computeTruth(const Query& query,
                                    bool &isValid) {
#define VOTING_SOLVER
#define VOTE_COUNT 3
#if defined(VOTING_SOLVER)
    bool results[VOTE_COUNT];
    unsigned trueCount = 0, falseCount = 0;

    for (unsigned i=0; i<VOTE_COUNT; ++i) {
        bool res1, res2;
        if (!oracle->impl->computeTruth(query, res1))
          return false;

        if (!solver->impl->computeTruth(query, res2))
          return false;

	if (res1 == res2)
          results[i] = res1;
        else
          results[i] = rand() & 1 ? res1:res2;

        if (results[i])
            ++trueCount;
        else
            ++falseCount;
    }

    if (trueCount > falseCount) {
        isValid =  true;
    }else {
        isValid = false;
    }
    return true;
#else
    bool answer;

  if (!solver->impl->computeTruth(query, isValid))
    return false;
  if (!oracle->impl->computeTruth(query, answer))
    return false;

  if (isValid != answer)
    assert(0 && "invalid solver result (computeTruth)");

  return true;
#endif


}

bool ValidatingSolver::computeValidity(const Query& query,
                                       Solver::Validity &result) {
#if defined(VOTING_SOLVER)
    Solver::Validity results[VOTE_COUNT];
    unsigned trueCount = 0, falseCount = 0, unknownCount = 0;
    for (unsigned i=0; i<VOTE_COUNT; ++i) {
        Solver::Validity res1, res2;
        if (!solver->impl->computeValidity(query, res1))
          return false;

        if (!oracle->impl->computeValidity(query, res2))
          return false;

        if (res1 == res2)
          results[i] = res1;
        else
          results[i] = res1;

        switch(results[i]) {
            case Solver::True: ++trueCount; break;
            case Solver::False: ++falseCount; break;
            case Solver::Unknown: ++unknownCount; break;
            default: assert(false);
        }
    }
    if (trueCount > falseCount && falseCount >= unknownCount)
        result = Solver::True;
    else
    if (trueCount > unknownCount && unknownCount >= falseCount)
        result = Solver::True;
    else
    if (falseCount > trueCount && trueCount >= unknownCount)
        result = Solver::False;
    else
    if (falseCount > unknownCount && unknownCount >= trueCount)
        result = Solver::False;
    else
    if (unknownCount > falseCount && falseCount >= trueCount)
        result = Solver::Unknown;
    else
    if (unknownCount > trueCount && trueCount >= falseCount)
        result = Solver::Unknown;
    else assert(false);
    return true;
#else
    Solver::Validity answer;

  if (!solver->impl->computeValidity(query, result))
    return false;
  if (!oracle->impl->computeValidity(query, answer))
    return false;

  if (result != answer)
    assert(0 && "invalid solver result (computeValidity)");

  return true;
#endif
}

bool ValidatingSolver::computeValue(const Query& query,
                                    ref<Expr> &result) {
  bool answer;

  if (!solver->impl->computeValue(query, result))
    return false;
  // We don't want to compare, but just make sure this is a legal
  // solution.
  if (!oracle->impl->computeTruth(query.withExpr(NeExpr::create(query.expr,
                                                                result)),
                                  answer))
    return false;

  if (answer)
    assert(0 && "invalid solver result (computeValue)");

  return true;
}

bool
ValidatingSolver::computeInitialValues(const Query& query,
                                       const std::vector<const Array*>
                                         &objects,
                                       std::vector< std::vector<unsigned char> >
                                         &values,
                                       bool &hasSolution) {
  bool answer;

  if (!solver->impl->computeInitialValues(query, objects, values,
                                          hasSolution))
    return false;

  if (hasSolution) {
    // Assert the bindings as constraints, and verify that the
    // conjunction of the actual constraints is satisfiable.
    std::vector< ref<Expr> > bindings;
    for (unsigned i = 0; i != values.size(); ++i) {
      const Array *array = objects[i];
      for (unsigned j=0; j<array->size; j++) {
        unsigned char value = values[i][j];
        bindings.push_back(EqExpr::create(ReadExpr::create(UpdateList(array, 0),
                                                           ConstantExpr::alloc(j, Expr::Int32)),
                                          ConstantExpr::alloc(value, Expr::Int8)));
      }
    }
    ConstraintManager tmp(bindings);
    ref<Expr> constraints = Expr::createIsZero(query.expr);
    for (ConstraintManager::const_iterator it = query.constraints.begin(),
           ie = query.constraints.end(); it != ie; ++it)
      constraints = AndExpr::create(constraints, *it);

    if (!oracle->impl->computeTruth(Query(tmp, constraints), answer))
      return false;
    if (!answer)
      assert(0 && "invalid solver result (computeInitialValues)");
  } else {
    if (!oracle->impl->computeTruth(query, answer))
      return false;
    if (!answer)
      assert(0 && "invalid solver result (computeInitialValues)");
  }

  return true;
}

Solver *klee::createValidatingSolver(Solver *s, Solver *oracle) {
  return new Solver(new ValidatingSolver(s, oracle));
}

/***/

class DummySolverImpl : public SolverImpl {
public:
  DummySolverImpl() {}

  bool computeValidity(const Query&, Solver::Validity &result) {
    ++stats::queries;
    // FIXME: We should have stats::queriesFail;
    return false;
  }
  bool computeTruth(const Query&, bool &isValid) {
    ++stats::queries;
    // FIXME: We should have stats::queriesFail;
    return false;
  }
  bool computeValue(const Query&, ref<Expr> &result) {
    ++stats::queries;
    ++stats::queryCounterexamples;
    return false;
  }
  bool computeInitialValues(const Query&,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution) {
    ++stats::queries;
    ++stats::queryCounterexamples;
    return false;
  }
};

Solver *klee::createDummySolver() {
  return new Solver(new DummySolverImpl());
}

/***/

class STPSolverImpl : public SolverImpl {
private:
  /// The solver we are part of, for access to public information.
  STPSolver *solver;
  VC vc;
  STPBuilder *builder;
  double timeout;
  bool useForkedSTP;

  void reinstantiate();

public:
  STPSolverImpl(STPSolver *_solver, bool _useForkedSTP);
  ~STPSolverImpl();

  char *getConstraintLog(const Query&);
  void setTimeout(double _timeout) { timeout = _timeout; }

  bool computeTruth(const Query&, bool &isValid);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query&,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};

static unsigned char *shared_memory_ptr;
static const unsigned shared_memory_size = 1<<20;
static int shared_memory_id;

static void stp_error_handler(const char* err_msg) {
  fprintf(stderr, "error: STP Error: %s\n", err_msg);
  exit(-1);
}

STPSolverImpl::STPSolverImpl(STPSolver *_solver, bool _useForkedSTP)
  : solver(_solver),
    vc(vc_createValidityChecker()),
    builder(new STPBuilder(vc)),
    timeout(0.0),
    useForkedSTP(_useForkedSTP)
{
  assert(vc && "unable to create validity checker");
  assert(builder && "unable to create STPBuilder");

#ifdef HAVE_EXT_STP
  vc_setInterfaceFlags(vc, EXPRDELETE, 0);
#endif

  vc_registerErrorHandler(::stp_error_handler);

  if (useForkedSTP) {
#ifdef __MINGW32__
    assert(false && "Cannot use forked stp solver on Windows");
#else
    shared_memory_id = shmget(IPC_PRIVATE, shared_memory_size, IPC_CREAT | 0700);
    assert(shared_memory_id>=0 && "shmget failed");
    shared_memory_ptr = (unsigned char*) shmat(shared_memory_id, NULL, 0);
    assert(shared_memory_ptr!=(void*)-1 && "shmat failed");
    shmctl(shared_memory_id, IPC_RMID, NULL);
#endif
  }
}

STPSolverImpl::~STPSolverImpl() {
  delete builder;

  vc_Destroy(vc);
}

void STPSolverImpl::reinstantiate()
{
    //XXX: This seems to cause crashes.
    //Will have to find other ways of preventing slowdown
    if (ReinstantiateSolver) {
        delete builder;
        vc_Destroy(vc);
        vc = vc_createValidityChecker();
        builder = new STPBuilder(vc);

        #ifdef HAVE_EXT_STP
        vc_setInterfaceFlags(vc, EXPRDELETE, 0);
        #endif

        vc_registerErrorHandler(::stp_error_handler);
    }
}

/***/

STPSolver::STPSolver(bool useForkedSTP)
  : Solver(new STPSolverImpl(this, useForkedSTP))
{
}

char *STPSolver::getConstraintLog(const Query &query) {
  return static_cast<STPSolverImpl*>(impl)->getConstraintLog(query);
}

void STPSolver::setTimeout(double timeout) {
  static_cast<STPSolverImpl*>(impl)->setTimeout(timeout);
}

/***/

char *STPSolverImpl::getConstraintLog(const Query &query) {
  vc_push(vc);
  for (std::vector< ref<Expr> >::const_iterator it = query.constraints.begin(),
         ie = query.constraints.end(); it != ie; ++it)
    vc_assertFormula(vc, builder->construct(*it));
  assert(query.expr == ConstantExpr::alloc(0, Expr::Bool) &&
         "Unexpected expression in query!");

  char *buffer;
  unsigned long length;
  vc_printQueryStateToBuffer(vc, builder->getFalse(),
                             &buffer, &length, false);
  vc_pop(vc);

  return buffer;
}

bool STPSolverImpl::computeTruth(const Query& query,
                                 bool &isValid) {
  std::vector<const Array*> objects;
  std::vector< std::vector<unsigned char> > values;
  bool hasSolution;

  if (!computeInitialValues(query, objects, values, hasSolution))
    return false;

  isValid = !hasSolution;
  return true;
}

bool STPSolverImpl::computeValue(const Query& query,
                                 ref<Expr> &result) {
  std::vector<const Array*> objects;
  std::vector< std::vector<unsigned char> > values;
  bool hasSolution;

  // Find the object used in the expression, and compute an assignment
  // for them.
  findSymbolicObjects(query.expr, objects);
  if (!computeInitialValues(query.withFalse(), objects, values, hasSolution))
    return false;
  assert(hasSolution && "state has invalid constraint set");

  // Evaluate the expression with the computed assignment.
  Assignment a(objects, values);
  result = a.evaluate(query.expr);

  return true;
}


static void runAndGetCex(::VC vc, STPBuilder *builder, ::VCExpr q,
                   const std::vector<const Array*> &objects,
                   std::vector< std::vector<unsigned char> > &values,
                   bool &hasSolution) {
  // XXX I want to be able to timeout here, safely
    int result;

    result = vc_query(vc, q);

    if (result < 0) {
        if (klee_message_stream) {
            char *buffer;
            unsigned long length;
            vc_push(vc);
            vc_printQueryStateToBuffer(vc, q,
                                       &buffer, &length, false);
            vc_pop(vc);

            *klee_message_stream << buffer << '\n';
            free(buffer);
        }

        //Bug in stp
        throw std::exception();
    }

//    klee_message_stream << "solver returned " << *result << '\n';
    hasSolution = !result;

    if (hasSolution) {
        values.reserve(objects.size());
        for (std::vector<const Array*>::const_iterator
             it = objects.begin(), ie = objects.end(); it != ie; ++it) {
            const Array *array = *it;
            std::vector<unsigned char> data;

            data.reserve(array->size);
            for (unsigned offset = 0; offset < array->size; offset++) {
                ExprHandle counter =
                        vc_getCounterExample(vc, builder->getInitialRead(array, offset));
                unsigned char val = getBVUnsigned(counter);
                data.push_back(val);
            }

            values.push_back(data);
        }
    }
}

static void stpTimeoutHandler(int x) {
  _exit(52);
}

static bool runAndGetCexForked(::VC vc,
                               STPBuilder *builder,
                               ::VCExpr q,
                               const std::vector<const Array*> &objects,
                               std::vector< std::vector<unsigned char> >
                                 &values,
                               bool &hasSolution,
                               double timeout) {
#ifdef __MINGW32__
  assert(false && "Cannot run runAndGetCexForked on Windows");
  return false;
#else

  unsigned char *pos = shared_memory_ptr;
  unsigned sum = 0;
  for (std::vector<const Array*>::const_iterator
         it = objects.begin(), ie = objects.end(); it != ie; ++it)
    sum += (*it)->size;
  assert(sum<shared_memory_size && "not enough shared memory for counterexample");

  fflush(stdout);
  fflush(stderr);

  sigset_t sig_mask, sig_mask_old;
  sigfillset(&sig_mask);
  sigemptyset(&sig_mask_old);
  sigprocmask(SIG_SETMASK, &sig_mask, &sig_mask_old);

  int pid = fork();
  if (pid==-1) {
    fprintf(stderr, "error: fork failed (for STP)");
    return false;
  }

  if (pid == 0) {
    sigprocmask(SIG_SETMASK, &sig_mask_old, NULL);
    if (timeout) {
      ::alarm(0); /* Turn off alarm so we can safely set signal handler */
      ::signal(SIGALRM, stpTimeoutHandler);
      ::alarm(std::max(1, (int)timeout));
    }
    unsigned res = vc_query(vc, q);
    if (!res) {
      for (std::vector<const Array*>::const_iterator
             it = objects.begin(), ie = objects.end(); it != ie; ++it) {
        const Array *array = *it;
        for (unsigned offset = 0; offset < array->size; offset++) {
          ExprHandle counter =
            vc_getCounterExample(vc, builder->getInitialRead(array, offset));
          *pos++ = getBVUnsigned(counter);
        }
      }
    }
    _exit(res);
  } else {
    int status;
    pid_t res;

    do {
      res = waitpid(pid, &status, 0);
    } while (res < 0 && errno == EINTR);

    sigprocmask(SIG_SETMASK, &sig_mask_old, NULL);

    if (res < 0) {
      fprintf(stderr, "error: waitpid() for STP failed\n");
      perror("waitpid()");
      return false;
    }

    // From timed_run.py: It appears that linux at least will on
    // "occasion" return a status when the process was terminated by a
    // signal, so test signal first.
    if (WIFSIGNALED(status) || !WIFEXITED(status)) {
      fprintf(stderr, "error: STP did not return successfully\n");
      return false;
    }

    int exitcode = WEXITSTATUS(status);
    if (exitcode==0) {
      hasSolution = true;
    } else if (exitcode==1) {
      hasSolution = false;
    } else if (exitcode==52) {
      fprintf(stderr, "error: STP timed out");
      return false;
    } else {
      fprintf(stderr, "error: STP did not return a recognized code (%d)\n", exitcode);
      return false;
    }

    if (hasSolution) {
      values = std::vector< std::vector<unsigned char> >(objects.size());
      unsigned i=0;
      for (std::vector<const Array*>::const_iterator
             it = objects.begin(), ie = objects.end(); it != ie; ++it) {
        const Array *array = *it;
        std::vector<unsigned char> &data = values[i++];
        data.insert(data.begin(), pos, pos + array->size);
        pos += array->size;
      }
    }

    return true;
  }
#endif
}
static bool __stp_printstate = true;
extern llvm::raw_ostream *g_solverLog;

bool
STPSolverImpl::computeInitialValues(const Query &query,
                                    const std::vector<const Array*>
                                      &objects,
                                    std::vector< std::vector<unsigned char> >
                                      &values,
                                    bool &hasSolution) {
  TimerStatIncrementer t(stats::queryTime);

  reinstantiate();

  vc_push(vc);

  for (ConstraintManager::const_iterator it = query.constraints.begin(),
         ie = query.constraints.end(); it != ie; ++it)
    vc_assertFormula(vc, builder->construct(*it));

  ++stats::queries;
  ++stats::queryCounterexamples;

  ExprHandle stp_e = builder->construct(query.expr);

  if (__stp_printstate) {
    char *buf;
    if (g_solverLog) {
        buf = vc_printSMTLIB(vc, stp_e);
        *g_solverLog << "==============START=============" << '\n';
        *g_solverLog << buf << '\n';
        g_solverLog->flush();
        *g_solverLog << "==============END=============" << '\n';
        free(buf);
    }
    //fprintf(stderr, "note: STP query: %.*s\n", (unsigned) len, buf);
  }

  bool success;
  if (useForkedSTP) {
    success = runAndGetCexForked(vc, builder, stp_e, objects, values,
                                 hasSolution, timeout);
  } else {
    try {
        runAndGetCex(vc, builder, stp_e, objects, values, hasSolution);
        success = true;
    } catch(std::exception &) {
        klee::klee_warning("STP solver threw an exception");
        exit(-1);
        vc_pop(vc);
        reinstantiate();
        success = false;
        return success;
    }
  }

  if (success) {
    if (hasSolution)
      ++stats::queriesInvalid;
    else
      ++stats::queriesValid;
  }

  vc_pop(vc);


  return success;
}
