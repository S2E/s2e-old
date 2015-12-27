/*
 * FuzzySearcher.cpp
 *
 *  Created on: 2015年12月23日
 *      Author: Epeius
 */

#include "FuzzySearcher.h"
extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <sysemu.h>
#include <sys/shm.h>
}
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iomanip>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <vector>
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
#include "FileUtil.h"

#define DEBUG

extern "C" void kbd_put_keycode(int keycode);
//extern "C" int get_keycode(const char*);
#define MAXPATH  512
char g_inicasepool[MAXPATH] = "/tmp/scorer/inipool/";
char g_priorcasepool[MAXPATH] = "/tmp/scorer/priorpool/";
char g_cmddir[MAXPATH] = "/tmp/scorer/cmd/";
char g_databasefile[MAXPATH] = "/tmp/scorer/inputscore.db";
char g_guestcurrentcasefile[MAXPATH] = "scorecase";
namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FuzzySearcher, "FuzzySearcher plugin", "FuzzySearcher",
		"ModuleExecutionDetector", "HostFiles");

FuzzySearcher::~FuzzySearcher() {
}
void FuzzySearcher::initialize() {
	bool ok = false;
	std::string cfgkey = getConfigKey();
	//afl related
	m_aflOutputpool = s2e()->getConfig()->getString(cfgkey + ".aflOutput", "", &ok); // AFL的输出目录
	if(!ok){
		s2e()->getDebugStream() << "FuzzySearcher: You should specify AFL's output directory as aflOutputpool\n";
		exit(0);
	}
	m_aflRoot = s2e()->getConfig()->getString(cfgkey + ".aflRoot", "", &ok); // AFL的根目录
	if(!ok){
		s2e()->getDebugStream() << "FuzzySearcher: You should specify AFL's root directory as aflRoot\n";
		exit(0);
	}
	m_aflAppArgs = s2e()->getConfig()->getString(cfgkey + ".aflAppArgs", "", &ok); // 目标软件的启动命令，并将文件位置替换为@@
	if(!ok){
		s2e()->getDebugStream() << "FuzzySearcher: You should specify target binary's full arguments as aflAppArgs\n";
		exit(0);
	}
	m_aflBinaryMode = s2e()->getConfig()->getBool(getConfigKey() + ".aflBinaryMode", false, &ok);//AFL是否为binary mode
	m_MAXLOOPs = s2e()->getConfig()->getInt(getConfigKey() + ".MaxLoops", 10, &ok);//默认迭代10次

	m_symbolicfilename = s2e()->getConfig()->getString(cfgkey + ".symbolicfilename",
				"testcase", &ok); // S2E中配置的符号文件名
	m_inicasepool = s2e()->getConfig()->getString(cfgkey + ".inicasepool",
			g_inicasepool, &ok); // 该目录存放输入文件
	m_curcasepool = s2e()->getConfig()->getString(cfgkey + ".curcasepool",
			g_inicasepool, &ok); // 该目录存放正在处理的文件
	//判断路径是否存在
	if(access(m_inicasepool.c_str(), F_OK)){
		std::cerr << "Could not find directory " << m_inicasepool << '\n';
		exit(0);
	}
	if(access(m_curcasepool.c_str(), F_OK)){
		std::cerr << "Could not find directory " << m_curcasepool << '\n';
		exit(0);
	}

	std::string mkdirError;
	std::string generated_dir =  s2e()->getOutputDirectory() + "/testcases";
	llvm::sys::Path generateDir(generated_dir);
#ifdef _WIN32
	if (generateDir.createDirectoryOnDisk(false, &mkdirError)) {
#else
	if (generateDir.createDirectoryOnDisk(true, &mkdirError)) {
#endif
		std::cerr << "Could not create directory " << generateDir.str()
				<< " error: " << mkdirError << '\n';
	}

	m_mainModule = s2e()->getConfig()->getString(cfgkey + ".mainModule",
			"MainModule", &ok); // S2E中配置的符号文件名

	m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin(
						"ModuleExecutionDetector"));
	if(!m_detector){
		std::cerr << "Could not find ModuleExecutionDetector plug-in. " << '\n';
		exit(0);
	}
	m_hostFiles = static_cast<HostFiles*>(s2e()->getPlugin("HostFiles"));
	m_AutoShFileGenerator = static_cast<AutoShFileGenerator*>(s2e()->getPlugin("AutoShFileGenerator"));
	/**
	 * 状态搜索相关
	 */
	m_autosendkey_enter = s2e()->getConfig()->getBool(
			getConfigKey() + ".autosendkey_enter",
			false, &ok);
	m_autosendkey_interval = s2e()->getConfig()->getInt(
			getConfigKey() + ".autosendkey_interval", 10, &ok);
	m_firstInstructionTranslateStart =
			s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
					sigc::mem_fun(*this,
							&FuzzySearcher::slotFirstInstructionTranslateStart));
	s2e()->getCorePlugin()->onStateSwitchEnd.connect(
						sigc::mem_fun(*this, &FuzzySearcher::onStateSwitchEnd));
	s2e()->getCorePlugin()->onStateKill.connect(
							sigc::mem_fun(*this, &FuzzySearcher::onStateKill));
	m_detector->onModuleLoad.connect(
								sigc::mem_fun(*this, &FuzzySearcher::onModuleLoad));
	m_detector->onModuleTranslateBlockStart.connect(
					sigc::mem_fun(*this, &FuzzySearcher::onModuleTranslateBlockStart));
	m_detector->onModuleTranslateBlockEnd.connect(
					sigc::mem_fun(*this, &FuzzySearcher::onModuleTranslateBlockEnd));
	s2e()->getExecutor()->setSearcher(this);
}


//return *states[theRNG.getInt32()%states.size()];

klee::ExecutionState& FuzzySearcher::selectState() {
	klee::ExecutionState *state;
	if(!m_speculativeStates.empty()){ //为了更大化随机，优先选取未决状态
		States::iterator it = m_speculativeStates.begin();
		int random_index = rand() %  m_speculativeStates.size(); //尝试一次随机化
		while(random_index){
			it++;
			random_index--;
		}
		state = *it;
		if (state->m_is_carry_on_state) { //随机查找失败
			if (m_speculativeStates.size() > 1) {
				++it;
				if(it == m_speculativeStates.end()){
					it--;
					it--;
				}
				state = *it;
			}else if (!m_normalStates.empty()) {
				state = *m_normalStates.begin();
			}
		}
	}else {
		assert(!m_normalStates.empty());
		States::iterator it = m_normalStates.begin();
		int random_index = rand() %  m_normalStates.size(); //尝试一次随机化
		while(random_index){
			it++;
			random_index--;
		}
		state = *it;
		if (state->m_is_carry_on_state) {
			if (m_normalStates.size() > 1) {
				++it;
				if(it == m_normalStates.end()){
					it--;
					it--;
				}
				state = *it;
			}
		}
	}


	/*
	if (!m_normalStates.empty()) {
		States::iterator it = m_normalStates.begin();
		int random_index = rand() %  m_normalStates.size(); //尝试一次随机化
		while(random_index){
			it++;
			random_index--;
		}
		state = *it;
		if (state->m_is_carry_on_state) {
			if (m_normalStates.size() > 1) {
				++it;
				if(it == m_normalStates.end()){
					it--;
					it--;
				}
				state = *it;
			} else if (!m_speculativeStates.empty()) {
				state = *m_speculativeStates.begin();
			}
		}
	} else {
		assert(!m_speculativeStates.empty());
		States::iterator it = m_speculativeStates.begin();
		int random_index = rand() %  m_speculativeStates.size(); //尝试一次随机化
		while(random_index){
			it++;
			random_index--;
		}
		state = *it;
		if (state->m_is_carry_on_state) {
			if (m_speculativeStates.size() > 1) {
				++it;
				if(it == m_speculativeStates.end()){
					it--;
					it--;
				}
				state = *it;
			}
		}
	}
	*/
	if(state->m_silently_concretized){ //如果在silently concretized状态中，则重新选择一个状态，并结束该状态
		s2e()->getDebugStream() << "FuzzySearcher: we are in a silently concretized state\n";
		if(m_normalStates.find(state) != m_normalStates.end()){
			m_normalStates.erase(state);
			if(m_normalStates.empty())
				s2e()->getDebugStream() << "FuzzySearcher: m_normalStates's size is " << "0" << "\n";
			else
				s2e()->getDebugStream() << "FuzzySearcher: m_normalStates's size is" << m_normalStates.size() << "\n";
		}else{
			m_speculativeStates.erase(state);
			if(m_speculativeStates.empty())
				s2e()->getDebugStream() << "FuzzySearcher: m_speculativeStates's size is " << "0" << "\n";
			else
				s2e()->getDebugStream() << "FuzzySearcher: m_speculativeStates's size is" << m_speculativeStates.size() << "\n";
		}
		return selectState();
	}
	return *state;
}

void FuzzySearcher::update(klee::ExecutionState *current,
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
		if (*it == NULL)
			continue;
		S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
		if (es->isSpeculative()) {
			m_speculativeStates.erase(es);
			s2e()->getDebugStream() << "m_speculativeStates.erase --- 2\n";
		} else {
			m_normalStates.erase(es);
			s2e()->getDebugStream() << "m_normalStates.erase --- 2\n";
		}
	}

	foreach2(it, addedStates.begin(), addedStates.end())
	{
		if (*it == NULL)
			continue;
		S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
		if (es->isSpeculative()) {
			m_speculativeStates.insert(es);
			s2e()->getDebugStream() << "FuzzySearcher: Insert speculative State, m_speculativeStates' size is "
									<< m_speculativeStates.size() << ", state is" << (es->m_is_carry_on_state ? "" : " not") << " carry on state\n";
		} else {
			m_normalStates.insert(es);
			s2e()->getDebugStream() << "FuzzySearcher: Insert normal State, m_normalStates' size is "
									<< m_normalStates.size() << ", state is" << (es->m_is_carry_on_state ? "" : " not") << " carry on state\n";
		}
	}

}
bool FuzzySearcher::empty() {
	return m_normalStates.empty() && m_speculativeStates.empty();
}

void FuzzySearcher::slotFirstInstructionTranslateStart(ExecutionSignal *signal,
		S2EExecutionState *state, TranslationBlock *tb, uint64_t pc) {
	if (!m_isfirstInstructionProcessed) {
		m_firstInstructionProcess = signal->connect(
		sigc::mem_fun(*this, &FuzzySearcher::ProcessFirstInstruction)); // we start from the very beginning of this state
	}
}
void FuzzySearcher::ProcessFirstInstruction(S2EExecutionState* state,
		uint64_t pc) {
	if (!m_isfirstInstructionProcessed) {
		prepareNextState(state, true);
		m_isfirstInstructionProcessed = true;
		m_firstInstructionProcess.disconnect(); // we only do this once time
		m_firstInstructionTranslateStart.disconnect();// we only do this once time
	}
}
void FuzzySearcher::onStateSwitchEnd(S2EExecutionState *currentState,
		S2EExecutionState *nextState) {
	s2e()->getDebugStream() << "FuzzySearcher: Capture state switch end event, give a chance to handle it.\n";
	// 状态切换完成，结束之前状态，并生成测试用例
	if(currentState && !(currentState->m_is_carry_on_state)){
		s2e()->getExecutor()->terminateStateEarly(*currentState, "FuzzySearcher: terminate this for fuzzing");
	}
	if(currentState && (currentState->m_silently_concretized)){
		//s2e()->getExecutor()->terminateStateEarly(*currentState, "FuzzySearcher: terminate silently concretized state");
	}
	// 后备状态都被选中了，说明只有一个状态了，可以准备下一个被选状态了。
	if (nextState && nextState->m_is_carry_on_state) {
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: We only have the seed state, now fetching new testcase.\n";
#endif
		//在新的一轮开始之前，这里由于AFL已经启动了，所以S2E首先先将AFL/output/queue中的目录拷贝至S2E获取输入的位置，即m_inicasepool
		assert(m_AFLStarted && "AFL has not started, why?");
		{
			std::stringstream ssAflQueue;
			ssAflQueue << m_aflOutputpool << "queue/";
			//清空m_inicasepool和m_curcasepool
			FileUtil::cleardir(m_inicasepool.c_str());
			FileUtil::cleardir(m_curcasepool.c_str());
			std::vector<std::string> taskfiles;
			int taskcount = FileUtil::count_file(ssAflQueue.str().c_str(), taskfiles);
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: we found " << taskcount << " testcase(s) in " << ssAflQueue.str() << "\n";
#endif
			typeof(taskfiles.begin()) it = taskfiles.begin();
			while(it != taskfiles.end()){
				std::stringstream taskfile;
				try{
					const char *filename = basename((*it).c_str());
					if (!filename) {
						s2e()->getDebugStream() << "FuzzySearcher: Could not allocate memory for file basename.\n";
						exit(0); // must be interrupted
					}
					taskfile << (*it).c_str();
					std::stringstream bckfile;
					bckfile << m_inicasepool.c_str() << filename;
#ifdef DEBUG
				s2e()->getDebugStream() << "FuzzySearcher: Copying filename: " << taskfile.str() << " from the queue to"
						" filename: " << bckfile.str() << "\n";
#endif
					FileUtil::copyfile(taskfile.str().c_str(), bckfile.str().c_str(), false,false);
					it++;
				}catch(...){
					it++;
					continue;
				}
			}
		}
		prepareNextState(nextState);
	}
}

void FuzzySearcher::onStateKill(S2EExecutionState *currentState) {
	//当状态被杀死时，生成测试用例并将其投放至AFL的队列中
	//step1. construct name and generate the testcase
	std::stringstream ssgeneratedCase;
	std::string destfilename;
	if(1){
		ssgeneratedCase << "testcases/" << getCurrentLoop() << "-"
				<< currentState->getID() << "-" << m_symbolicfilename; // Lopp-StateID-m_symbolicfilename
		destfilename = s2e()->getOutputFilename(ssgeneratedCase.str().c_str()) ;
	}else{
		ssgeneratedCase << m_aflOutputpool << "queue/" << getCurrentLoop() << "-"
				<< currentState->getID() << "-" << m_symbolicfilename; // Lopp-StateID-m_symbolicfilename
		destfilename = ssgeneratedCase.str();
	}
#ifdef DEBUG
	s2e()->getDebugStream() << "FuzzySearcher: Generating testcase: " << destfilename << ".\n";
#endif
	generateCaseFile(currentState, destfilename);
	/*
	 * AFLROOT=/home/epeius/work/afl-1.96b
	 * $AFLROOT/afl-fuzz -m 4096M -t 50000 -i /home/epeius/work/DSlab.EPFL/FinalTest/evincetest/seeds/ -o /home/epeius/work/DSlab.EPFL/FinalTest/evincetest/res/ -Q /usr/bin/evince @@
	 */
	if(!m_AFLStarted){//如果AFL还未启动，则启动AFL
		std::stringstream aflCmdline;
		std::string generated_dir =  s2e()->getOutputDirectory() + "/testcases";
		aflCmdline << m_aflRoot << "afl-fuzz -m 4096M -t 5000 -i " << generated_dir << " -o " << m_aflOutputpool <<
										(m_aflBinaryMode ? " -Q " : "") << m_aflAppArgs << " &";
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: AFL command line is: " << aflCmdline.str() << "\n";
#endif
		system(aflCmdline.str().c_str()); //FIXME:阻塞??:
		m_AFLStarted = true;
	}
}

klee::Executor::StatePair FuzzySearcher::prepareNextState(
		S2EExecutionState *state, bool isinitial) {

	klee::Executor::StatePair sp;
	state->jumpToSymbolicCpp();
	bool oldForkStatus = state->isForkingEnabled();
	state->enableForking();
	if (isinitial) {
		m_dummy_symb = state->createSymbolicValue("dummy_symb_var",
				klee::Expr::Int32); // we need to create an initial state which can be used to continue execution
		m_current_conditon = 0;
	}
	//	printf("FuzzySearcher: prepareNextState\n");
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

	if(m_loops >= m_MAXLOOPs){//已达到最大次数限制，结束S2E和AFL
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: Ready to exit\n";
#endif
		try{
			char cmd[] = "pgrep -l afl-fuzz";
			FILE *pp = popen(cmd, "r"); //建立管道
			if (!pp) {
				s2e()->getDebugStream() << "FuzzySearcher: Cannot open the pipe to read\n";
				exit(0);
			}
			char tmp[128]; //设置一个合适的长度，以存储每一行输出
			while (fgets(tmp, sizeof(tmp), pp) != NULL) {
				if (tmp[strlen(tmp) - 1] == '\n') {
					tmp[strlen(tmp) - 1] = '\0'; //去除换行符
				}
				std::string str_tmp = tmp;
				str_tmp = str_tmp.substr(0, str_tmp.find_first_of(' '));
				int tmpPid = atoi(str_tmp.c_str());
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: try to kill pid: " << tmpPid << "\n";
#endif
				kill(tmpPid, SIGKILL);
			}
			pclose(pp); //关闭管道
		}catch(...){
			s2e()->getDebugStream() << "FuzzySearcher: Cannot kill AFL, why?\n";
		}
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: Reach the maxmium iteration, quitting...\n";
#endif
		//由于给AFL发送了KILL
		qemu_system_shutdown_request();
		return sp;
	}

	//从任务池中领取任务
	getNewCaseFromPool(fs);
	m_loops += 1; //迭代轮数加1
#ifdef DEBUG
	s2e()->getDebugStream() << "FuzzySearcher: Ready to start " << m_loops << " iteration(s).\n";
#endif

	m_key_enter_sent = false;
	CorePlugin *plg = s2e()->getCorePlugin();
	m_timerconn = plg->onTimer.connect(
							sigc::mem_fun(*this, &FuzzySearcher::onTimer));
	llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
	m_currentTime = curTime.seconds();
	S2EExecutionState::resetLastSymbolicId();
	s2e()->getExecutor()->updateStates(state);
	return sp;
}

void FuzzySearcher::onTimer() {
	llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
	if (m_currentTime < (curTime.seconds() - m_autosendkey_interval)) {
		m_currentTime = curTime.seconds();
		//让程序自动运行起来,应该是延时一会儿再发，否则收不到。
		if (m_autosendkey_enter && !m_key_enter_sent) {
			int keycode = 0x9c; //kp_enter

			if (keycode & 0x80) {
				kbd_put_keycode(0xe0);
			}
			kbd_put_keycode(keycode & 0x7f);

#ifdef DEBUG
			s2e()->getDebugStream() << "FuzzySearcher: Automatically sent kp_enter to QEMU.\n";
#endif
			//kbd_put_keycode(keycode);
			m_key_enter_sent = true;
			m_timerconn.disconnect();
		}
	}

}
/**
 * Fetch新的测试用例
 */
S2EExecutionState* FuzzySearcher::getNewCaseFromPool(S2EExecutionState* instate) {
	bool done = false;
	int idlecounter = 0;
	while (!done) {
		sleep(1);
		idlecounter ++;
		//收集用例数量
		std::vector<std::string> taskfiles;
		int taskcount = FileUtil::count_file(this->m_inicasepool.c_str(),
				taskfiles);
#ifdef DEBUG
		s2e()->getDebugStream() << "FuzzySearcher: Find " << taskcount << " testcases in the queue.\n";
#endif
		int maxfile = 0;
		std::string selectedcasename;
		// select random a file to handle
		try {
			maxfile = taskcount;
			int selectindex = maxfile - 1;
			if (maxfile > 0) {
				selectindex = rand() % maxfile;
				selectedcasename = taskfiles[selectindex];
			} else {
				continue;
			}
		} catch (...) {
			continue;
		}

		if (maxfile > 0) {
			std::string cachefile = "";
			try {
				std::stringstream taskfile;
				taskfile << selectedcasename;
#ifdef DEBUG
				s2e()->getDebugStream() << "FuzzySearcher: Selecting filename: " << selectedcasename << " from the queue.\n";
#endif
				//copy this to queue and rename it.
				const char *filename = basename(selectedcasename.c_str());
				if (!filename) {
					s2e()->getDebugStream() << "FuzzySearcher: Could not allocate memory for file basename ---2.\n";
					exit(0); // must be interrupted
				}
				std::stringstream bckfile;
				bckfile << m_curcasepool.c_str() << filename;
#ifdef DEBUG
				s2e()->getDebugStream() << "FuzzySearcher: Copying filename: " << taskfile.str() << " from the queue to"
						" filename: " << bckfile.str() << "\n";
#endif
				FileUtil::copyfile(taskfile.str().c_str(),
						bckfile.str().c_str(), false,false);

				std::stringstream rnmfile;
				rnmfile << m_curcasepool.c_str() << m_symbolicfilename;
#ifdef DEBUG
				s2e()->getDebugStream() << "FuzzySearcher: Renaming filename: " << bckfile.str() << " from the queue to"
										" filename: " << rnmfile.str() << "\n";
#endif
				FileUtil::renamefile(bckfile.str().c_str(), rnmfile.str().c_str());
			} catch (...) {
				continue;
			}
		} else {
			continue;
		}
		done = true;
	}
	return instate;
}

bool FuzzySearcher::generateCaseFile(S2EExecutionState *state, std::string destfilename) {
	//copy out template file to destination file
	std::stringstream template_file;
	template_file << m_curcasepool.c_str() << m_symbolicfilename;
	if (!copyfile(template_file.str().c_str(), destfilename.c_str()))
		return false;
	//try to solve the constraint and write the result to destination file
	int fd = open(destfilename.c_str(), O_RDWR);
	if (fd < 0) {
		s2e()->getDebugStream() << "could not open dest file: "
				<< destfilename.c_str() << "\n";
		close(fd);
		return false;
	}
	/* Determine the size of the file */
	off_t size = lseek(fd, 0, SEEK_END);
	if (size < 0) {
		s2e()->getDebugStream() << "could not determine the size of :"
				<< destfilename.c_str() << "\n";
		close(fd);
		return false;
	}

	off_t offset = 0;
	std::string delim_str = "_";
	const char *delim = delim_str.c_str();
	char *p;
	char maxvarname[1024] = { 0 };
	ConcreteInputs out;
	bool success = s2e()->getExecutor()->getSymbolicSolution(*state, out);

	if (!success) {
		s2e()->getWarningsStream() << "Could not get symbolic solutions"
				<< '\n';
		return false;
	}
	ConcreteInputs::iterator it;
	for (it = out.begin(); it != out.end(); ++it) {
		const VarValuePair &vp = *it;
		std::string varname = vp.first;
		// "__symfile___%s___%d_%d_symfile__value_%02x",filename, offset,size,buffer[buffer_i]);
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
			return false;
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
				return false;
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
				return false;
			}
		}
	}
	close(fd);
	return true;
}

#define BUFFER_SIZE 4
bool FuzzySearcher::copyfile(const char* fromfile, const char* tofile) {
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

void FuzzySearcher::onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &md){

}


void FuzzySearcher::onModuleTranslateBlockStart(ExecutionSignal* es,
		S2EExecutionState* state, const ModuleDescriptor &mod,
		TranslationBlock* tb, uint64_t pc){
	if (!tb) {
		return;
	}
	if(m_mainModule == mod.Name){
		es->connect(sigc::mem_fun(*this, &FuzzySearcher::slotExecuteBlockStart));
	}

}
void FuzzySearcher::onModuleTranslateBlockEnd(ExecutionSignal *signal,
		S2EExecutionState* state, const ModuleDescriptor &module,
		TranslationBlock *tb, uint64_t endPc,
		bool staticTarget, uint64_t targetPc) {
	if (!tb) {
		return;
	}
	if(m_mainModule == module.Name){
		signal->connect(sigc::mem_fun(*this, &FuzzySearcher::slotExecuteBlockEnd));
	}

}
/**
 */
void FuzzySearcher::slotExecuteBlockStart(S2EExecutionState *state,
		uint64_t pc) {
	const ModuleDescriptor *curMd = m_detector->getCurrentDescriptor(state);
	if(!curMd){
		return;
	}
#ifdef DEBUG
	s2e()->getDebugStream(state) << "FuzzySearcher: Find module when executing, we are in " << curMd->Name
						<< ", current BB is: " << hexval(pc) << ".\n";
#endif
	// do work here.
	if(!m_AFLStarted)//将bitmap共享内存的创建权交给afl
		return;
	if(!m_findBitMapSHM){
		m_findBitMapSHM = getAFLBitmapSHM();
	}else{
		assert(m_aflBitmapSHM && "AFL's bitmap is NULL, why??");
		DECLARE_PLUGINSTATE(FuzzySearcherState, state);
		/*
		 * cur_location = (block_address >> 4) ^ (block_address << 8);
			shared_mem[cur_location ^ prev_location]++;
			prev_location = cur_location >> 1;
		 */
#ifdef DEBUG
		s2e()->getDebugStream(state) << "FuzzySearcher: Ready to update AFL bitmap.\n";
		bool success = plgState->updateAFLBitmapSHM(m_aflBitmapSHM, pc);
		if(success)
			s2e()->getDebugStream(state) << "FuzzySearcher: Successfully updated AFL bitmap.\n";
		else
			s2e()->getDebugStream(state) << "FuzzySearcher: Failed to update AFL bitmap.\n";
#else
		bool success = plgState->updateAFLBitmapSHM(m_aflBitmapSHM, pc);
#endif
	}
}

void FuzzySearcher::slotExecuteBlockEnd(S2EExecutionState *state,
		uint64_t pc) {

}

bool FuzzySearcher::getAFLBitmapSHM(){
	m_aflBitmapSHM = NULL;
	key_t shmkey;
	do{
		if((shmkey = ftok("/tmp/aflbitmap", 1)) < 0){
			s2e()->getDebugStream() << "FuzzySearcher: ftok() error: " << strerror(errno) << "\n";
			return false;
		}
		int shm_id;
		try{
			if(!m_findBitMapSHM)
				sleep(5);//等待AFL完成
			shm_id = shmget(shmkey, AFL_BITMAP_SIZE, IPC_CREAT | 0600);
			if(shm_id < 0){
				s2e()->getDebugStream() << "FuzzySearcher: shmget() error: " << strerror(errno) << "\n";
				return false;
			}
			void * afl_area_ptr = shmat(shm_id, NULL, 0);
			if (afl_area_ptr == (void*)-1){
				s2e()->getDebugStream() << "FuzzySearcher: shmat() error: " << strerror(errno) << "\n";
				exit(1);
			}
			m_aflBitmapSHM = (unsigned char*)afl_area_ptr;
			m_findBitMapSHM = true;
#ifdef DEBUG
			s2e()->getDebugStream() << "FuzzySearcher: Share memory id is " << shm_id << "\n";
#endif
		}catch(...){
			return false;
		}
	}while(0);
	return true;
}

bool FuzzySearcherState::updateAFLBitmapSHM(unsigned char* AflBitmap, uint64_t curBBpc){
	uint64_t cur_location = (curBBpc >> 4) ^ (curBBpc << 8);
	cur_location &= AFL_BITMAP_SIZE - 1;
	if(cur_location >= AFL_BITMAP_SIZE)
		return false;
	AflBitmap[cur_location ^ m_prev_loc]++;
	m_prev_loc = cur_location >> 1;
	return true;
}
FuzzySearcherState::FuzzySearcherState() {
	m_plugin = NULL;
	m_state = NULL;
	m_prev_loc = 0;
}

FuzzySearcherState::FuzzySearcherState(S2EExecutionState *s, Plugin *p) {
	m_plugin = static_cast<FuzzySearcher*>(p);
	m_state = s;
	m_prev_loc = 0;;
}

FuzzySearcherState::~FuzzySearcherState() {
}

PluginState *FuzzySearcherState::clone() const {
	return new FuzzySearcherState(*this);
}

PluginState *FuzzySearcherState::factory(Plugin *p, S2EExecutionState *s) {
	FuzzySearcherState *ret = new FuzzySearcherState(s, p);
	return ret;
}
}
} /* namespace s2e */
