//===-- Common.h ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * This file groups declarations that are common to both KLEE and Kleaver.
 */

#ifndef KLEE_COMMON_H
#define KLEE_COMMON_H

#ifdef __CYGWIN__
#ifndef WINDOWS
#define WINDOWS
#endif
#endif

#include "klee/Solver.h"
#include <string>
#include <stdio.h>
#include <iostream>
#include <llvm/Support/raw_ostream.h>

namespace klee {
    const char ALL_QUERIES_SMT2_FILE_NAME[]="all-queries.smt2";
    const char SOLVER_QUERIES_SMT2_FILE_NAME[]="solver-queries.smt2";
    const char ALL_QUERIES_PC_FILE_NAME[]="all-queries.pc";
    const char SOLVER_QUERIES_PC_FILE_NAME[]="solver-queries.pc";
}

// XXX ugh
namespace klee {
  class Solver;

  extern llvm::raw_ostream* klee_warning_stream;
  extern llvm::raw_ostream* klee_message_stream;

  /// Print "KLEE: ERROR" followed by the msg in printf format and a
  /// newline on stderr and to warnings.txt, then exit with an error.
  void klee_error(const char *msg, ...)
    __attribute__ ((format (printf, 1, 2), noreturn));

  /// Print "KLEE: " followed by the msg in printf format and a
  /// newline on stderr and to messages.txt.
  void klee_message(const char *msg, ...)
    __attribute__ ((format (printf, 1, 2)));

  /// Print "KLEE: " followed by the msg in printf format and a
  /// newline to messages.txt.
  void klee_message_to_file(const char *msg, ...)
    __attribute__ ((format (printf, 1, 2)));

  /// Print "KLEE: WARNING" followed by the msg in printf format and a
  /// newline on stderr and to warnings.txt.
  void klee_warning(const char *msg, ...)
    __attribute__ ((format (printf, 1, 2)));

  /// Print "KLEE: WARNING" followed by the msg in printf format and a
  /// newline on stderr and to warnings.txt. However, the warning is only 
  /// printed once for each unique (id, msg) pair (as pointers).
  void klee_warning_once(const void *id,
                         const char *msg, ...)
    __attribute__ ((format (printf, 2, 3)));




  /// Print "KLEE: WARNING" followed by the msg in printf format and a
  /// newline on stderr and to warnings.txt. However, the warning is only
  /// printed once for each unique (id, msg) pair (as pointers).
  /// This function should be used for warnings about external function calls
  void klee_warning_external(const void *id,
                             const char *msg, ...)
    __attribute__ ((format (printf, 2, 3)));

  struct hexval {
      uint64_t value;
      int width;

      hexval(uint64_t _value, int _width=0) : value(_value), width(_width) {}
      hexval(void* _value, int _width=0): value((uint64_t)_value), width(_width) {}
  };

  inline llvm::raw_ostream& operator<<(llvm::raw_ostream& out, const hexval& h)
  {
      out << "0x";
      out.write_hex(h.value);
      return out;
  }

  inline std::ostream& operator<<(std::ostream& out, const hexval& h)
  {
      out << std::hex << "0x" << (h.value);
      return out;
  }

}
#endif /* KLEE_COMMON_H */

