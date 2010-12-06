#include "ForcedInliner.h"

#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Transforms/Utils/BasicInliner.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <vector>
#include <set>

using namespace llvm;


namespace llvm {

  /// BasicInlinerImpl - BasicInliner implemantation class. This hides
  /// container info, used by basic inliner, from public interface.
  struct VISIBILITY_HIDDEN ForcedInlinerImpl {
    
    ForcedInlinerImpl(const ForcedInlinerImpl&); // DO NOT IMPLEMENT
    void operator=(const ForcedInlinerImpl&); // DO NO IMPLEMENT
  public:
    ForcedInlinerImpl(TargetData *T) : TD(T) {
        m_inlineOnce = false;
    }

    /// addFunction - Add function into the list of functions to process.
    /// All functions must be inserted using this interface before invoking
    /// inlineFunctions().
    void addFunction(Function *F) {
      Functions.push_back(F);
    }

    /// inlineFuctions - Walk all call sites in all functions supplied by
    /// client. Inline as many call sites as possible. Delete completely
    /// inlined functions.
    void inlineFunctions();
    

    /// setPrefix - inline only those functions whose name begins with prefix
    void setPrefix(const std::string &prefix) {
        m_prefix = prefix;
    }

    void setInlineOnceEachFunction(bool val) {
        m_inlineOnce = val;
    }

    const std::set<CallSite>&getNotInlinedCallSites() const {
        return m_notInlinedCallSites;
    }
  private:
    TargetData *TD;
    std::vector<Function *> Functions;
    std::string m_prefix;
    bool m_inlineOnce;
    std::set<CallSite> m_notInlinedCallSites;
  };

/// inlineFuctions - Walk all call sites in all functions supplied by
/// client. Inline as many call sites as possible. Delete completely
/// inlined functions.
void ForcedInlinerImpl::inlineFunctions() {
      
  m_notInlinedCallSites.clear();

  // Scan through and identify all call sites ahead of time so that we only
  // inline call sites in the original functions, not call sites that result
  // from inlining other functions.
  std::vector<CallSite> CallSites;
  std::set<Function*> CalledFunctions;
  
  for (std::vector<Function *>::iterator FI = Functions.begin(),
         FE = Functions.end(); FI != FE; ++FI) {
    Function *F = *FI;
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        CallSite CS = CallSite::get(I);
        if (CS.getInstruction() && CS.getCalledFunction()
            && !CS.getCalledFunction()->isDeclaration()) {

            if (m_inlineOnce) {
                if (CalledFunctions.find(CS.getCalledFunction()) == CalledFunctions.end()) {
                    CallSites.push_back(CS);
                    CalledFunctions.insert(CS.getCalledFunction());
                }else {
                    m_notInlinedCallSites.insert(CS);
                }
            }else {
                CallSites.push_back(CS);
            }
        }
      }
  }
  
  DOUT << ": " << CallSites.size() << " call sites.\n";
  
  // Inline call sites.
  bool Changed = false;
  do {
    Changed = false;
    for (unsigned index = 0; index != CallSites.size() && !CallSites.empty(); ++index) {
      CallSite CS = CallSites[index];
      if (Function *Callee = CS.getCalledFunction()) {
        
        // Eliminate calls that are never inlinable.
        if (Callee->isDeclaration() ||
            CS.getInstruction()->getParent()->getParent() == Callee) {
          CallSites.erase(CallSites.begin() + index);
              --index;
              continue;
        }

        // Elimitate functions that do not have the right prefix
        if (m_prefix.size() > 0 && Callee->getNameStr().find(m_prefix) == std::string::npos) {
            CallSites.erase(CallSites.begin() + index);
                --index;
                continue;
        }
        
        // Inline
        if (InlineFunction(CS, NULL, TD)) {
          Changed = true;
          CallSites.erase(CallSites.begin() + index);
          --index;
        }
      }
    }
  } while (Changed);
}

ForcedInliner::ForcedInliner(TargetData *TD) {
  Impl = new ForcedInlinerImpl(TD);
}

ForcedInliner::~ForcedInliner() {
  delete Impl;
}

/// addFunction - Add function into the list of functions to process.
/// All functions must be inserted using this interface before invoking
/// inlineFunctions().
void ForcedInliner::addFunction(Function *F) {
  Impl->addFunction(F);
}

/// inlineFuctions - Walk all call sites in all functions supplied by
/// client. Inline as many call sites as possible. Delete completely
/// inlined functions.
void ForcedInliner::inlineFunctions() {
  Impl->inlineFunctions();
}

void ForcedInliner::setPrefix(const std::string &prefix) {
       Impl->setPrefix(prefix);
}

void ForcedInliner::setInlineOnceEachFunction(bool val) {
    Impl->setInlineOnceEachFunction(val);
}

const std::set<CallSite>& ForcedInliner::getNotInlinedCallSites() const {
    return Impl->getNotInlinedCallSites();
}

}
