#include "KleeExecutor.h"

using namespace klee;

KleeExecutor::KleeExecutor(const InterpreterOptions &opts, InterpreterHandler *ie)
        : Executor(opts, ie)
{
}

///

Interpreter *Interpreter::createKleeExecutor(const InterpreterOptions &opts,
                                 InterpreterHandler *ih) {
  return new KleeExecutor(opts, ih);
}
