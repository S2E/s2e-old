/*
 * ForkController.cpp
 *
 *  Created on: 2015年1月4日
 *      Author: wb
 */

#include "ForkController.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
namespace s2e {
namespace plugins {
S2E_DEFINE_PLUGIN(ForkController, "ForkController plugin", "ForkController",
		"ModuleExecutionDetector");

ForkController::~ForkController() {
}
void ForkController::initialize() {
	ConfigFile *cfg = s2e()->getConfig();
	ConfigFile::string_list pollingEntries = cfg->getListKeys(
			getConfigKey() + ".forkRanges");
	if (pollingEntries.size() == 0) {
		Range rg;
		rg.start = 0x00000000;
		rg.end = 0xC0000000; // we do not check the kernel
		m_forkRanges.insert(rg);
	} else {
		foreach2(it, pollingEntries.begin(), pollingEntries.end())
		{
			std::stringstream ss1;
			ss1 << getConfigKey() << ".forkRanges" << "." << *it;
			ConfigFile::integer_list il = cfg->getIntegerList(ss1.str());
			if (il.size() != 2) {
				s2e()->getWarningsStream() << "Range entry " << ss1.str()
						<< " must be of the form {startPc, endPc} format"
						<< '\n';
				continue;
			}

			bool ok = false;
			uint64_t start = cfg->getInt(ss1.str() + "[1]", 0, &ok);
			if (!ok) {
				s2e()->getWarningsStream() << "could not read " << ss1.str()
						<< "[0]" << '\n';
				continue;
			}

			uint64_t end = cfg->getInt(ss1.str() + "[2]", 0, &ok);
			if (!ok) {
				s2e()->getWarningsStream() << "could not read " << ss1.str()
						<< "[1]" << '\n';
				continue;
			}
			//Convert the format to native address
			Range rg;
			rg.start = start;
			rg.end = end;
			m_forkRanges.insert(rg);
		}
	}
	m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin(
			"ModuleExecutionDetector"));
	if (m_detector) {
		s2e()->getCorePlugin()->onTranslateBlockStart.connect(
				sigc::mem_fun(*this, &ForkController::onTranslateBlockStart));
		s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
				sigc::mem_fun(*this, &ForkController::onTranslateBlockEnd));
	}
}

void ForkController::onTranslateBlockStart(ExecutionSignal* es,
		S2EExecutionState* state, TranslationBlock* tb, uint64_t pc) {
	if (!tb) {
		return;
	}
	es->connect(sigc::mem_fun(*this, &ForkController::slotExecuteBlockStart));
}
void ForkController::onTranslateBlockEnd(ExecutionSignal *signal,
		S2EExecutionState* state,
		TranslationBlock *tb, uint64_t endPc,
		bool staticTarget, uint64_t targetPc) {
	if (!tb) {
		return;
	}
	signal->connect(sigc::mem_fun(*this, &ForkController::slotExecuteBlockEnd));
}
/**
 */
void ForkController::slotExecuteBlockStart(S2EExecutionState *state,
		uint64_t pc) {
	if (state->isSymbolicExecutionEnabled()) {
		bool found = false;
		if (!found) {
			foreach2(rgit, m_forkRanges.begin(), m_forkRanges.end())
			{
				if ((pc >= (*rgit).start) && (pc <= (*rgit).end)) {
					found = true;
					break;
				}
			}
		}
		if (found) {
			state->enableForking();
		}
	}
}

/**
 *白名单机制,只有分析结果范围内允许状态分化,其他情况一律禁止状态分化
 */
void ForkController::slotExecuteBlockEnd(S2EExecutionState *state,
		uint64_t pc) {
	if (state->isForkingEnabled())
		state->disableForking();
}

}
} /* namespace s2e */
