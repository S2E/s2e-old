/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2014, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Stefan Bucur <stefan.bucur@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef S2E_PLUGINS_CONCOLICSESSION_H
#define S2E_PLUGINS_CONCOLICSESSION_H

#include <s2e/Plugin.h>
#include <s2e/Selectors.h>
#include <klee/Searcher.h>

// FIXME: Is this a hack?
#include "../BaseInstructions.h"
#include "InterpreterMonitor.h"

#include <llvm/Support/TimeValue.h>
#include <llvm/Support/raw_ostream.h>

#include <set>
#include <vector>
#include <map>
#include <fstream>


namespace s2e {
namespace plugins {


class ConcolicSession;


struct ProgramCounterHash {
	uint64_t operator()(const S2EExecutionState *state) {
		return state->getPc();
	}
};


struct TraceNodeHash {
	TraceNodeHash(InterpreterMonitor *monitor = 0) : monitor_(monitor) {}

	HighLevelTreeNode* operator()(S2EExecutionState *state);

	InterpreterMonitor* monitor_;
};


struct HighLevelInstructionHash {
	HighLevelInstructionHash(InterpreterMonitor *monitor = 0) :
			trace_node_(monitor) {

	}

	HighLevelInstruction *operator()(S2EExecutionState *state) {
		HighLevelTreeNode *tnode = trace_node_(state);
		if (tnode == NULL)
			return NULL;

		return tnode->instruction();
	}

	TraceNodeHash trace_node_;
};


struct MinDistanceToUncovWeight {
	double operator()(HighLevelInstruction *instr) {
		double base = 1.0 / (instr->dist_to_uncovered() ?
					instr->dist_to_uncovered() : 100);
		double weight = base;

		return weight;
	}

	double operator()(HighLevelTreeNode *node) {
		if (node == NULL)
			return 1.0 / 100;

		return operator ()(node->instruction());
	}
};

struct ForkWeight {
	ForkWeight() : concolic_session_(0) {}

	double operator()(S2EExecutionState *state);

	ConcolicSession *concolic_session_;
};


class ForkPoint {
public:
	typedef std::vector<ForkPoint*> ForkPointVector;

	ForkPoint(ForkPoint *parent, int index, uint64_t pc,
			HighLevelTreeNode* hl_node, int children_count)
		: parent_(parent),
		  children_(children_count),
		  depth_(0),
		  index_(index),
		  pc_(pc),
		  hl_node_(hl_node) {
		if (parent != NULL) {
			assert(0 <= index && index < parent->children_.size());

			parent->children_[index] = this;
			depth_ = parent->depth_ + 1;
		} else {
			index_ = -1;
		}
	}

	ForkPoint *parent() const {
		return parent_;
	}

	int index() const {
		return index_;
	}

	int depth() const {
		return depth_;
	}

	uint64_t pc() const {
		return pc_;
	}

	HighLevelTreeNode *hl_node() const {
		return hl_node_;
	}

	void clear() {
		for (ForkPointVector::iterator it = children_.begin(),
				ie = children_.end(); it != ie; ++it) {
			ForkPoint *fork_point = *it;
			if (fork_point != NULL) {
				fork_point->clear();
				delete fork_point;
			}
		}
		children_.clear();
	}

private:
	ForkPoint *parent_;
	ForkPointVector children_;

	int depth_;
	int index_;

	uint64_t pc_;
	HighLevelTreeNode *hl_node_;

	// Disallow copy and assign
	ForkPoint(const ForkPoint&);
	void operator=(const ForkPoint&);
};


class MemoryTracer;
class TranslationBlockTracer;


/*
 * TODO: Only one session at a time is supported for now.  Add support for more
 * if needed.
 */
class ConcolicSession: public Plugin,
					   public klee::Searcher,
					   public BaseInstructionsPluginInvokerInterface {
	S2E_PLUGIN
public:
	ConcolicSession(S2E* s2e_);
	virtual ~ConcolicSession();

	virtual void initialize();

	// BaseInstructionsPluginInvokerInterface

	virtual unsigned int handleOpcodeInvocation(S2EExecutionState *state,
			uint64_t guestDataPtr, uint64_t guestDataSize);

	// klee::Searcher

    klee::ExecutionState &selectState();
    void update(klee::ExecutionState *current,
                const std::set<klee::ExecutionState*> &addedStates,
                const std::set<klee::ExecutionState*> &removedStates);
    bool empty();

    double getForkWeight(S2EExecutionState *state);

private:
	typedef std::vector<S2EExecutionState*> StateVector;
	typedef std::set<S2EExecutionState*> StateSet;
	typedef std::map<S2EExecutionState*, double> ForkWeightMap;
	typedef std::map<S2EExecutionState*, std::pair<ForkPoint*, int> > ForkPointMap;

	// Test case tracing
	llvm::raw_ostream *cfg_tc_stream_;
	llvm::raw_ostream *paths_tc_stream_;
	llvm::raw_ostream *all_tc_stream_;

	llvm::raw_ostream *compl_feature_stream_;
	llvm::raw_ostream *pending_feature_stream_;

	// Session configuration
	bool stop_on_error_;
	bool use_random_pending_;
	bool use_weighting_;
	int tree_dump_interval_;
	int state_time_out_;
	bool extra_details_;

	// Callback connections
	sigc::connection on_interpreter_trace_;
	sigc::connection on_state_fork_;
	sigc::connection on_state_kill_;
	sigc::connection on_state_switch_;
	sigc::connection on_timer_;

	// The interpreter monitor plug-in
	InterpreterMonitor *interp_monitor_;
	// The searcher used for anything outside a concolic session
	klee::DFSSearcher *out_searcher_;

	// TODO: Reorganize the data structure below around the execution state.
	// Idea: Create a ConcolicSessionState that keeps everything in one place.
	// Do not derive from S2EExecutionState.

	// The fork tree
	ForkPoint *root_fork_point_;

	// Active state information
	S2EExecutionState *active_state_;
	HighLevelTreeNode *tree_divergence_node_;
	HighLevelTreeNode *cfg_divergence_node_;

	// Pending states data structures
	Selector<S2EExecutionState*> *pending_states_;

	// Fork points
	ForkPoint *starting_fork_point_;
	ForkPoint *active_fork_point_;
	int active_fork_index_;
	ForkPointMap pending_fork_points_; // XXX: Memory leaks everywhere

	// Fork weights
	ForkWeightMap pending_fork_weights_;
	uint64_t fork_weight_pc_;
	StateVector fork_strike_;

	// Time tracking
	llvm::sys::TimeValue start_time_stamp_;
	llvm::sys::TimeValue path_time_stamp_;
	llvm::sys::TimeValue session_deadline_;
	llvm::sys::TimeValue path_deadline_;
	llvm::sys::TimeValue next_dump_stamp_;


	// Debugging
	MemoryTracer *memory_tracer_;
	TranslationBlockTracer *tb_tracer_;

	// Pending state management
	bool emptyPendingStates();
	void selectPendingState(S2EExecutionState *state);
	void insertPendingState(S2EExecutionState *state);
	void copyAndClearPendingStates(StateVector &states);

	void terminateSession(S2EExecutionState *state);
	void dumpTestCase(S2EExecutionState *state,
			llvm::sys::TimeValue time_stamp, llvm::sys::TimeValue total_delta,
			llvm::raw_ostream &out);
	void computeMinMaxDistToUncovered(S2EExecutionState *state, int &min_dist,
			int &max_dist);

	int startConcolicSession(S2EExecutionState *state, uint32_t max_time);
	int endConcolicSession(S2EExecutionState *state, bool is_error_path);

	void dumpTraceGraphs();

	void flushPathConstraints(S2EExecutionState *state);
	bool writeToGuest(S2EExecutionState *state,
			uint64_t address, void *buf, uint64_t size);

	void onInterpreterTrace(S2EExecutionState *state,
			HighLevelTreeNode *tree_node);
	void onStateFork(S2EExecutionState *state, const StateVector &newStates,
			const std::vector<klee::ref<klee::Expr> > &newConditions);
	void onStateKill(S2EExecutionState *state);
	void onStateSwitch(S2EExecutionState *old_state,
			S2EExecutionState *new_state);
	void onTimer();

	// Disallow copy and assign
	ConcolicSession(const ConcolicSession&);
	void operator=(const ConcolicSession&);
};


} /* namespace plugins */
} /* namespace s2e */

#endif /* S2E_PLUGINS_CONCOLICSESSION_H */
