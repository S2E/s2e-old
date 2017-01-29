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
#include "AutoFileInput.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(AutoFileInput, "AutoFileInput plugin", "AutoFileInput",
		);

AutoFileInput::AutoFileInput(S2E* s2e) :
		Plugin(s2e) {
	m_file_updated = false;
	m_key_enter_sent = false;
	m_isfirstInstructionProcessed = false;
	m_crash_count = 0;
	m_candidate_count = 0;
	m_current_conditon = 0;
	m_currentTime =0;
	m_autosendkey_interval = 10;
	m_autosendkey_enter = false;
	m_gen_crash_onvuln = false;
}

void AutoFileInput::initialize() {
	bool ok = false;
	m_command_file = s2e()->getConfig()->getString(
			getConfigKey() + ".command_file", "", &ok);
	if (!ok) {
		s2e()->getWarningsStream() << "You must specifiy "
				<< getConfigKey() + ".command_file" << '\n';
		exit(-1);
	}
	m_command_str = s2e()->getConfig()->getString(
			getConfigKey() + ".command_str", "", &ok);
	if (!ok) {
		s2e()->getWarningsStream() << "You must specifiy "
				<< getConfigKey() + ".command_str" << '\n';
		exit(-1);
	}
	m_case_file = s2e()->getConfig()->getString(
			getConfigKey() + ".case_file", "", &ok);
	if (!ok) {
		s2e()->getWarningsStream() << "You must specifiy "
				<< getConfigKey() + ".case_file" << '\n';
		exit(-1);
	}
	m_autosendkey_enter = s2e()->getConfig()->getBool(
			getConfigKey() + ".autosendkey_enter",
			false, &ok);
	m_autosendkey_interval = s2e()->getConfig()->getInt(
			getConfigKey() + ".autosendkey_interval",
			10, &ok);
	m_gen_crash_onvuln = s2e()->getConfig()->getBool(
			getConfigKey() + ".gen_crash_onvuln",
			false, &ok);
	std::ofstream cfile(m_command_file.c_str());
	cfile << m_command_str;
	cfile.close();
	m_snap_short_name = s2e()->getSnapShortName();

	std::string mkdirError;
	std::string crash_dir =  s2e()->getOutputDirectory() + "/crash";
	llvm::sys::Path crashDir(crash_dir);
#ifdef _WIN32
	if (crashDir.createDirectoryOnDisk(false, &mkdirError)) {
#else
	if (crashDir.createDirectoryOnDisk(true, &mkdirError)) {
#endif
		std::cerr << "Could not create directory " << crashDir.str()
				<< " error: " << mkdirError << '\n';
	}
	std::string candidate_dir =  s2e()->getOutputDirectory() + "/candidate";
	llvm::sys::Path candidateDir(candidate_dir);
#ifdef _WIN32
	if (candidateDir.createDirectoryOnDisk(false, &mkdirError)) {
#else
	if (candidateDir.createDirectoryOnDisk(true, &mkdirError)) {
#endif
		std::cerr << "Could not create directory " << candidateDir.str()
				<< " error: " << mkdirError << '\n';
	}

	s2e()->getCorePlugin()->onVulnFound.connect(
	sigc::mem_fun(*this, &AutoFileInput::onVulnFound));

	s2e()->getCorePlugin()->onInitializationComplete.connect(
	sigc::mem_fun(*this, &AutoFileInput::onInitializationComplete));

	s2e()->getCorePlugin()->onStateSwitchEnd.connect(
	sigc::mem_fun(*this, &AutoFileInput::onStateSwitchEnd));

	m_firstInstructionTranslateStart =
			s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
					sigc::mem_fun(*this,
							&AutoFileInput::slotFirstInstructionTranslateStart));
	s2e()->getCorePlugin()->onTestCaseGeneration.connect(
			sigc::mem_fun(*this, &AutoFileInput::onTestCaseGeneration));

	s2e()->getExecutor()->setSearcher(this);

}
klee::ExecutionState& AutoFileInput::selectState() {
	klee::ExecutionState *state;

	if (!m_normalStates.empty()) {
		typeof(m_normalStates.begin()) it = m_normalStates.begin();
		state = *it;
		if (state->m_is_carry_on_state ) {
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

void AutoFileInput::update(klee::ExecutionState *current,
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
void AutoFileInput::afterupdate(klee::ExecutionState *current){
	if (current && current->m_shouldbedeleted == true) {
		current->m_shouldbedeleted = false; //CHECK really need this?//防止死循环
		s2e()->getExecutor()->terminateStateEarly(*current,
				"terminate replay state.");
	}
}
bool AutoFileInput::empty() {
	return m_normalStates.empty() && m_speculativeStates.empty();
}
void AutoFileInput::slotFirstInstructionTranslateStart(
		ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb,
		uint64_t pc) {

	if (!m_isfirstInstructionProcessed) {
		m_firstInstructionProcess = signal->connect(
				sigc::mem_fun(*this,
						&AutoFileInput::ProcessFirstInstruction));
	}
}
void AutoFileInput::ProcessFirstInstruction(S2EExecutionState* state,
		uint64_t pc) {
	if (!m_isfirstInstructionProcessed) {
		prepareNextState(state, true);
		m_isfirstInstructionProcessed = true;
		m_firstInstructionProcess.disconnect();
		m_firstInstructionTranslateStart.disconnect();
	}
}
void AutoFileInput::onStateSwitchEnd(S2EExecutionState *currentState,
		S2EExecutionState *nextState) {
	// 后备状态都被选中了，说明只有一个状态了，可以准备下一个被选状态了。
	if (nextState && nextState->m_is_carry_on_state) {
		if(m_file_updated){
				prepareNextState(nextState);
				m_file_updated = false;
		}
		else
		{
//			   s2e()->getExecutor()->terminateStateEarly(*nextState,"do not need test any more.");
			s2e()->getWarningsStream() << "Do not need test any more..."<<"\n";
			exit(0);
		}
	}
}
klee::Executor::StatePair AutoFileInput::prepareNextState(
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
	sigc::mem_fun(*this, &AutoFileInput::onTimer));
	llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
	m_currentTime = curTime.seconds();
	S2EExecutionState::resetLastSymbolicId();
	s2e()->getExecutor()->updateStates(state);
	return sp;
}
void AutoFileInput::onInitializationComplete(S2EExecutionState* state) {

}
void AutoFileInput::onTimer() {
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
void AutoFileInput::onVulnFound(S2EExecutionState *state,
		std::string vulntype, std::string message) {
	size_t pos = message.find(std::string("可将数据长度增加"));
	//TODO 增加调度，可能有很多个漏洞，每一个都可能是新的参数，因此需要将没有处理的运行脚本进入备份队列，然后依次处理。
	if (pos != std::string::npos) {
		std::string submessage = message.substr(pos);
		size_t posend = submessage.find(std::string("字节"));
		std: ;
		string lengthstr = submessage.substr(24, posend - 24);
		if(m_file_updated){
			m_file_updated = true;
			long int length = strtol(lengthstr.c_str(), NULL, 0);
			const char *filename = basename(m_case_file.c_str());
			if (!filename) {
				fprintf(stderr, "Could not allocate memory for file basename\n");
				return;
			}
			//copy to ;and append
			std::stringstream ssvulncase;
			ssvulncase << "/candidate/" << getNodeID() << "-" << state->getID()	<< "-" << m_candidate_count++ << "-" << filename;
			std::string destfilename =s2e()->getOutputFilename(ssvulncase.str().c_str()) ;
			if (!copyfile(m_case_file.c_str(), destfilename.c_str()))
				return;
			std::ofstream cfile(destfilename.c_str(), ios_base::app);
			for (int i = 0; i < 2 * length; i++) {
				cfile << 'A';
			}
			cfile.close();
		}else{
			m_file_updated = true;
			long int length = strtol(lengthstr.c_str(), NULL, 0);
			std::ofstream cfile(m_case_file.c_str(), ios_base::app);
			for (int i = 0; i < 2 * length; i++) {
				cfile << 'A';
			}
			cfile.close();
		}
//		printf("%ld\n", length);
	}
	size_t posstackbug = message.find(std::string("StackCheckerEx: BUG:"));
	size_t posheapbug = message.find(std::string("HeapCheckerImp: BUG:"));
	if ((posstackbug != std::string::npos) || (posheapbug != std::string::npos) ) {
		if(m_gen_crash_onvuln)
			generateCrashFile(state);
		// s2e()->getExecutor()->terminateStateEarly(*state, "BUG FOUND. PLEASE CHECK.");
	}
}

void AutoFileInput::onTestCaseGeneration(S2EExecutionState *state,
		const std::string &message) {
	if((strstr(message.c_str(), "terminate replay state"))) { return; }
	if((strstr(message.c_str(), "terminate writen-out state"))) { return; }
	s2e()->getMessagesStream() << "AutoFileInput: processTestCase of state "
			<< state->getID() << " at address " << hexval(state->getPc())
			<< '\n';

	 if((strstr(message.c_str(), "generate_crash_file"))) {
		 generateCrashFile(state);
	 }else{
		 ConcreteInputs out;
		 	bool success = s2e()->getExecutor()->getSymbolicSolution(*state, out);

		 	if (!success) {
		 		s2e()->getWarningsStream() << "Could not get symbolic solutions"
		 				<< '\n';
		 		return;
		 	}

		 	s2e()->getMessagesStream() << '\n';

		 	std::stringstream ss;
		 	ConcreteInputs::iterator it;
		 	for (it = out.begin(); it != out.end(); ++it) {
		 		const VarValuePair &vp = *it;
		 		ss << std::setw(20) << vp.first << ": ";

		 		for (unsigned i = 0; i < vp.second.size(); ++i) {
		 			if (i != 0)
		 				ss << ' ';
		 			ss << std::setw(2) << std::setfill('0') << std::hex
		 					<< (unsigned) vp.second[i] << std::dec;
		 		}
		 		ss << std::setfill(' ') << ", ";

		 		if (vp.second.size() == sizeof(int32_t)) {
		 			int32_t valueAsInt = vp.second[0] | ((int32_t) vp.second[1] << 8)
		 					| ((int32_t) vp.second[2] << 16)
		 					| ((int32_t) vp.second[3] << 24);
		 			ss << "(int32_t) " << valueAsInt << ", ";
		 		}
		 		if (vp.second.size() == sizeof(int64_t)) {
		 			int64_t valueAsInt = vp.second[0] | ((int64_t) vp.second[1] << 8)
		 					| ((int64_t) vp.second[2] << 16)
		 					| ((int64_t) vp.second[3] << 24)
		 					| ((int64_t) vp.second[4] << 32)
		 					| ((int64_t) vp.second[5] << 40)
		 					| ((int64_t) vp.second[6] << 48)
		 					| ((int64_t) vp.second[7] << 56);
		 			ss << "(int64_t) " << valueAsInt << ", ";
		 		}

		 		ss << "(string) \"";
		 		for (unsigned i = 0; i < vp.second.size(); ++i) {
		 			ss << (char) (std::isprint(vp.second[i]) ? vp.second[i] : '.');
		 		}
		 		ss << "\"\n";
		 	}

		 	s2e()->getMessagesStream() << ss.str() << "\n";
	 }
}
void AutoFileInput::generateCrashFile(S2EExecutionState *state) {
	    	std::stringstream ssvuln;
			ssvuln <<  "/crash/command-" << getNodeID() << "-" << state->getID()	<< ".bat";
			copyfile(m_command_file.c_str(), s2e()->getOutputFilename(ssvuln.str().c_str()).c_str());

			// generate testcase file.
			const char *filename = basename(m_case_file.c_str());
			if (!filename) {
				fprintf(stderr, "Could not allocate memory for file basename\n");
				return;
			}
			std::stringstream ssvulncase;
			ssvulncase << "/crash/" << getNodeID() << "-" << state->getID()	<< "-" << m_crash_count << "-" << filename;
			std::string destfilename =s2e()->getOutputFilename(ssvulncase.str().c_str()) ;
			if (!copyfile(m_case_file.c_str(), destfilename.c_str()))
				return;

			int fd = open(destfilename.c_str(), O_RDWR);
			if (fd < 0) {
				s2e()->getDebugStream() << "could not open dest file: "
						<< destfilename.c_str() << "\n";
				close(fd);
				return;
			}
			/* Determine the size of the file */
			off_t size = lseek(fd, 0, SEEK_END);
			if (size < 0) {
				s2e()->getDebugStream() << "could not determine the size of :"
						<< destfilename.c_str() << "\n";
				close(fd);
				return;
			}

			off_t offset = 0;
			char *delim = "_";
			char *p;
			char maxvarname[1024] = { 0 };
			ConcreteInputs out;
			bool success = s2e()->getExecutor()->getSymbolicSolution(*state, out);

			if (!success) {
				s2e()->getWarningsStream() << "Could not get symbolic solutions"
						<< '\n';
				return;
			}
			ConcreteInputs::iterator it;
			for (it = out.begin(); it != out.end(); ++it) {
				const VarValuePair &vp = *it;
				std::string varname = vp.first;
				// "__symfile___%s___%d_%d_symfile__value_%02x",filename, current_chunk, total_chunks,buffer[buffer_i]);
				//parse offset
				strcpy(maxvarname, varname.c_str());
				if((strstr(maxvarname,"symfile__value"))){
				strtok(maxvarname, delim);
				strtok(NULL, delim);
				strtok(NULL, delim);
				//strtok(NULL, delim);
				p = strtok(NULL, delim);
				offset = atol(p);
				if (lseek(fd, offset, SEEK_SET) < 0) {
					s2e()->getDebugStream() << "could not seek to position : " << offset
							<< "\n";
					close(fd);
					return;
				}
				}else if((strstr(maxvarname,"___symfile___"))){
					//v1___symfile___E:\case\huplayerpoc.m3u___27_2_symfile___0: 1a 00, (string) ".."
					//__symfile___%s___%d_%d_symfile__
					strtok(maxvarname, delim);
					strtok(NULL, delim);
					strtok(NULL, delim);
					//strtok(NULL, delim);
					p = strtok(NULL, delim);
					offset = atol(p);
					if (lseek(fd, offset, SEEK_SET) < 0) {
						s2e()->getDebugStream() << "could not seek to position : " << offset
								<< "\n";
						close(fd);
						return;
					}
				}else{
					continue;
				}
				unsigned wbuffer[1] = {0};
				for (unsigned i = 0; i < vp.second.size(); ++i) {
					wbuffer[0] = (unsigned) vp.second[i];
					ssize_t written_count = write(fd, wbuffer, 1);
					if (written_count < 0) {
						s2e()->getDebugStream() << " could not write to file : "
								<< destfilename.c_str() << "\n";
						close(fd);
						return;
					}
				}
			}
			close(fd);
			m_crash_count ++;
}
#define BUFFER_SIZE 4
bool AutoFileInput::copyfile(const char* fromfile, const char* tofile) {
	int from_fd, to_fd;
	int bytes_read, bytes_write;
	char buffer[BUFFER_SIZE];
	char *ptr;

	/* 打开源文件 */
	if ((from_fd = open(fromfile, O_RDONLY)) == -1) /*open file readonly,返回-1表示出错，否则返回文件描述符*/
	{
		fprintf(stderr, "Open %s Error:%s\n", fromfile, strerror(errno));
		return false;
	}
	/* 创建目的文件 */
	/* 使用了O_CREAT选项-创建文件,open()函数需要第3个参数,
	 mode=S_IRUSR|S_IWUSR表示S_IRUSR 用户可以读 S_IWUSR 用户可以写*/
	if ((to_fd = open(tofile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
		fprintf(stderr, "Open %s Error:%s\n", tofile, strerror(errno));
		return false;
	}
	/* 以下代码是一个经典的拷贝文件的代码 */
	while ((bytes_read = read(from_fd, buffer, BUFFER_SIZE))) {
		/* 一个致命的错误发生了 */
		if ((bytes_read == -1) && (errno != EINTR))
			break;
		else if (bytes_read > 0) {
			ptr = buffer;
			while ((bytes_write = write(to_fd, ptr, bytes_read))) {
				/* 一个致命错误发生了 */
				if ((bytes_write == -1) && (errno != EINTR))
					break;
				/* 写完了所有读的字节 */
				else if (bytes_write == bytes_read)
					break;
				/* 只写了一部分,继续写 */
				else if (bytes_write > 0) {
					ptr += bytes_write;
					bytes_read -= bytes_write;
				}
			}
			/* 写的时候发生的致命错误 */
			if (bytes_write == -1)
				break;
		}
	}
	close(from_fd);
	close(to_fd);
	return true;
}
}
}

