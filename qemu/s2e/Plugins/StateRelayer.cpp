extern "C" {
#include "config.h"
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <sysemu.h>
#include <cpus.h>
}
#include <iomanip>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <vector>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>

#include <s2e/Plugin.h>
#include <s2e/s2e_qemu.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2ESJLJ.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>    /**/
#include <errno.h>     /*errno*/
#include <unistd.h>    /*ssize_t*/
#include <sys/types.h>
#include <sys/stat.h>  /*mode_t*/

#include <stdlib.h>
#include <llvm/Support/TimeValue.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

extern "C" void kbd_put_keycode(int keycode);
using namespace std;

//#include <regex>  // regular expression 正则表达式 c++ 11

#include <klee/Solver.h>
#include <klee/util/ExprUtil.h>
#include "StateRelayer.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StateRelayer, "StateRelayer plugin", "StateRelayer",);

StateRelayer::StateRelayer(S2E* s2e) :
		Plugin(s2e) {
	m_key_enter_sent = false;
	m_isfirstInstructionProcessed = false;
	m_current_conditon = 0;
	m_currentTime = 0;
	m_autosendkey_interval = 10;
	m_autosendkey_enter = false;
}

void StateRelayer::initialize() {
	bool ok = false;
	m_autosendkey_enter = s2e()->getConfig()->getBool(
			getConfigKey() + ".autosendkey_enter",
			false, &ok);
	m_autosendkey_interval = s2e()->getConfig()->getInt(
			getConfigKey() + ".autosendkey_interval", 10, &ok);

	s2e()->getCorePlugin()->onInitializationComplete.connect(
	sigc::mem_fun(*this, &StateRelayer::onInitializationComplete));

	s2e()->getCorePlugin()->onStateSwitchEnd.connect(
	sigc::mem_fun(*this, &StateRelayer::onStateSwitchEnd));

	m_firstInstructionTranslateStart =
			s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
					sigc::mem_fun(*this,
							&StateRelayer::slotFirstInstructionTranslateStart));

	s2e()->getExecutor()->setSearcher(this);
	s2e()->getDebugStream() << s2e()->getSingleRole() << "\n";
}
klee::ExecutionState& StateRelayer::selectState() {
	klee::ExecutionState *state;

	if (!m_normalStates.empty()) {
		typeof(m_normalStates.begin()) it = m_normalStates.begin();
		state = *it;
		if (state->m_is_carry_on_state) {
			if (m_normalStates.size() > 1) {
				++it;
				state = *it;
			} else if (!m_speculativeStates.empty()) {
				state = *m_speculativeStates.begin();
			}
		}
	} else {
		assert(!m_speculativeStates.empty());
		typeof(m_speculativeStates.begin()) it = m_speculativeStates.begin();
		state = *it;
		if (state->m_is_carry_on_state) {
			if (m_speculativeStates.size() > 1) {
				++it;
				state = *it;
			}
		}
	}
	return *state;
}

void StateRelayer::update(klee::ExecutionState *current,
		const std::set<klee::ExecutionState*> &addedStates,
		const std::set<klee::ExecutionState*> &removedStates) {
	if (current && addedStates.empty() && removedStates.empty()) {
		S2EExecutionState *s2estate = dynamic_cast<S2EExecutionState*>(current);
		if (!s2estate->isZombie()) {
			if (current->isSpeculative()) {
				m_normalStates.erase(current);
				m_speculativeStates.insert(current);
			} else {
				m_speculativeStates.erase(current);
				m_normalStates.insert(current);
			}
		}
	}

	foreach2(it, removedStates.begin(), removedStates.end())
	{
		S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
		if (es->isSpeculative()) {
			m_speculativeStates.erase(es);
		} else {
			m_normalStates.erase(es);
		}
	}

	foreach2(it, addedStates.begin(), addedStates.end())
	{
		S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
		if (es->isSpeculative()) {
			m_speculativeStates.insert(es);
		} else {
			m_normalStates.insert(es);
		}
	}
}
void StateRelayer::afterupdate(klee::ExecutionState *current) {
	if (current && current->m_shouldbedeleted == true) {
		current->m_shouldbedeleted = false; //CHECK really need this?//防止死循环
		s2e()->getExecutor()->terminateStateEarly(*current,
				"terminate replay state.");
	}
}
bool StateRelayer::empty() {
	return m_normalStates.empty() && m_speculativeStates.empty();
}
void StateRelayer::slotFirstInstructionTranslateStart(ExecutionSignal *signal,
		S2EExecutionState *state, TranslationBlock *tb, uint64_t pc) {

	if (!m_isfirstInstructionProcessed) {
		m_firstInstructionProcess = signal->connect(
		sigc::mem_fun(*this, &StateRelayer::ProcessFirstInstruction));
	}
}
void StateRelayer::ProcessFirstInstruction(S2EExecutionState* state,
		uint64_t pc) {
	if (!m_isfirstInstructionProcessed) {
		prepareNextState(state, true);
		m_isfirstInstructionProcessed = true;
		m_firstInstructionProcess.disconnect();
		m_firstInstructionTranslateStart.disconnect();
	}
}
void StateRelayer::onStateSwitchEnd(S2EExecutionState *currentState,
		S2EExecutionState *nextState) {
	// 后备状态都被选中了，说明只有一个状态了，可以准备下一个被选状态了。
	if (nextState && nextState->m_is_carry_on_state) {
		prepareNextState(nextState);
	}
}
klee::Executor::StatePair StateRelayer::prepareNextState(
		S2EExecutionState *state, bool isinitial) {
	klee::Executor::StatePair sp;
	state->jumpToSymbolicCpp();
	bool oldForkStatus = state->isForkingEnabled();
	state->enableForking();
	if (isinitial) {
		m_dummy_symb = state->createSymbolicValue("dummy_symb_var",
				klee::Expr::Int32);
		m_current_conditon = 0;
	}
	printf("prepareNextState\n");
	state->m_preparingstate = true;
	std::vector<klee::Expr> conditions;
	klee::ref<klee::Expr> cond = klee::NeExpr::create(m_dummy_symb,
			klee::ConstantExpr::create(m_current_conditon, klee::Expr::Int32));
	sp = s2e()->getExecutor()->fork(*state, cond, false);
	S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
	S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

	klee::ref<klee::Expr> condnot = klee::EqExpr::create(m_dummy_symb,
			klee::ConstantExpr::create(m_current_conditon, klee::Expr::Int32));
	fs->addConstraint(condnot);

	ts->setForking(oldForkStatus);
	fs->setForking(oldForkStatus);

	ts->m_is_carry_on_state = true;
	fs->m_is_carry_on_state = false;

	ts->m_preparingstate = false;
	fs->m_preparingstate = false;
	m_current_conditon++;
	//如果是并行化运行，从任务池中领取任务，更新当前状态即可//通过这里实现状态恢复
	m_key_enter_sent = false;
	CorePlugin *plg = s2e()->getCorePlugin();
	m_timerconn = plg->onTimer.connect(
	sigc::mem_fun(*this, &StateRelayer::onTimer));
	llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
	m_currentTime = curTime.seconds();
	S2EExecutionState::resetLastSymbolicId();
	s2e()->getExecutor()->updateStates(state);
	return sp;
}
void StateRelayer::onInitializationComplete(S2EExecutionState* state) {

}
void StateRelayer::onTimer() {
	llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
	if (m_currentTime < (curTime.seconds() - m_autosendkey_interval)) {
		m_currentTime = curTime.seconds();
		//让程序自动运行起来,应该是延时一会儿再发，否则收不到。
		if (m_autosendkey_enter && !m_key_enter_sent) {
			int keycode = 0x9c;
			if (keycode & 0x80)
				kbd_put_keycode(0xe0);
			kbd_put_keycode(keycode & 0x7f);
			m_key_enter_sent = true;
			m_timerconn.disconnect();
		}
	}

}
}
}

