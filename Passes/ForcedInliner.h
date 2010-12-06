#ifndef __FORCED_INLINER_H__

#define __FORCED_INLINER_H__

#include "llvm/Transforms/Utils/InlineCost.h"

#include <string>
#include <set>

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

    /// setPrefix - inline only those functions whose name begins with prefix
    void setPrefix(const std::string &prefix);

    /// specifies whether to inline one call site only for a particular function
    void setInlineOnceEachFunction(bool val);

    /// All the call sites that were not inline during the process
    const std::set<CallSite>& getNotInlinedCallSites() const;


  private:
    ForcedInlinerImpl *Impl;    
  };
}


#endif
