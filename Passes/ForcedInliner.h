#ifndef __FORCED_INLINER_H__

#define __FORCED_INLINER_H__

#include "llvm/Transforms/Utils/InlineCost.h"

namespace llvm {

  class Function;
  class TargetData;
  struct ForcedInlinerImpl;

  /// BasicInliner - BasicInliner provides function level inlining interface.
  /// Clients provide list of functions which are inline without using
  /// module level call graph information. Note that the BasicInliner is
  /// free to delete a function if it is inlined into all call sites.
  class ForcedInliner {
  public:
    
    explicit ForcedInliner(TargetData *T = NULL);
    ~ForcedInliner();

    /// addFunction - Add function into the list of functions to process.
    /// All functions must be inserted using this interface before invoking
    /// inlineFunctions().
    void addFunction(Function *F);

    /// inlineFuctions - Walk all call sites in all functions supplied by
    /// client. Inline as many call sites as possible. Delete completely
    /// inlined functions.
    void inlineFunctions();

  private:
    ForcedInlinerImpl *Impl;
  };
}


#endif
