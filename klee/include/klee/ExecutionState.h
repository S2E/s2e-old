//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/Internal/ADT/TreeStream.h"

#include "klee/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#include "klee/util/Assignment.h"

#include <map>
#include <set>
#include <vector>

namespace klee {
  class Array;
  class CallPathNode;
  struct Cell;
  struct KFunction;
  struct KInstruction;
  class MemoryObject;
  class PTreeNode;
  struct InstructionInfo;

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const MemoryMap &mm);

struct StackFrame {
  KInstIterator caller;
  KFunction *kf;
  CallPathNode *callPathNode;

  std::vector<const MemoryObject*> allocas;
  Cell *locals;

  /// Minimum distance to an uncovered instruction once the function
  /// returns. This is not a good place for this but is used to
  /// quickly compute the context sensitive minimum distance to an
  /// uncovered instruction. This value is updated by the StatsTracker
  /// periodically.
  unsigned minDistToUncoveredOnReturn;

  // For vararg functions: arguments not passed via parameter are
  // stored (packed tightly) in a local (alloca) memory object. This
  // is setup to match the way the front-end generates vaarg code (it
  // does not pass vaarg through as expected). VACopy is lowered inside
  // of intrinsic lowering.
  MemoryObject *varargs;

  StackFrame(KInstIterator caller, KFunction *kf);
  StackFrame(const StackFrame &s);
  ~StackFrame();
};

class ExecutionState {
  friend class AddressSpace;
  //epeius start
public:
  bool  m_is_carry_on_state; //
  bool  m_silently_concretized;// whether this state is a silent concretized state<KLEE EROOR>
  //epeius end
public:
  typedef std::vector<StackFrame> stack_ty;

private:
  // unsupported, use copy constructor
  ExecutionState &operator=(const ExecutionState&); 
  std::map< std::string, std::string > fnAliases;

public:
  bool fakeState;
  // Are we currently underconstrained?  Hack: value is size to make fake
  // objects.
  unsigned underConstrained;
  unsigned depth;
  
  // pc - pointer to current instruction stream
  KInstIterator pc, prevPC;
  stack_ty stack;
  ConstraintManager constraints;
  mutable double queryCost;
  double weight;
  AddressSpace addressSpace;
  TreeOStream pathOS, symPathOS;
  unsigned instsSinceCovNew;
  bool coveredNew;

  /// Disables forking, set by user code.
  bool forkDisabled;

  std::map<const std::string*, std::set<unsigned> > coveredLines;
  PTreeNode *ptreeNode;

  /// ordered list of symbolics: used to generate test cases. 
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector< std::pair<const MemoryObject*, const Array*> > symbolics;

  Assignment concolics;
  bool speculative;
  ref<Expr> speculativeCondition;

  // Used by the checkpoint/rollback methods for fake objects.
  // FIXME: not freeing things on branch deletion.
  MemoryMap shadowObjects;

  unsigned incomingBBIndex;

  std::string getFnAlias(std::string fn);
  void addFnAlias(std::string old_fn, std::string new_fn);
  void removeFnAlias(std::string fn);
  
private:
  ExecutionState() : fakeState(false), underConstrained(0),
                     addressSpace(this), ptreeNode(0) {}

protected:
  virtual ExecutionState* clone();
  virtual void addressSpaceChange(const MemoryObject *mo,
                                  const ObjectState *oldState,
                                  ObjectState *newState);

public:
  ExecutionState(KFunction *kf);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(const std::vector<ref<Expr> > &assumptions);

  virtual ~ExecutionState();
  
  ExecutionState *branch();

  void pushFrame(KInstIterator caller, KFunction *kf);
  void popFrame();

  void addSymbolic(const MemoryObject *mo, const Array *array) { 
    symbolics.push_back(std::make_pair(mo, array));
  }
  virtual void addConstraint(ref<Expr> e) { 
    constraints.addConstraint(e); 
  }

  virtual bool merge(const ExecutionState &b);

  bool isSpeculative() const {
      return speculative;
  }
};

}

#endif
