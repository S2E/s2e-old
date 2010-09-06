#ifndef S2E_PLUGINS_MAXTBSEARCHER_H
#define S2E_PLUGINS_MAXTBSEARCHER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/S2EExecutionState.h>

#include <klee/Searcher.h>

#include <vector>

namespace s2e {
namespace plugins {

class MaxTbSearcher;

class MaxTbSearcherState: public PluginState
{
private:
    uint64_t m_metric;
    MaxTbSearcher *m_plugin;
    S2EExecutionState *m_state;
public:

    MaxTbSearcherState();
    MaxTbSearcherState(S2EExecutionState *s, Plugin *p);
    virtual ~MaxTbSearcherState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class MaxTbSearcher;

};

class MaxTbSearcher : public Plugin, public klee::Searcher
{
    S2E_PLUGIN
public:
    struct MaxTbSorter{
        Plugin *p;

        MaxTbSorter() {
            p = NULL;
        }

        bool operator()(const S2EExecutionState *s1, const S2EExecutionState *s2) const{
            const MaxTbSearcherState *p1 = static_cast<MaxTbSearcherState*>(p->getPluginState(const_cast<S2EExecutionState*>(s1), &MaxTbSearcherState::factory));
            const MaxTbSearcherState *p2 = static_cast<MaxTbSearcherState*>(p->getPluginState(const_cast<S2EExecutionState*>(s2), &MaxTbSearcherState::factory));

            if (p1->m_metric == p2->m_metric) {
                return p1 < p2;
            }
            return p1->m_metric < p2->m_metric;
        }
    };

    typedef std::set<S2EExecutionState*, MaxTbSorter> StateSet;

    //Maps a translation block address to the number of times it was executed
    typedef std::map<uint64_t, uint64_t> TbMap;
    typedef std::map<ModuleDescriptor, TbMap, ModuleDescriptor::ModuleByName > TbsByModule;

    MaxTbSearcher(S2E* s2e): Plugin(s2e) {}
    void initialize();

    virtual klee::ExecutionState& selectState();
    virtual void update(klee::ExecutionState *current,
                        const std::set<klee::ExecutionState*> &addedStates,
                        const std::set<klee::ExecutionState*> &removedStates);

    virtual bool empty();

private:

    ModuleExecutionDetector *m_moduleExecutionDetector;
    bool m_searcherInited;

    klee::Searcher *m_parentSearcher;
    TbsByModule m_coveredTbs;

    MaxTbSorter m_sorter;
    StateSet m_states;


    void addTb(S2EExecutionState *s, uint64_t absTargetPc);
    bool isExplored(S2EExecutionState *s, uint64_t absTargetPc);
    uint64_t computeTargetPc(S2EExecutionState *s);
    bool updatePc(S2EExecutionState *es);

    void onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);

    void onTraceTb(S2EExecutionState* state, uint64_t pc);

    void initializeSearcher();

    friend class MaxTbSearcherState;
};



} // namespace plugins
} // namespace s2e

#endif
