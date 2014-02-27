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

#include "InterpreterMonitor.h"

#include <s2e/S2E.h>

#include <algorithm>
#include <queue>
#include <sstream>

namespace {
struct cmp_by_second {
	template <class T>
	bool operator()(const T &x, const T &y) const {
		return x.second < y.second;
	}
};
}

namespace s2e {
namespace plugins {


typedef struct {
	uint32_t op_code;
	uint32_t frame_count;
	uint32_t frames[1];
} __attribute__((packed)) TraceUpdate;


////////////////////////////////////////////////////////////////////////////////


HighLevelInstruction* HighLevelCFG::recordEdge(const HighLevelPC &source,
		const HighLevelPC &dest, HighLevelOpcode opcode) {
	HighLevelInstruction *dst_inst;

	InstructionMap::iterator src_it = instructions_.find(source);
	InstructionMap::iterator dst_it = instructions_.find(dest);

	bool result;

	if (dst_it == instructions_.end()) {
		dst_inst = new HighLevelInstruction(dest, opcode);
		result = instructions_.insert(std::make_pair(dest, dst_inst)).second;
		changed_ = changed_ || result;

	} else {
		dst_inst = dst_it->second;
	}

	if (src_it != instructions_.end()) {
		HighLevelInstruction *src_inst = src_it->second;
		result = src_inst->successors_.insert(std::make_pair(dest, dst_inst)).second;
		changed_ = changed_ || result;

		result = dst_inst->predecessors_.insert(std::make_pair(source, src_inst)).second;
		changed_ = changed_ || result;
	}

	return dst_inst;
}

HighLevelInstruction* HighLevelCFG::recordNode(const HighLevelPC &hlpc) {
	InstructionMap::iterator it = instructions_.find(hlpc);

	if (it != instructions_.end()) {
		return it->second;
	}

	HighLevelInstruction *inst = new HighLevelInstruction(hlpc,
			HighLevelOpcode());
	changed_ = changed_ ||
			instructions_.insert(std::make_pair(hlpc, inst)).second;
	return inst;
}


void HighLevelCFG::clear() {
	clearBasicBlocks();

	for (InstructionMap::iterator it = instructions_.begin(),
			ie = instructions_.end(); it != ie; ++it) {
		HighLevelInstruction *inst = it->second;
		delete inst;
	}
	instructions_.clear();
}


void HighLevelCFG::clearBasicBlocks() {
	for (BasicBlockList::iterator it = basic_blocks_.begin(),
			ie = basic_blocks_.end(); it != ie; ++it) {
		HighLevelBasicBlock *basic_block = *it;
		delete basic_block;
	}
	basic_blocks_.clear();
}

bool HighLevelCFG::analyzeCFG() {
	if (!changed_) {
		debug_stream_ << "CFG unchanged, skipping analysis." << '\n';
		return false;
	}

	debug_stream_ << "=== Starting CFG analysis ===" << '\n';
	debug_stream_ << instructions_.size() << " total instructions to process."
			<< '\n';

	if (!basic_blocks_.empty()) {
		debug_stream_ << "Clearing previous basic blocks." << '\n';
		clearBasicBlocks();
	}

	extractBasicBlocks();
	computeDominatorTree();

	extractBranchOpcodes();
	computeDistanceToUncovered();

	changed_ = false;

	debug_stream_ << "=== CFG analysis complete ===" << '\n';
	return true;
}

void HighLevelCFG::extractBasicBlocks() {
	assert(basic_blocks_.empty());

	std::queue<HighLevelBasicBlock*> bb_queue;
	std::map<HighLevelInstruction*, HighLevelBasicBlock*> bb_map;
	std::set<HighLevelInstruction*> marked;

	HighLevelInstruction *entry_inst = instructions_.find(HighLevelPC())->second;
	HighLevelBasicBlock *entry_bb = new HighLevelBasicBlock(entry_inst);

	bb_map[entry_inst] = entry_bb;
	marked.insert(entry_inst);
	bb_queue.push(entry_bb);

	while (!bb_queue.empty()) {
		HighLevelBasicBlock *bb = bb_queue.front();
		bb_queue.pop();

		int bb_size = 1;
		HighLevelInstruction *current = bb->head_;

		// Progress through instruction and stop at the end of the BB
		for (;;) {
			if (current->successors().size() != 1) {
				break;
			}
			// The candidate for the next instruction in the BB
			HighLevelInstruction *next = current->next();
			assert(next->predecessors().size() >= 1);

			// Abort if the candidate doesn't seem to be in the BB
			if (next->predecessors().size() > 1 || marked.count(next) > 0) {
				break;
			}

			current = next;
			marked.insert(current);
			bb_size++;
		}
		bb->tail_ = current;
		bb->size_ = bb_size;

		for (HighLevelInstruction::AdjacencyMap::const_iterator
				it = current->successors_.begin(),
				ie = current->successors_.end(); it != ie; ++it) {
			HighLevelInstruction *next = it->second;

			if (marked.count(next) == 0) {
				HighLevelBasicBlock *next_bb = new HighLevelBasicBlock(next);

				bb_map[next] = next_bb;
				marked.insert(next);
				bb_queue.push(next_bb);
			}

			bb->successors_.push_back(bb_map[next]);
			bb_map[next]->predecessors_.push_back(bb);
		}

		basic_blocks_.push_back(bb);
	}

	debug_stream_ << "Extracted " << basic_blocks_.size()
			<< " basic blocks" << '\n';
}

void HighLevelCFG::computeDominatorTree() {
	if (basic_blocks_.empty()) {
		return;
	}

	// A poor man's implementation

	// Initialization
	for (BasicBlockList::iterator it = basic_blocks_.begin(),
			ie = basic_blocks_.end(); it != ie; ++it) {
		HighLevelBasicBlock *bb = *it;

		if (it == basic_blocks_.begin()) {
			bb->dominators_.insert(bb);
		} else {
			bb->dominators_.insert(basic_blocks_.begin(), basic_blocks_.end());
		}
	}

	// Iterative convergence
	bool converged = false;
	while (!converged) {
		converged = true;

		for (BasicBlockList::iterator it = basic_blocks_.begin(),
				ie = basic_blocks_.end(); it != ie; ++it) {
			if (it == basic_blocks_.begin())
				continue;

			HighLevelBasicBlock *bb = *it;

			std::set<HighLevelBasicBlock*> result;

			for (HighLevelBasicBlock::AdjacencyList::iterator
					pit = bb->predecessors_.begin(),
					pie = bb->predecessors_.end(); pit != pie; ++pit) {
				HighLevelBasicBlock *pred_bb = *pit;

				if (pit == bb->predecessors_.begin()) {
					result.insert(pred_bb->dominators_.begin(),
							pred_bb->dominators_.end());
				} else {
					std::set<HighLevelBasicBlock*> intersection;
					std::set_intersection(pred_bb->dominators_.begin(),
							pred_bb->dominators_.end(),
							result.begin(), result.end(),
							std::inserter(intersection, intersection.begin()));
					result = intersection;
				}
			}

			result.insert(bb);
			if (bb->dominators_.size() != result.size()) {
				converged = false;
				bb->dominators_ = result;
			}
		}
	}
}


void HighLevelCFG::extractBranchOpcodes() {
	branch_opcodes_.clear();

	int total_counter = 0;

	for (BasicBlockList::iterator it = basic_blocks_.begin(),
			ie = basic_blocks_.end(); it != ie; ++it) {
		HighLevelBasicBlock *bb = *it;
		if (bb->successors_.size() > 1) {
			branch_opcodes_[bb->tail()->opcode()]++;
			total_counter++;
		}
	}

	typedef std::set<std::pair<HighLevelOpcode, int>, cmp_by_second> SortedOpcodes;

	SortedOpcodes sorted_opcodes(branch_opcodes_.begin(),
			branch_opcodes_.end());

	branch_opcodes_.clear();

	int cumul_counter = 0;
	for (SortedOpcodes::reverse_iterator it = sorted_opcodes.rbegin(),
			ie = sorted_opcodes.rend(); it != ie; ++it) {
		cumul_counter += it->second;
		// XXX Hack, hack, hack
		if (cumul_counter < total_counter * 8 / 10) {
			branch_opcodes_.insert(*it);
		} else {
			break;
		}
	}

	debug_stream_ << "Extracted " << branch_opcodes_.size()
			<< " branch opcodes." << '\n';
}


bool HighLevelCFG::isBranchInstruction(const HighLevelInstruction* inst) {
	return branch_opcodes_.count(inst->opcode()) > 0;
}


void HighLevelCFG::computeDistanceToUncovered() {
	int uncovered_points = 0;

	for (InstructionMap::iterator it = instructions_.begin(),
			ie = instructions_.end(); it != ie; ++it) {
		HighLevelInstruction *inst = it->second;
		inst->dist_to_uncovered_ = (isBranchInstruction(inst) &&
				inst->successors_.size() < 2) ? 1 : 0;
		uncovered_points += inst->dist_to_uncovered_;
	}

	debug_stream_ << "Identified " << uncovered_points
			<< " potential uncovered branching points." << '\n';

	// Adapted from the minDistanceToUncovered code in KLEE
	bool done;
	do {
		done = true;
		for (InstructionMap::iterator it = instructions_.begin(),
				ie = instructions_.end(); it != ie; ++it) {
			HighLevelInstruction *inst = it->second;
			int current_best = inst->dist_to_uncovered_;
			int new_best = current_best;

			for (HighLevelInstruction::AdjacencyMap::const_iterator
					sit = inst->successors_.begin(),
					sie = inst->successors_.end(); sit != sie; ++sit) {
				HighLevelInstruction *succ = sit->second;
				if (succ->dist_to_uncovered_ && (new_best == 0 ||
						succ->dist_to_uncovered_ + 1 < new_best)) {
					new_best = succ->dist_to_uncovered_ + 1;
				}
			}

			if (new_best != current_best) {
				inst->dist_to_uncovered_ = new_best;
				done = false;
			}
		}
	} while (!done);
}


int HighLevelCFG::computeMinDistance(const HighLevelInstruction *source,
			const HighLevelInstruction *dest) const {
	typedef std::map<const HighLevelInstruction*, int> DistanceMap;
	typedef std::set<const HighLevelInstruction*> VisitedSet;

	// XXX: Not really a code gem, but should do the job :)

	DistanceMap dist_map;
	dist_map[source] = 0;

	VisitedSet visited;

	const HighLevelInstruction *current = source;

	while (current != NULL) {
		int current_dist = dist_map.find(current)->second;

		for (HighLevelInstruction::AdjacencyMap::const_iterator
				iit = current->successors_.begin(),
				iie = current->successors_.end(); iit != iie; ++iit) {
			const HighLevelInstruction *inst = iit->second;
			if (visited.count(inst))
				continue;
			DistanceMap::iterator dit = dist_map.find(inst);
			if (dit == dist_map.end()) {
				dist_map[inst] = current_dist + 1;
			} else if (current_dist + 1 < dit->second) {
				dist_map[inst] = current_dist + 1;
			}
		}

		visited.insert(current);

		if (current == dest)
			break;

		current = NULL;
		DistanceMap::iterator min_dit = dist_map.end();

		for (DistanceMap::iterator dit = dist_map.begin(), die = dist_map.end();
				dit != die; ++dit) {
			if (visited.count(dit->first))
				continue;
			if (min_dit == dist_map.end() || dit->second < min_dit->second) {
				min_dit = dit;
			}
		}
		if (min_dit != dist_map.end()) {
			current = min_dit->first;
		}
	}

	if (dist_map.count(dest))
		return dist_map[dest];
	else
		return -1;
}


////////////////////////////////////////////////////////////////////////////////


void GraphVisualizer::drawNode(const std::string &name,
		const AttributeMap &attributes) {
	os_ << "  " << name;
	if (!attributes.empty()) {
		os_ << " ";
		recordAttributes(attributes);
	}
	os_ << ";" << '\n';
}


void GraphVisualizer::drawNode(int id, const AttributeMap &attributes) {
	os_ << "  " << "node" << id;
	if (!attributes.empty()) {
		os_ << " ";
		recordAttributes(attributes);
	}
	os_ << ";" << '\n';
}


void GraphVisualizer::drawEdge(const std::string &source, const std::string &dest,
			const AttributeMap &attributes) {
	os_ << "  " << source << " -> " << dest;
	if (!attributes.empty()) {
		os_ << " ";
		recordAttributes(attributes);
	}
	os_ << ";" << '\n';
}


void GraphVisualizer::drawEdge(int source, int dest,
		const AttributeMap &attributes) {
	os_ << "  " << "node" << source << " -> " << "node" << dest;
	if (!attributes.empty()) {
		os_ << " ";
		recordAttributes(attributes);
	}
	os_ << ";" << '\n';
}


void GraphVisualizer::recordAttributes(const AttributeMap &attributes) {
	os_ << "[";
	for (AttributeMap::const_iterator it = attributes.begin(),
			ie = attributes.end(); it != ie; ++it) {
		if (it != attributes.begin()) {
			os_ << ", ";
		}
		os_ << it->first << "=" << it->second;
	}
	os_ << "]";
}


////////////////////////////////////////////////////////////////////////////////


void HighLevelTreeVisualizer::preprocessTree(HighLevelTreeNode *root) {
	std::queue<HighLevelTreeNode*> branches;
	branches.push(root);

	while (!branches.empty()) {
		HighLevelTreeNode *head = branches.front();
		branches.pop();

		while(head->successors().size() == 1) {
			HighLevelTreeNode *next = head->successors().begin()->second;
			if (head->path_counter() > max_path_count_) {
				max_path_count_ = head->path_counter();
			}
			if (head->fork_counter() > max_path_count_) {
				max_path_count_ = head->fork_counter();
			}
			head = next;
		}

		if (!head->successors().empty()) {
			for (HighLevelTreeNode::AdjacencyMap::const_iterator
					it = head->successors().begin(),
					ie = head->successors().end(); it != ie; ++it) {
				HighLevelTreeNode *new_head = it->second;
				branches.push(new_head);
			}
		}
	}
}


void HighLevelTreeVisualizer::printTreeNode(HighLevelTreeNode *node, NodeType node_type,
		int ref_path_count) {
	// XXX: Pretty much ugly
	int min_font_size = 7;
	int max_font_size = 40;

	double node_scale = log2((double)node->path_counter())
			/ log2((double)max_path_count_);
	int font_size = min_font_size + (int)((max_font_size -
			min_font_size) * node_scale);

	os() << "  { rank = same; ";

	os() << "  " << "node" << node_names_.getID(node) << " [";
	switch (node_type) {
	case NORMAL:
		os() << "style=bold, ";
		break;
	case INTERN:
		break;
	case TERMINAL:
		os() << "style=filled, fillcolor=lightgrey, ";
		break;
	}
	os() << "fontsize=" << font_size << ", ";
	os() << "shape=record, label=\"{ " << node->path_counter();

	if (node_type == INTERN) {
		os() << "\\n(+" << (node->path_counter() - ref_path_count) << ")";
	}

	os() << " | opcode = " << node->instruction()->opcode()
			<< "\\nhlpc = " << node->instruction()->hlpc() << "}\"";
	os() << "]; ";

	if (!node->fork_counter()) {
		os() << "}" << '\n';
		return;
	}

	node_scale = log2((double)node->fork_counter())
			/ log2((double)max_path_count_);
	font_size = min_font_size + (int)((max_font_size -
			min_font_size) * node_scale);

	os() << "  " << "node" << node_names_.getID(node) << " -> "
			<< "pending" << node_names_.getID(node)
			<< " [style=dotted];" << '\n';
	os() << "  " << "pending" << node_names_.getID(node)
			<< " [shape=record, style=filled, fillcolor=lightyellow, label=\"{ "
			<< node->fork_counter() << "\\nalternates | { ";

#if 0
	typedef std::map<uint64_t, ssize_t> SizeMap;
	SizeMap size_map;
	node->pending_states_.sizeBreakdown(size_map);

	for (SizeMap::iterator it = size_map.begin(), ie = size_map.end();
			it != ie; ++it) {
		if (it != size_map.begin()) {
			os() << " | ";
		}
		os() << it->second << "\\n" << hexval(it->first);
	}
#endif

	os()	<< " } }\", fontsize=" << font_size << "]; ";

	os() << "}" << '\n';
}


void HighLevelTreeVisualizer::dumpTree(HighLevelTreeNode *root) {
	std::queue<HighLevelTreeNode*> branches;

	preprocessTree(root);

	os() << "digraph {" << '\n';

	branches.push(root);

	while (!branches.empty()) {
		HighLevelTreeNode *head = branches.front();
		HighLevelTreeNode *prev = head;
		int accumulated = 0;
		int last_path_counter = head->path_counter();

		branches.pop();

		printTreeNode(head, NORMAL, last_path_counter);

		while (head->successors().size() == 1) {
			HighLevelTreeNode *next = head->successors().begin()->second;

			if (next->fork_counter() > 0 ||
					last_path_counter != next->path_counter() ||
					next->successors().size() != 1) {
				NodeType node_type = NORMAL;
				if (next->successors().size() == 1) {
					node_type = INTERN;
				} else if (next->successors().empty()) {
					node_type = TERMINAL;
				}

				if (accumulated > 1) {
					int missing_id = node_names_.getID();
					// Print a missing nodes marking
					drawEdge(node_names_.getID(prev), missing_id, AttributeMap());

					std::stringstream missing_label;
					missing_label << '"' << accumulated << " extra nodes" << '"';
					AttributeMap missing_attrs;
					missing_attrs["shape"] = "none";
					missing_attrs["label"] = missing_label.str();

					drawNode(missing_id, missing_attrs);
					drawEdge(missing_id, node_names_.getID(next), AttributeMap());
					printTreeNode(next, node_type, last_path_counter);
				} else if (accumulated == 1) {
					// Print prev, head and next
					drawEdge(node_names_.getID(prev), node_names_.getID(head),
							AttributeMap());
					printTreeNode(head, node_type, last_path_counter);
					drawEdge(node_names_.getID(head), node_names_.getID(next),
							AttributeMap());
					printTreeNode(next, node_type, last_path_counter);
				} else {
					// Print just head and next (prev == head)
					assert(prev == head);

					drawEdge(node_names_.getID(head), node_names_.getID(next),
							AttributeMap());
					printTreeNode(next, node_type, last_path_counter);
				}

				accumulated = 0;
				last_path_counter = next->path_counter();
				prev = next;
			} else {
				accumulated += 1;
			}
			head = next;
		}

		if (!head->successors().empty()) {
			for (HighLevelTreeNode::AdjacencyMap::const_iterator
					it = head->successors().begin(),
					ie = head->successors().end(); it != ie; ++it) {
				HighLevelTreeNode *new_head = it->second;
				drawEdge(node_names_.getID(head), node_names_.getID(new_head),
						AttributeMap());
				branches.push(new_head);
			}
		}
	}

	os() << "}" << '\n';
}


////////////////////////////////////////////////////////////////////////////////

void HighLevelCFGVisualizer::dumpCFG(HighLevelCFG &cfg) {
	os() << "digraph {" << '\n';

	max_hl_path_count_ = 0;

	for (HighLevelCFG::BasicBlockList::const_iterator
			it = cfg.basic_blocks().begin(),
			ie = cfg.basic_blocks().end(); it != ie; ++it) {
		HighLevelBasicBlock *bb = *it;

		max_hl_path_count_ = (bb->head()->high_level_paths() > max_hl_path_count_)
				? bb->head()->high_level_paths() : max_hl_path_count_;
	}

	for (HighLevelCFG::BasicBlockList::const_iterator
			it = cfg.basic_blocks().begin(),
			ie = cfg.basic_blocks().end(); it != ie; ++it) {
		HighLevelBasicBlock *bb = *it;
		printBasicBlock(bb);

		for (HighLevelBasicBlock::AdjacencyList::const_iterator
				it = bb->successors().begin(), ie = bb->successors().end();
				it != ie; ++it) {
			HighLevelBasicBlock *next_bb = *it;

			AttributeMap edge_attr;

			if (bb->dominators().count(next_bb) > 0) {
				// This is a back edge, draw it appropriately
				edge_attr["penwidth"] = "3";
			}

			drawEdge(bb_names_.getID(bb), bb_names_.getID(next_bb), edge_attr);
		}
	}

	os() << "}" << '\n';
}


void HighLevelCFGVisualizer::printBasicBlock(const HighLevelBasicBlock *bb) {
	std::stringstream bb_label;

	bb_label << '"' << "{ ";

	bb_label << bb->head()->high_level_paths() << " HL PATHS";
	bb_label << " | ";

	for (HighLevelInstruction *current = bb->head(), *previous = bb->head();;
			current = current->next()) {
		if (current->fork_counter_ != 0 || current->low_level_paths_ !=
				previous->low_level_paths_) {
			printInstructionSeq(previous, current, bb_label);
			bb_label << " | ";
			printInstruction(current, bb_label);
			if (current != bb->tail()) {
				previous = current->next();
				bb_label << " | ";
			} else {
				break;
			}
		} else if (current == bb->tail()) {
			printInstructionSeq(previous, current, bb_label);
			break;
		}
	}

	bb_label << " }" << '"';

	// XXX: Horrible! Look away!
	int min_font_size = 7;
	int max_font_size = 40;

	double scale = log2((double)bb->head()->high_level_paths()) /
			log2((double)max_hl_path_count_);
	std::stringstream font_size;
	font_size << (min_font_size + (int)(max_font_size - min_font_size) * scale);

	AttributeMap bb_attrs;
	bb_attrs["label"] = bb_label.str();
	bb_attrs["shape"] = "record";
	bb_attrs["fontsize"] = font_size.str();

	drawNode(bb_names_.getID(bb), bb_attrs);
}


void HighLevelCFGVisualizer::printInstructionSeq(HighLevelInstruction *head,
			HighLevelInstruction *tail, std::ostream &os) {
	int size = 1;
	for (HighLevelInstruction *current = head; current != tail;
			current = current->next()) {
		size++;
	}

	os << head->hlpc() << ' ' << head->opcode();

	if (size > 2) {
		os << "\\n";
		os << "(" << (size - 2) << " instructions)";
	}

	if (size > 1) {
		os << "\\n";
		os << tail->hlpc() << ' ' << tail->opcode();
	}
}


void HighLevelCFGVisualizer::printInstruction(HighLevelInstruction *instr,
		std::ostream &os) {
	os << "{ " << instr->low_level_paths_ << " LL paths"
			<< " | " << instr->fork_counter_ << " fork points"
			<< " }";
}

////////////////////////////////////////////////////////////////////////////////


S2E_DEFINE_PLUGIN(InterpreterMonitor,
		"Support tracing of high-level interpreted code.",
		"");

InterpreterMonitor::InterpreterMonitor(S2E *s2e)
	: Plugin(s2e),
	  cfg_(s2e->getMessagesStream()),
	  root_node_(NULL),
	  active_node_(NULL),
	  active_state_(NULL) {

}


void InterpreterMonitor::initialize() {

}


void InterpreterMonitor::startTrace(S2EExecutionState *state) {
	assert(root_node_ == NULL && "Tracing already in progress");

	on_state_fork_ = s2e()->getCorePlugin()->onStateFork.connect(
			sigc::mem_fun(*this, &InterpreterMonitor::onStateFork));
	on_state_switch_ = s2e()->getCorePlugin()->onStateSwitch.connect(
			sigc::mem_fun(*this, &InterpreterMonitor::onStateSwitch));
	on_state_kill_ = s2e()->getCorePlugin()->onStateKill.connect(
			sigc::mem_fun(*this, &InterpreterMonitor::onStateKill));

	HighLevelInstruction *root = cfg_.recordNode(HighLevelPC());
	root_node_ = new HighLevelTreeNode(root, NULL);
	active_node_ = root_node_;
	active_state_ = state;

	active_node_->bumpPathCounter();
}

void InterpreterMonitor::stopTrace(S2EExecutionState *state) {
	root_node_->clear();
	delete root_node_;

	cfg_.clear();

	root_node_ = NULL;
	active_node_ = NULL;
	active_state_ = NULL;

	on_state_fork_.disconnect();
	on_state_switch_.disconnect();
	on_state_kill_.disconnect();
}


unsigned int InterpreterMonitor::handleOpcodeInvocation(S2EExecutionState *state,
			uint64_t guestDataPtr, uint64_t guestDataSize) {
	if (root_node_ == NULL) {
		return 0;
	}

	char *message_buffer = new char[guestDataSize];
	if (message_buffer == NULL) {
		return 1;
	}

	if (!state->readMemoryConcrete(guestDataPtr, message_buffer, guestDataSize,
			S2EExecutionState::VirtualAddress)) {
		delete [] message_buffer;
		return 1;
	}

	TraceUpdate *update = (TraceUpdate*)message_buffer;

	HighLevelPC hlpc = HighLevelPC(&update->frames[0],
			&update->frames[update->frame_count]);
	HighLevelOpcode opcode = update->op_code;
	delete [] message_buffer;

	doUpdateHLPC(state, hlpc, opcode);
	return 0;
}


void InterpreterMonitor::onStateFork(S2EExecutionState *state,
		const StateVector &new_states,
		const std::vector<klee::ref<klee::Expr> > &new_conditions) {
	assert(state == active_state_);

	for (StateVector::const_iterator it = new_states.begin(),
			ie = new_states.end(); it != ie; ++it) {
		S2EExecutionState *new_state = *it;
		if (new_state == state)
			continue;

		state_mapping_[new_state] = active_node_;
		active_node_->bumpForkCounter();
	}
}


void InterpreterMonitor::onStateSwitch(S2EExecutionState *state,
		S2EExecutionState *new_state) {
	s2e()->getMessagesStream(state) << "Switching to state " << new_state->getID() << '\n';

	assert(state == active_state_);

	state_mapping_[state] = active_node_;
	active_node_ = getHLTreeNode(new_state);
	assert(active_node_ != NULL);
	active_state_ = new_state;

	// We assume here that we switch to a new state, so we don't risk
	// counting the same path twice.
	active_node_->bumpPathCounter();
}


void InterpreterMonitor::onStateKill(S2EExecutionState *state) {
	state_mapping_.erase(state);
}


void InterpreterMonitor::doUpdateHLPC(S2EExecutionState *state,
		const HighLevelPC &hlpc, HighLevelOpcode opcode) {
	assert(state == active_state_);

	HighLevelInstruction *inst = cfg_.recordEdge(
			active_node_->instruction()->hlpc(),
			hlpc, opcode);

	active_node_ = active_node_->getOrCreateSuccessor(inst);
	active_node_->bumpPathCounter();

	on_hlpc_update.emit(state, active_node_);
}


HighLevelTreeNode *InterpreterMonitor::getHLTreeNode(S2EExecutionState *state) const {
	if (root_node_ == NULL)
		return NULL;

	if (state == active_state_) {
		return active_node_;
	}

	StateNodeMapping::const_iterator it = state_mapping_.find(state);

	if (it == state_mapping_.end()) {
		return NULL;
	}

	return it->second;
}


void InterpreterMonitor::dumpHighLevelTree(std::ostream &os) {
	assert(root_node_ != NULL);

	HighLevelTreeVisualizer visualizer(os);
	visualizer.dumpTree(root_node_);
}


void InterpreterMonitor::dumpHighLevelCFG(std::ostream &os) {
	HighLevelCFGVisualizer visualizer(os);
	cfg_.analyzeCFG();
	visualizer.dumpCFG(cfg_);
}


} /* namespace plugins */
} /* namespace s2e */
