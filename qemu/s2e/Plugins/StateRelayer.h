#ifndef S2E_PLUGINS_StateRelayer_H
#define S2E_PLUGINS_StateRelayer_H

#include <string>
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <klee/Searcher.h>

namespace s2e {
namespace plugins {
/**
 * 实现持续性的测试,保证测试状态生生不息.
 * 主要用于结合符号执行和模糊测试的思路,与输入控制插件一起使用.
 */
/** Handler required for KLEE interpreter */
class StateRelayer: public Plugin, public klee::Searcher {
S2E_PLUGIN

	struct SortById {
		bool operator ()(const klee::ExecutionState *_s1,
				const klee::ExecutionState *_s2) const {
			const S2EExecutionState *s1 =
					static_cast<const S2EExecutionState*>(_s1);
			const S2EExecutionState *s2 =
					static_cast<const S2EExecutionState*>(_s2);

			return s1->getID() < s2->getID();
		}
	};
	typedef std::set<klee::ExecutionState*, SortById> States;

public:
	typedef std::set<std::string> StringSet;
	virtual klee::ExecutionState& selectState();

	virtual void afterupdate(klee::ExecutionState *current);
	virtual void update(klee::ExecutionState *current,
			const std::set<klee::ExecutionState*> &addedStates,
			const std::set<klee::ExecutionState*> &removedStates);

	virtual bool empty();

private:
    typedef std::pair<std::string, std::vector<unsigned char> > VarValuePair;
    typedef std::vector<VarValuePair> ConcreteInputs;
	bool m_autosendkey_enter;
	int64_t m_autosendkey_interval;
	bool m_key_enter_sent;
	uint64_t m_currentTime;sigc::connection m_timerconn;

	States m_normalStates;
	States m_speculativeStates;

	klee::ref<klee::Expr> m_dummy_symb;
	int m_current_conditon;bool m_isfirstInstructionProcessed;sigc::connection m_firstInstructionTranslateStart;sigc::connection m_firstInstructionProcess;
public:
	StateRelayer(S2E* s2e);

	void initialize();

private:
	void onInitializationComplete(S2EExecutionState* state);
	void onTimer();

	void ProcessFirstInstruction(S2EExecutionState* state, uint64_t pc);
	klee::Executor::StatePair prepareNextState(S2EExecutionState *state,
			bool isinitial = false);
	void slotFirstInstructionTranslateStart(ExecutionSignal *signal,
			S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);

	void onStateSwitchEnd(S2EExecutionState *currentState,
			S2EExecutionState *nextState);
};
}
}

#endif
