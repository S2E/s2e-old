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

#include "ConcolicSession.h"

#include "../ExecutionTracers/MemoryTracer.h"
#include "../ExecutionTracers/TranslationBlockTracer.h"

#include <s2e/ConfigFile.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>

#include <llvm/Support/TimeValue.h>

#include <klee/ExecutionState.h>

#include <fstream>
#include <queue>
#include <cmath>
#include <unistd.h>


using namespace llvm;
using namespace klee;


namespace s2e {
namespace plugins {

typedef enum {
	CONCOLIC_RET_OK,
	CONCOLIC_RET_TOO_SMALL,
	CONCOLIC_RET_ERROR
} ConcolicStatus;


typedef enum {
	START_CONCOLIC_SESSION,
	END_CONCOLIC_SESSION
} ConcolicCommand;


typedef struct {
	ConcolicCommand command;
	uint32_t max_time;
	uint8_t is_error_path;
	uint32_t result_ptr;
	uint32_t result_size;
} __attribute__((packed)) ConcolicMessage;


////////////////////////////////////////////////////////////////////////////////


HighLevelTreeNode* TraceNodeHash::operator()(S2EExecutionState *state) {
	if (monitor_ == NULL) {
		monitor_ = static_cast<InterpreterMonitor*>(
				g_s2e->getPlugin("InterpreterMonitor"));
		assert(monitor_ != NULL);
	}
	return monitor_->getHLTreeNode(state);
}


double ForkWeight::operator()(S2EExecutionState *state) {
	if (concolic_session_ == NULL) {
		concolic_session_ = static_cast<ConcolicSession*>(
				g_s2e->getPlugin("ConcolicSession"));
		assert(concolic_session_ != NULL);
	}

	double weight = concolic_session_->getForkWeight(state);

	return weight;
}


////////////////////////////////////////////////////////////////////////////////


S2E_DEFINE_PLUGIN(ConcolicSession,
		"Support user-controlled concolic executions of pieces of code.",
		"", "InterpreterMonitor");

ConcolicSession::ConcolicSession(S2E* s2e_)
	: Plugin(s2e_),
	  cfg_tc_stream_(NULL),
	  paths_tc_stream_(NULL),
	  all_tc_stream_(NULL),
	  compl_feature_stream_(NULL),
	  pending_feature_stream_(NULL),
	  stop_on_error_(true),
	  use_random_pending_(false),
	  use_weighting_(false),
	  tree_dump_interval_(0),
	  state_time_out_(0),
	  extra_details_(false),
	  interp_monitor_(NULL),
	  root_fork_point_(NULL),
	  active_state_(NULL),
	  tree_divergence_node_(NULL),
	  cfg_divergence_node_(NULL),
	  pending_states_(NULL),
	  starting_fork_point_(NULL),
	  active_fork_point_(NULL),
	  active_fork_index_(0),
	  fork_weight_pc_(0),
	  start_time_stamp_(sys::TimeValue::ZeroTime),
	  path_time_stamp_(sys::TimeValue::ZeroTime),
	  session_deadline_(sys::TimeValue::ZeroTime),
	  path_deadline_(sys::TimeValue::ZeroTime),
	  next_dump_stamp_(sys::TimeValue::ZeroTime),
	  memory_tracer_(NULL),
	  tb_tracer_(NULL) {

	out_searcher_ = new DFSSearcher();
}


ConcolicSession::~ConcolicSession() {
	if (pending_states_ != NULL)
		delete pending_states_;

	if (pending_feature_stream_ != NULL)
		delete pending_feature_stream_;
	if (compl_feature_stream_ != NULL)
		delete compl_feature_stream_;
	if (cfg_tc_stream_ != NULL)
		delete cfg_tc_stream_;
	if (paths_tc_stream_ != NULL)
		delete paths_tc_stream_;
	if (all_tc_stream_ != NULL)
		delete all_tc_stream_;

	delete out_searcher_;
}


void ConcolicSession::initialize() {
	stop_on_error_ = s2e()->getConfig()->getBool(getConfigKey() + ".stopOnError", false);
	use_random_pending_ = s2e()->getConfig()->getBool(getConfigKey() + ".useRandomPending", false);
	use_weighting_ = s2e()->getConfig()->getBool(getConfigKey() + ".useWeighting", false);
	tree_dump_interval_ = s2e()->getConfig()->getInt(getConfigKey() + ".treeDumpInterval", 60);
	state_time_out_ = s2e()->getConfig()->getInt(getConfigKey() + ".stateTimeOut", 60);
	extra_details_ = s2e()->getConfig()->getBool(getConfigKey() + ".extraDetails", false);

	cfg_tc_stream_ = s2e()->openOutputFile("cfg_test_cases.dat");
	paths_tc_stream_ = s2e()->openOutputFile("hl_test_cases.dat");
	all_tc_stream_ = s2e()->openOutputFile("all_test_cases.dat");
	compl_feature_stream_ = s2e()->openOutputFile("complete_features.dat");
	pending_feature_stream_ = s2e()->openOutputFile("pending_features.dat");

	memory_tracer_ = static_cast<MemoryTracer*>(
			s2e()->getPlugin("MemoryTracer"));
	tb_tracer_ = static_cast<TranslationBlockTracer*>(
			s2e()->getPlugin("TranslationBlockTracer"));

	interp_monitor_ = static_cast<InterpreterMonitor*>(
			s2e()->getPlugin("InterpreterMonitor"));

	if (use_random_pending_) {
		typedef RandomSelector<S2EExecutionState*> RandomPendingSelector;
		pending_states_ = new RandomPendingSelector();
	} else if (use_weighting_) {
		typedef WeightedRandomSelector<HighLevelInstruction*,
				MinDistanceToUncovWeight> ClassKeySelector;
		typedef WeightedRandomSelector<S2EExecutionState*, ForkWeight>
			ClassValueSelector;
		typedef ClassSelector<S2EExecutionState*, HighLevelInstruction*,
				HighLevelInstructionHash, ClassValueSelector, ClassKeySelector>
				WeightingSelector;
		pending_states_ = new WeightingSelector();
	} else {
		typedef ClassSelector<S2EExecutionState*, HighLevelTreeNode*, TraceNodeHash,
				  ClassSelector<S2EExecutionState*,
								uint64_t,
								ProgramCounterHash> > CUPASelector;

		pending_states_ = new CUPASelector();
	}

	s2e()->getExecutor()->setSearcher(this);
}


unsigned int ConcolicSession::handleOpcodeInvocation(S2EExecutionState *state,
		uint64_t guestDataPtr, uint64_t guestDataSize) {

	if (guestDataSize < sizeof(ConcolicMessage)) {
		return CONCOLIC_RET_ERROR;
	}

	ConcolicMessage message;

	if (!state->readMemoryConcrete(guestDataPtr, &message,
			sizeof(message), S2EExecutionState::VirtualAddress)) {
		return CONCOLIC_RET_ERROR;
	}

	int result;

	switch (message.command) {
	case START_CONCOLIC_SESSION:
		result = startConcolicSession(state, message.max_time);
		break;
	case END_CONCOLIC_SESSION:
		result = endConcolicSession(state, message.is_error_path);
		break;
	default:
		assert(0 && "Invalid concolic session command");
		break;
	}

	return result;
}


int ConcolicSession::startConcolicSession(S2EExecutionState *state,
		uint32_t max_time) {
	assert(active_state_ == NULL);

	// XXX: Move this in a separate protocol call
	flushPathConstraints(state);

	interp_monitor_->startTrace(state);

	active_state_ = state;
	tree_divergence_node_ = NULL;
	cfg_divergence_node_ = NULL;

	start_time_stamp_ = sys::TimeValue::now();
	path_time_stamp_ = start_time_stamp_;
	path_deadline_ = start_time_stamp_ + sys::TimeValue((int64_t)state_time_out_);

	if (max_time) {
		session_deadline_ = start_time_stamp_ + sys::TimeValue((int64_t)max_time);
	} else {
		session_deadline_ = sys::TimeValue::ZeroTime;
	}

	if (tree_dump_interval_) {
		next_dump_stamp_ = start_time_stamp_
				+ sys::TimeValue((int64_t)tree_dump_interval_);
	} else {
		next_dump_stamp_ = sys::TimeValue::ZeroTime;
	}

	// Fork weight computation
	fork_weight_pc_ = 0;
	fork_strike_.clear();

	// Fork points
	root_fork_point_ = new ForkPoint(NULL, -1, active_state_->getPc(),
			interp_monitor_->getHLTreeNode(active_state_), 1);
	starting_fork_point_ = root_fork_point_;
	active_fork_point_ = root_fork_point_;
	active_fork_index_ = 0;

	if (memory_tracer_) {
		memory_tracer_->enableTracing();
	}
	if (tb_tracer_) {
		tb_tracer_->enableTracing();
	}

	// Activate the callbacks
	on_state_fork_ = s2e()->getCorePlugin()->onStateFork.connect(
			sigc::mem_fun(*this, &ConcolicSession::onStateFork));
	on_state_kill_ = s2e()->getCorePlugin()->onStateKill.connect(
			sigc::mem_fun(*this, &ConcolicSession::onStateKill));
	on_state_switch_ = s2e()->getCorePlugin()->onStateSwitch.connect(
			sigc::mem_fun(*this, &ConcolicSession::onStateSwitch));
	on_timer_ = s2e()->getCorePlugin()->onTimer.connect(
			sigc::mem_fun(*this, &ConcolicSession::onTimer));
	on_interpreter_trace_ = interp_monitor_->on_hlpc_update.connect(
			sigc::mem_fun(*this, &ConcolicSession::onInterpreterTrace));

	s2e()->getMessagesStream(state) << "***** CONCOLIC SESSION - START *****"
			<< '\n';
	if (use_random_pending_) {
		s2e()->getMessagesStream(state) << "Using random state selection." << '\n';
	}

	return CONCOLIC_RET_OK;
}


int ConcolicSession::endConcolicSession(S2EExecutionState *state,
		bool is_error_path) {
	assert(active_state_ != NULL);

	// Disable the path deadline while executing this routine
	path_deadline_ = sys::TimeValue::ZeroTime;

	HighLevelTreeNode *trace_node = interp_monitor_->getHLTreeNode(state);

	if (is_error_path && stop_on_error_) {
		assert(trace_node->path_counter() == 1
				&& "How could you miss it the first time?");
		s2e()->getMessagesStream(state) << "Error path hit!" << '\n';
	} else {
		assert(trace_node->path_counter() > 0);
	}

	llvm::sys::TimeValue time_stamp = sys::TimeValue::now();

	s2e()->getMessagesStream(state) << "Processing test case for "
			<< klee::concolics(state) << '\n';

	if (interp_monitor_->cfg().changed()) {
		assert(trace_node->path_counter() == 1
				&& "How could you miss it the first time?");
		s2e()->getMessagesStream(state) << "New CFG fragment discovered!"
				<< '\n';

		dumpTestCase(state, time_stamp, time_stamp - path_time_stamp_,
				*cfg_tc_stream_);
	}

	if (trace_node->path_counter() == 1) {
		s2e()->getMessagesStream(state) << "New HL tree path!" << '\n';

		dumpTestCase(state, time_stamp, time_stamp - path_time_stamp_,
				*paths_tc_stream_);
	}

	dumpTestCase(state, time_stamp, time_stamp - path_time_stamp_,
			*all_tc_stream_);

	interp_monitor_->cfg().analyzeCFG();
	pending_states_->updateWeights();

	// Measure this again since the CFG analysis may be expensive
	time_stamp = sys::TimeValue::now();

	if (emptyPendingStates() || (is_error_path && stop_on_error_)) {
		s2e()->getMessagesStream(state)
				<< (is_error_path ? "Premature termination."
						: "Exhaustive search complete.")
			    << '\n';
		terminateSession(state);
	} else {
		selectPendingState(state);
		path_time_stamp_ = time_stamp;
		path_deadline_ = path_time_stamp_ + sys::TimeValue((int64_t)state_time_out_);
	}

	// s2e()->getExecutor()->yieldState(*state);
	s2e()->getExecutor()->terminateState(*state);
	// Unreachable at this point

	return CONCOLIC_RET_OK;
}


bool ConcolicSession::writeToGuest(S2EExecutionState *state,
		uint64_t address, void *buf, uint64_t size) {
	bool result = state->writeMemoryConcrete(address, buf, size, S2EExecutionState::VirtualAddress);
	if (result) {
		s2e()->getMessagesStream(state) << "Wrote " << size
				<< " bytes in range " << hexval(address) << "-"
				<< hexval(address + size - 1)
				<< '\n';
	}
	return result;
}


void ConcolicSession::flushPathConstraints(S2EExecutionState *state) {
	state->constraints.flush();
	state->concolics.clear();
	state->symbolics.clear();
}


void ConcolicSession::dumpTestCase(S2EExecutionState *state,
		llvm::sys::TimeValue time_stamp,
		llvm::sys::TimeValue total_delta,
		llvm::raw_ostream &out) {
	out << (time_stamp - start_time_stamp_).usec();
	out << " " << hexval(starting_fork_point_->pc());

	if (extra_details_) {
		int min_dist, max_dist;
		computeMinMaxDistToUncovered(state, min_dist, max_dist);

		HighLevelTreeNode *starting_node = starting_fork_point_->hl_node();

		out << " " << starting_node->instruction()->dist_to_uncovered();

		if (tree_divergence_node_) {
			out << " " << tree_divergence_node_->distanceToAncestor(starting_node)
					<< "/" << interp_monitor_->cfg().computeMinDistance(
							starting_node->instruction(),
							tree_divergence_node_->instruction());
		} else {
			out << " " << "-/-";
		}

		if (cfg_divergence_node_) {
			out << " " << cfg_divergence_node_->distanceToAncestor(starting_node)
					<< "/" << interp_monitor_->cfg().computeMinDistance(
							starting_node->instruction(),
							cfg_divergence_node_->instruction());
		} else {
			out << " " << "-/-";
		}

		out << " " << min_dist << "/" << max_dist;
	}

	klee::Assignment assignment = state->concolics;

	for (klee::Assignment::bindings_ty::iterator
			bit = assignment.bindings.begin(),
			bie = assignment.bindings.end(); bit != bie; ++bit) {
		std::string assgn_value(bit->second.begin(), bit->second.end());

		out << " " << bit->first->getOriginalName() << "=>";
		out << hexstring(assgn_value);
	}

	out << '\n';
	out.flush();
}


void ConcolicSession::computeMinMaxDistToUncovered(S2EExecutionState *state,
		int &min_dist, int &max_dist) {
	HighLevelTreeNode *node = interp_monitor_->getHLTreeNode(state);
	assert(node && "Terminating state must already have a tree node.");

	min_dist = -1;
	max_dist = -1;

	while (node != NULL) {
		int dist = node->instruction()->dist_to_uncovered();
		if (dist > 0) {
			if (max_dist < 0 || dist > max_dist) {
				max_dist = dist;
			}
			if (min_dist < 0 || dist < min_dist) {
				min_dist = dist;
			}
		}
		node = node->parent();
	}
}


bool ConcolicSession::emptyPendingStates() {
	return pending_states_->empty();
}


void ConcolicSession::selectPendingState(S2EExecutionState *state) {
	active_state_ = pending_states_->select();

	pending_states_->erase(active_state_);

	pending_fork_weights_.erase(active_state_);

	active_fork_point_ = pending_fork_points_[active_state_].first;
	active_fork_index_ = pending_fork_points_[active_state_].second;
	starting_fork_point_ = active_fork_point_;
	pending_fork_points_.erase(active_state_);

	tree_divergence_node_ = NULL;
	cfg_divergence_node_ = NULL;

	s2e()->getMessagesStream(active_state_) << "Switched to state "
				<< klee::concolics(active_state_) << '\n';
}


void ConcolicSession::insertPendingState(S2EExecutionState *state) {
	pending_states_->insert(state);

	uint64_t fork_pc = state->getPc();
	pending_fork_weights_[state] = 1.0;

	if (fork_pc == fork_weight_pc_) {
		// Halve the selection probability of the other strikers
		for (StateVector::iterator it = fork_strike_.begin(),
				ie = fork_strike_.end(); it != ie; ++it) {
			if (pending_fork_weights_.count(*it)) {
				pending_fork_weights_[*it] *= 0.75;
			}
		}
	} else {
		fork_weight_pc_ = fork_pc;
		fork_strike_.clear();
	}

	fork_strike_.push_back(state);
}


void ConcolicSession::copyAndClearPendingStates(StateVector &states) {
	pending_states_->copyTo(states);
	pending_states_->clear();
	pending_fork_weights_.clear();
	pending_fork_points_.clear();
}


double ConcolicSession::getForkWeight(S2EExecutionState *state) {
	ForkWeightMap::iterator it = pending_fork_weights_.find(state);

	return (it == pending_fork_weights_.end()) ? 1.0 : it->second;
}


void ConcolicSession::terminateSession(S2EExecutionState *state) {
	if (tb_tracer_) {
		tb_tracer_->disableTracing();
	}
	if (memory_tracer_) {
		memory_tracer_->disableTracing();
	}

	dumpTraceGraphs();

	if (!emptyPendingStates()) {
		StateVector state_vector;
		copyAndClearPendingStates(state_vector);

		s2e()->getMessagesStream(state) << "Terminating "
				<< state_vector.size() << " pending states." << '\n';

		for (StateVector::iterator it = state_vector.begin(),
				ie = state_vector.end(); it != ie; ++it) {
			S2EExecutionState *pending_state = *it;
#if 0 // TODO: Make this a configurable option
			dumpTestCase(pending_state, timed_out_test_cases_);
#endif
			s2e()->getExecutor()->terminateState(*pending_state);
		}
	}

	s2e()->getMessagesStream(state) << "***** CONCOLIC SESSION - END *****" << '\n';

	active_state_ = NULL;

	on_timer_.disconnect();
	on_state_fork_.disconnect();
	on_state_kill_.disconnect();
	on_state_switch_.disconnect();
	on_interpreter_trace_.disconnect();

	interp_monitor_->stopTrace(state);

	root_fork_point_->clear();
	delete root_fork_point_;
	root_fork_point_ = NULL;
}


void ConcolicSession::onInterpreterTrace(S2EExecutionState *state,
		HighLevelTreeNode *tree_node) {
	assert(state == active_state_);

	// Clear any lucky strike
	// fork_strike_.clear();

	if (tree_divergence_node_ == NULL && tree_node->path_counter() == 1) {
		tree_divergence_node_ = tree_node;
	}
	if (cfg_divergence_node_ == NULL && interp_monitor_->cfg().changed()) {
		cfg_divergence_node_ = tree_node;
	}
}


void ConcolicSession::onStateFork(S2EExecutionState *state,
		const StateVector &newStates,
		const std::vector<ref<Expr> > &newConditions) {
	assert(state == active_state_);

	active_fork_point_ = new ForkPoint(active_fork_point_, active_fork_index_,
			active_state_->getPc(),
			interp_monitor_->getHLTreeNode(active_state_),
			newStates.size() + 1);
	active_fork_index_ = 0;

	for (int i = 0; i < newStates.size(); ++i) {
		S2EExecutionState *new_state = newStates[i];

		if (new_state == state)
			continue;

		pending_fork_points_[new_state].first = active_fork_point_;
		pending_fork_points_[new_state].second = (i + 1);

		insertPendingState(new_state);
	}
}


void ConcolicSession::onStateKill(S2EExecutionState *state) {
	assert(active_state_ != NULL);

	if (state != active_state_) {
		// This happens when a state was killed at the end of
		// the scheduleNextState call. The active_state_ is updated before
		// killing the current state.
		return;
	}

	// In case of an unplanned kill, we must schedule a new alternate
	endConcolicSession(state, false);
}


void ConcolicSession::onStateSwitch(S2EExecutionState *old_state,
			S2EExecutionState *new_state) {

}


void ConcolicSession::onTimer() {
	if (active_state_ == NULL) {
		return;
	}

	sys::TimeValue time_stamp = sys::TimeValue::now();
	S2EExecutionState *state = active_state_;

	if (path_deadline_ != sys::TimeValue::ZeroTime && time_stamp >= path_deadline_) {
		s2e()->getMessagesStream(state) << "State time-out." << '\n';
		endConcolicSession(state, true);
		return;
	}

	if (session_deadline_ == sys::TimeValue::ZeroTime
			&& next_dump_stamp_ == sys::TimeValue::ZeroTime)
		return;

	if (session_deadline_ != sys::TimeValue::ZeroTime && time_stamp >= session_deadline_) {
		s2e()->getMessagesStream(state) << "Concolic session time-out."
				<< '\n';
		terminateSession(state);
		s2e()->getExecutor()->terminateState(*state);
		return;
	}

	if (next_dump_stamp_ != sys::TimeValue::ZeroTime && time_stamp >= next_dump_stamp_) {
		s2e()->getMessagesStream(state) << "Dumping execution tree." << '\n';
		dumpTraceGraphs();
		next_dump_stamp_ = time_stamp + sys::TimeValue((int64_t)tree_dump_interval_);
	}
}


klee::ExecutionState &ConcolicSession::selectState() {
	if (active_state_ == NULL) {
		return out_searcher_->selectState();
	}

	return *active_state_;
}


void ConcolicSession::update(klee::ExecutionState *current,
            const std::set<klee::ExecutionState*> &addedStates,
            const std::set<klee::ExecutionState*> &removedStates) {
	out_searcher_->update(current, addedStates, removedStates);

	// Nothing do to here.
	// Handle state management through the onStateFork/onStateKill hooks.
}


bool ConcolicSession::empty() {
	return out_searcher_->empty();
}


void ConcolicSession::dumpTraceGraphs() {
	std::ofstream tree_of;
	std::string file_name = s2e()->getNextOutputFilename("interp_tree.dot");
	tree_of.open(file_name.c_str());
	assert(tree_of.good() && "Could not open interpreter tree dump");

	interp_monitor_->dumpHighLevelTree(tree_of);
	tree_of.close();

	std::ofstream cfg_of;
	file_name = s2e()->getNextOutputFilename("interp_cfg.dot");
	cfg_of.open(file_name.c_str());
	assert(cfg_of.good() && "Could not open interpreter CFG dump");

	interp_monitor_->dumpHighLevelCFG(cfg_of);
	cfg_of.close();
}


} /* namespace plugins */
} /* namespace s2e */
