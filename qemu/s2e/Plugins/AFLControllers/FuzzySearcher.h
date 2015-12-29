/*
 * FuzzySearcher.h
 *
 *  Created on: 2015.12.24
 *      Author: Epeius
 */

#ifndef FUZZYSEARCHER_H_
#define FUZZYSEARCHER_H_
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/HostFiles.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <klee/Searcher.h>
#include <vector>
#include "klee/util/ExprEvaluator.h"
#include "AutoShFileGenerator.h"


namespace s2e {
namespace plugins {
class FuzzySearcher;
class FuzzySearcherState: public PluginState
{
private:
	FuzzySearcher* m_plugin;
    S2EExecutionState *m_state;
public:
    //in order to improve efficiency, we write the branches of S2E to AFL's bitmap
    uint64_t m_prev_loc;//previous location when executing
    FuzzySearcherState();
    FuzzySearcherState(S2EExecutionState *s, Plugin *p);
    virtual ~FuzzySearcherState();
    virtual PluginState *clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    inline bool updateAFLBitmapSHM(unsigned char* bitmap, uint64_t pc);

    friend class FuzzySearcher;
};

#define AFL_BITMAP_SIZE (1 << 16)

class FuzzySearcher : public Plugin , public klee::Searcher {
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
	typedef std::set<std::string> StringSet;
	typedef std::pair<std::string, std::vector<unsigned char> > VarValuePair;
	typedef std::vector<VarValuePair> ConcreteInputs;
	ModuleExecutionDetector *m_detector;
	HostFiles *m_hostFiles;
	AutoShFileGenerator *m_AutoShFileGenerator;
public:
	bool m_autosendkey_enter;
	int64_t m_autosendkey_interval;
	bool m_key_enter_sent;
	uint64_t m_currentTime;
	sigc::connection m_timerconn;
	States m_normalStates;
	States m_speculativeStates;
	klee::ref<klee::Expr> m_dummy_symb;
	int m_current_conditon;bool m_isfirstInstructionProcessed;sigc::connection m_firstInstructionTranslateStart;sigc::connection m_firstInstructionProcess;
	virtual klee::ExecutionState& selectState();
	virtual void update(klee::ExecutionState *current,
			const std::set<klee::ExecutionState*> &addedStates,
			const std::set<klee::ExecutionState*> &removedStates);
	virtual bool empty();

	void onTimer();
	void ProcessFirstInstruction(S2EExecutionState* state, uint64_t pc);
	klee::Executor::StatePair prepareNextState(S2EExecutionState *state,
	bool isinitial = false);
	S2EExecutionState* getNewCaseFromPool(S2EExecutionState* instate);
	void slotFirstInstructionTranslateStart(ExecutionSignal *signal,
			S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);
	void onStateSwitchEnd(S2EExecutionState *currentState,
			S2EExecutionState *nextState);
	void onStateKill(S2EExecutionState *State);
	bool generateCaseFile(S2EExecutionState *state, std::string templatefile);
	static bool copyfile(const char* fromfile, const char* tofile);
private:
	/**
	 * schduelar
	 */
	std::string m_inicasepool;//initial testcase's directory
	std::string m_curcasepool;//current input directory(as a temple place)

	// AFL related
	std::string m_aflOutputpool;//AFL's output directory
	std::string m_aflRoot;//AFL's root directory
	bool m_aflBinaryMode;//whether to use binary mode
	bool m_AFLStarted;//AFL started
	std::string m_aflAppArgs;//arguments of targer binary code and replace the file with "@@"
	int m_aflPid;//AFL's Pid
	unsigned char* m_aflBitmapSHM;//AFL's bitmap
	bool m_findBitMapSHM;//whether we have find bitmap
	// AFL end

	std::string m_symbolicfilename;//

	std::string m_hostfilebase;
	std::string m_mainModule;//main module name (i.e. target binary)

	int m_idlecondition;

	int m_loops;//current iteration
	int m_MAXLOOPs;//stop condition


public:
	FuzzySearcher(S2E* s2e) :
			Plugin(s2e) {
		m_hostFiles = NULL;
		m_detector = NULL;
		m_isfirstInstructionProcessed = false;
		m_current_conditon = 0;
		m_autosendkey_enter = true;
		m_key_enter_sent = false;
		m_autosendkey_interval = 10;
		m_currentTime = 0;
		m_idlecondition = 0;
		m_loops = 0;
		m_aflPid = 0;
		m_aflBitmapSHM = 0;
		m_findBitMapSHM = false;
		m_AFLStarted = false;
		m_aflBinaryMode = false;
	}
	virtual ~FuzzySearcher();
	void initialize();
	/*
	sigc::signal<void,
	       S2EExecutionState*,
	       const ModuleDescriptor &
	    >onModuleLoad;
	    */
	void onModuleLoad(S2EExecutionState*, const ModuleDescriptor &);


	void slotExecuteBlockStart(S2EExecutionState* state, uint64_t pc);
	void slotExecuteBlockEnd(S2EExecutionState* state, uint64_t pc);

	  void onModuleTranslateBlockStart(ExecutionSignal*,
			  S2EExecutionState*,
			  const ModuleDescriptor &,
			  TranslationBlock*,
			  uint64_t );
	  void onModuleTranslateBlockEnd(
			  ExecutionSignal *signal,
			  S2EExecutionState* state,
			  const ModuleDescriptor &module,
			  TranslationBlock *tb,
			  uint64_t endPc,
			  bool staticTarget,
			  uint64_t targetPc);

	void onTestCaseGeneration(S2EExecutionState *state,
			const std::string &message) ;
	int getCurrentLoop(void)const{
		return m_loops;
	}

	bool getAFLBitmapSHM();
};
}
} /* namespace s2e */

#endif /* !FUZZYSEARCHER_H_ */
