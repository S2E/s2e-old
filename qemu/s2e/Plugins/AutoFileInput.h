#ifndef S2E_PLUGINS_AutoFileInput_H
#define S2E_PLUGINS_AutoFileInput_H

#include <string>
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <klee/Searcher.h>

namespace s2e {
namespace plugins {

/** Handler required for KLEE interpreter */
class AutoFileInput: public Plugin, public klee::Searcher {
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
	std::string m_command_str;
	std::string m_command_file;
	std::string m_case_file;
	std::string m_snap_short_name;
	bool m_file_updated;
	bool m_autosendkey_enter;
	int64_t m_autosendkey_interval;
	int64_t m_crash_count;
	int64_t m_candidate_count;
	bool m_key_enter_sent;
	uint64_t m_currentTime;sigc::connection m_timerconn;

	bool m_gen_crash_onvuln;
	States m_normalStates;
	States m_speculativeStates;

	klee::ref<klee::Expr> m_dummy_symb;
	int m_current_conditon;bool m_isfirstInstructionProcessed;sigc::connection m_firstInstructionTranslateStart;sigc::connection m_firstInstructionProcess;
public:
	AutoFileInput(S2E* s2e);

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
	void onVulnFound(S2EExecutionState *state, std::string type,
			std::string message);
    void onTestCaseGeneration(S2EExecutionState *state, const std::string &message);
    void generateCrashFile(S2EExecutionState *state);
    bool copyfile(const char* fromfile, const char* tofile);
};
}
}

#endif
