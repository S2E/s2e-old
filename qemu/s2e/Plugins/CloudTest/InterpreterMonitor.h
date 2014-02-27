/*
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

#ifndef INTERPRETERMONITOR_H_
#define INTERPRETERMONITOR_H_

#include <s2e/Plugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Signals/Signals.h>
#include <s2e/Utils.h>

#include "../BaseInstructions.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/raw_ostream.h>

#include <ostream>


namespace s2e {
namespace plugins {


typedef llvm::SmallVector<uint32_t, 2> HighLevelPC;
typedef uint32_t HighLevelOpcode;

inline llvm::raw_ostream& operator<<(llvm::raw_ostream &os,
		const HighLevelPC& t) {
	os << '[';
	for (HighLevelPC::const_reverse_iterator it = t.rbegin(), ie = t.rend();
			it != ie; ++it) {
		if (it != t.rbegin()) {
			os << '/';
		}
		os << hexval(*it);
	}
	os << ']';
	return os;
}

inline std::ostream& operator<<(std::ostream &os, const HighLevelPC &t) {
	os << '[';
	for (HighLevelPC::const_reverse_iterator it = t.rbegin(), ie = t.rend();
			it != ie; ++it) {
		if (it != t.rbegin()) {
			os << '/';
		}
		os << hexval(*it);
	}
	os << ']';
	return os;
}


class HighLevelInstruction {
public:
	typedef std::map<HighLevelPC, HighLevelInstruction*> AdjacencyMap;

	const HighLevelPC &hlpc() const {
		return hlpc_;
	}

	HighLevelOpcode opcode() const {
		return opcode_;
	}

	const AdjacencyMap &successors() const {
		return successors_;
	}

	const AdjacencyMap &predecessors() const {
		return predecessors_;
	}

	HighLevelInstruction *next() {
		assert(successors_.size() == 1);
		return successors_.begin()->second;
	}

	const HighLevelInstruction *next() const {
		assert(successors_.size() == 1);
		return successors_.begin()->second;
	}

	int high_level_paths() const {
		return high_level_paths_;
	}

	int dist_to_uncovered() const {
		return dist_to_uncovered_;
	}

	int low_level_paths_;
	int fork_counter_;

private:
	HighLevelInstruction(const HighLevelPC &hlpc, HighLevelOpcode opcode)
		: low_level_paths_(0),
		  fork_counter_(0),
		  high_level_paths_(0),
		  dist_to_uncovered_(0),
		  hlpc_(hlpc),
		  opcode_(opcode) {

	}

	int high_level_paths_;
	int dist_to_uncovered_;

	HighLevelPC hlpc_;
	HighLevelOpcode opcode_;

	AdjacencyMap successors_;
	AdjacencyMap predecessors_;

	friend class HighLevelCFG;
	friend class HighLevelTreeNode;

	// Disallow copy and assign
	HighLevelInstruction(const HighLevelInstruction &);
	void operator=(const HighLevelInstruction &);
};


class HighLevelBasicBlock {
public:
	typedef std::vector<HighLevelBasicBlock*> AdjacencyList;
	typedef std::set<HighLevelBasicBlock*> DominatorSet;

	HighLevelInstruction *head() const {
		return head_;
	}

	HighLevelInstruction *tail() const {
		return tail_;
	}

	int size() const {
		return size_;
	}

	const AdjacencyList &successors() const {
		return successors_;
	}

	const AdjacencyList &predecessors() const {
		return predecessors_;
	}

	const DominatorSet &dominators() const {
		return dominators_;
	}

private:
	HighLevelBasicBlock(HighLevelInstruction *head,
			HighLevelInstruction *tail = 0, int size = 0) :
			head_(head), tail_(tail), size_(size) {

	}

	HighLevelInstruction *head_;
	HighLevelInstruction *tail_;
	int size_;

	AdjacencyList successors_;
	AdjacencyList predecessors_;
	DominatorSet dominators_;

	friend class HighLevelCFG;

	// Disallow copy and assign
	HighLevelBasicBlock(const HighLevelBasicBlock &);
	void operator=(const HighLevelBasicBlock &);
};


class HighLevelCFG {
public:
	typedef std::map<HighLevelPC, HighLevelInstruction*> InstructionMap;
	typedef std::vector<HighLevelBasicBlock*> BasicBlockList;
	typedef std::map<HighLevelOpcode, unsigned> BranchOpcodeSet;

	HighLevelCFG(llvm::raw_ostream &debug_stream)
		: debug_stream_(debug_stream),
		  changed_(false) {

	}

	HighLevelInstruction* recordEdge(const HighLevelPC &source,
			const HighLevelPC &dest, HighLevelOpcode opcode);

	HighLevelInstruction* recordNode(const HighLevelPC &hlpc);

	bool changed() const {
		return changed_;
	}

	void clear();

	const InstructionMap &instructions() const {
		return instructions_;
	}

	const BasicBlockList &basic_blocks() const {
		return basic_blocks_;
	}

	bool analyzeCFG();

	bool isBranchInstruction(const HighLevelInstruction* inst);
	int computeMinDistance(const HighLevelInstruction *source,
			const HighLevelInstruction *dest) const;
private:
	void clearBasicBlocks();
	void extractBasicBlocks();
	void computeDominatorTree();
	void extractBranchOpcodes();
	void computeDistanceToUncovered();

	llvm::raw_ostream &debug_stream_;

	bool changed_;

	InstructionMap instructions_;
	BasicBlockList basic_blocks_;
	BranchOpcodeSet branch_opcodes_;

	// Disallow copy and assign
	HighLevelCFG(const HighLevelCFG &);
	void operator=(const HighLevelCFG &);
};


class HighLevelTreeNode {
public:
	typedef std::map<HighLevelPC, HighLevelTreeNode*> AdjacencyMap;

	HighLevelTreeNode(HighLevelInstruction *instruction,
			HighLevelTreeNode *parent) :
		path_counter_(0),
		fork_counter_(0),
		instruction_(instruction),
		parent_(parent) {

		instruction_->high_level_paths_++;
	}

	virtual ~HighLevelTreeNode() {
		assert(children_.empty());
	}

	HighLevelInstruction *instruction() {
		return instruction_;
	}

	HighLevelTreeNode *parent() const {
		return parent_;
	}

	HighLevelTreeNode *getOrCreateSuccessor(HighLevelInstruction *instruction) {
		AdjacencyMap::iterator it = children_.find(instruction->hlpc());
		if (it != children_.end())
			return it->second;

		HighLevelTreeNode *node = new HighLevelTreeNode(instruction, this);
		children_.insert(std::make_pair(instruction->hlpc(), node));
		return node;
	}

	void clear() {
		for (AdjacencyMap::iterator it = children_.begin(),
				ie = children_.end(); it != ie; ++it) {
			HighLevelTreeNode *successor = it->second;
			successor->clear();
			delete successor;
		}
		children_.clear();
	}

	const AdjacencyMap &successors() const {
		return children_;
	}

	int path_counter() const {
		return path_counter_;
	}

	int fork_counter() const {
		return fork_counter_;
	}

	void bumpPathCounter() {
		path_counter_++;
		instruction_->low_level_paths_++;
	}

	void bumpForkCounter() {
		fork_counter_++;
		instruction_->fork_counter_++;
	}

	int distanceToAncestor(HighLevelTreeNode *node) {
		int counter = 0;
		HighLevelTreeNode *current = this;

		while (current != node) {
			current = current->parent_;
			counter++;
			if (current == NULL)
				return -1;
		}
		return counter;
	}

private:
	int path_counter_;
	int fork_counter_;

	HighLevelInstruction *instruction_;
	HighLevelTreeNode *parent_;
	AdjacencyMap children_;

	// Disallow copy and assign
	HighLevelTreeNode(const HighLevelTreeNode&);
	void operator=(const HighLevelTreeNode&);
};


template<class Value,
         class IDType = int>
class IDProvider {
public:
	IDProvider() : id_counter_(0) {

	}

	IDType getID() {
		return ++id_counter_;
	}

	IDType getID(const Value &value) {
		typename std::map<Value, IDType>::iterator it = assigned_ids_.find(value);

		if (it != assigned_ids_.end()) {
			return it->second;
		}

		assigned_ids_[value] = (++id_counter_);

		return assigned_ids_[value];
	}

private:
	IDType id_counter_;
	std::map<Value, IDType> assigned_ids_;
};


class GraphVisualizer {
public:
	typedef std::map<std::string, std::string> AttributeMap;

	GraphVisualizer(std::ostream &os) : os_(os) {

	}

	std::ostream &os() {
		return os_;
	}

protected:
	void drawNode(const std::string &name, const AttributeMap &attributes);
	void drawNode(int id, const AttributeMap &attributes);

	void drawEdge(const std::string &source, const std::string &dest,
			const AttributeMap &attributes);
	void drawEdge(int source, int dest, const AttributeMap &attributes);

private:
	void recordAttributes(const AttributeMap &attributes);

	std::ostream &os_;

	// Disallow copy and assign
	GraphVisualizer(const GraphVisualizer&);
	void operator=(const GraphVisualizer&);
};


class HighLevelTreeVisualizer: public GraphVisualizer {
public:
	HighLevelTreeVisualizer(std::ostream &os)
		: GraphVisualizer(os),
		  max_path_count_(0) {

	}

	void dumpTree(HighLevelTreeNode *root);

private:
	typedef std::map<HighLevelTreeNode*, int> NodeNameMap;

	typedef enum {
		NORMAL = 0,
		INTERN = 1,
		TERMINAL = 2
	} NodeType;

	void preprocessTree(HighLevelTreeNode *root);

	void printTreeNode(HighLevelTreeNode *node, NodeType node_type,
			int ref_path_count);

	IDProvider<HighLevelTreeNode*> node_names_;
	int max_path_count_;

	// Disallow copy and assign
	HighLevelTreeVisualizer(const HighLevelTreeVisualizer&);
	void operator=(const HighLevelTreeVisualizer&);
};


class HighLevelCFGVisualizer: public GraphVisualizer {
public:
	HighLevelCFGVisualizer(std::ostream &os) :
		GraphVisualizer(os),
		max_hl_path_count_(0) {

	}

	void dumpCFG(HighLevelCFG &cfg);

private:
	void printBasicBlock(const HighLevelBasicBlock *bb);

	void printInstructionSeq(HighLevelInstruction *head,
			HighLevelInstruction *tail, std::ostream &os);
	void printInstruction(HighLevelInstruction *instr, std::ostream &os);

	IDProvider<const HighLevelBasicBlock*> bb_names_;
	int max_hl_path_count_;
};


class InterpreterMonitor: public Plugin,
			              public BaseInstructionsPluginInvokerInterface {
	S2E_PLUGIN
public:
	InterpreterMonitor(S2E *s2e);

	virtual ~InterpreterMonitor() {}

	void startTrace(S2EExecutionState *state);
	void stopTrace(S2EExecutionState *state);

	HighLevelTreeNode *getHLTreeNode(S2EExecutionState *state) const;
	void dumpHighLevelTree(std::ostream &os);
	void dumpHighLevelCFG(std::ostream &os);

	HighLevelCFG &cfg() {
		return cfg_;
	}

	bool active() const {
		return root_node_ != NULL;
	}

	void initialize();
	virtual unsigned int handleOpcodeInvocation(S2EExecutionState *state,
				uint64_t guestDataPtr, uint64_t guestDataSize);

	// Provided connections

	sigc::signal<void, S2EExecutionState*, HighLevelTreeNode*> on_hlpc_update;

private:
	typedef std::vector<S2EExecutionState*> StateVector;
	typedef std::map<S2EExecutionState*, HighLevelTreeNode*> StateNodeMapping;

	HighLevelCFG cfg_;

	HighLevelTreeNode *root_node_;
	HighLevelTreeNode *active_node_;
	S2EExecutionState *active_state_;
	StateNodeMapping state_mapping_;

	// Required connections
	sigc::connection on_state_fork_;
	sigc::connection on_state_switch_;
	sigc::connection on_state_kill_;

	void doUpdateHLPC(S2EExecutionState *state, const HighLevelPC &hlpc,
				HighLevelOpcode opcode);

	void onStateFork(S2EExecutionState *state, const StateVector &new_states,
			const std::vector<klee::ref<klee::Expr> > &new_conditions);
	void onStateSwitch(S2EExecutionState *state, S2EExecutionState *new_state);
	void onStateKill(S2EExecutionState *state);
};


} /* namespace plugins */
} /* namespace s2e */

#endif /* INTERPRETERMONITOR_H_ */
