#ifndef __S2E_BASE_H
#define __S2E_BASE_H

#include <klee/Interpreter.h>

namespace klee {
  class ExecutionState;
}

class BaseHandler : public klee::InterpreterHandler
{
private:
  klee::Interpreter *m_interpreter;
  std::ostream *m_infoFile;

  char m_outputDirectory[1024];
  unsigned m_testIndex;  // number of tests written so far
  unsigned m_pathsExplored; // number of paths explored so far

  // used for writing .ktest files
  int m_argc;
  char **m_argv;

public:
  BaseHandler();
  ~BaseHandler();

  std::ostream &getInfoStream() const { return *m_infoFile; }

  std::string getOutputFilename(const std::string &filename);
  std::ostream *openOutputFile(const std::string &filename);

  void incPathsExplored() { m_pathsExplored++; }

  void processTestCase(const klee::ExecutionState &state,
                               const char *err,
                               const char *suffix);
};

#endif // __S2E_BASE_H
