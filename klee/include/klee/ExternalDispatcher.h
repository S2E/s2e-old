//===-- ExternalDispatcher.h ------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXTERNALDISPATCHER_H
#define KLEE_EXTERNALDISPATCHER_H

#include <map>
#include <string>
#include <stdint.h>

namespace llvm {
  class ExecutionEngine;
  class Instruction;
  class Function;
  class FunctionType;
  class Module;
}

namespace klee {
  class ExternalDispatcher {
  protected:
    typedef std::map<const llvm::Instruction*,llvm::Function*> dispatchers_ty;
    dispatchers_ty dispatchers;
    llvm::Module *dispatchModule;
    llvm::ExecutionEngine *originalEngine;
    llvm::ExecutionEngine *executionEngine;
    std::map<std::string, void*> preboundFunctions;

    static uint64_t *gTheArgsP;

    llvm::Function *createDispatcher(llvm::Function *f, llvm::Instruction *i);
    virtual bool runProtectedCall(llvm::Function *f, uint64_t *args);
    
  public:
    ExternalDispatcher(llvm::ExecutionEngine* engine = NULL);
    virtual ~ExternalDispatcher();

    /* Call the given function using the parameter passing convention of
     * ci with arguments in args[1], args[2], ... and writing the result
     * into args[0].
     */
    virtual bool executeCall(llvm::Function *function, llvm::Instruction *i, uint64_t *args);
    virtual void *resolveSymbol(const std::string &name);
  };  
}

#endif
