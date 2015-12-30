/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
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
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
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

ForkController::~ForkController()
{
}
void ForkController::initialize()
{
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
		S2EExecutionState* state, TranslationBlock* tb, uint64_t pc)
{
	if (!tb) {
		return;
	}
	es->connect(sigc::mem_fun(*this, &ForkController::slotExecuteBlockStart));
}
void ForkController::onTranslateBlockEnd(ExecutionSignal *signal,
		S2EExecutionState* state, TranslationBlock *tb, uint64_t endPc,
		bool staticTarget, uint64_t targetPc)
{
	if (!tb) {
		return;
	}
	signal->connect(sigc::mem_fun(*this, &ForkController::slotExecuteBlockEnd));
}
/**
 */
void ForkController::slotExecuteBlockStart(S2EExecutionState *state,
		uint64_t pc)
{
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

void ForkController::slotExecuteBlockEnd(S2EExecutionState *state, uint64_t pc)
{
	if (state->isForkingEnabled())
		state->disableForking();
}

}
} /* namespace s2e */
