//===-- KleeExecutor.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Class to perform actual execution, hides implementation details from external
// interpreter.
//
//===----------------------------------------------------------------------===//
#ifndef KLEE_KLEEEXECUTOR_H
#define KLEE_KLEEEXECUTOR_H

#include <klee/Executor.h>

namespace klee {

class KleeExecutor : public Executor
{
public:
    KleeExecutor(const InterpreterOptions &opts, InterpreterHandler *ie);
};

} // namespace

#endif // KLEEEXECUTOR_H
