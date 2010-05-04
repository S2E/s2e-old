extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "ExecutionTrace.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/System/TimeValue.h>

#include <iostream>
#include <sstream>



namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(ExecutionTrace, "Execution trace plugin", "",);

void ExecutionTrace::initialize()
{
    std::stringstream ss;
    std::string file;

    if (!createTable()) {
        exit(-1);
    }

    std::vector<std::string> keyList;
    keyList = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;
    foreach2(it, keyList.begin(), keyList.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str())) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the branch coverage sections"
            <<std::endl;
        exit(-1);
    }

    
}

bool ExecutionTrace::initSection(const std::string &cfgKey)
{
    //Fetch the coverage type
    std::string covType = s2e()->getConfig()->getString(cfgKey + ".tracetype");
    if (covType.compare("translationblocks") == 0) {
        return initTbTrace(cfgKey);
    }else {
        s2e()->getWarningsStream() << "Unsupported type of tracing "
            << covType << std::endl;
        return false;
    }
}

bool ExecutionTrace::createTable()
{
    const char *query = "create table ExecutionTrace(" 
          "'timestamp' unsigned big int,"  
          "'moduleId' varchar(30),"
          "'tbpc' unsigned big int,"
          "'tbtype' tinyint," 
          "'pid' unsigned big int"
          ");";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

bool ExecutionTrace::initTbTrace(const std::string &cfgKey)
{
    bool ok;
    std::string key = cfgKey + ".module";
    std::string moduleId =  s2e()->getConfig()->getString(key, "", &ok);

    if (!ok) {
        s2e()->getWarningsStream() << "You must specifiy " << key << std::endl;
        return false;
    }

    //Check that the interceptor is there
    ModuleExecutionDetector *executionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    if(!executionDetector) {
        s2e()->getWarningsStream() << "You must configure the moduleExecutionDetector plugin! " << std::endl;
        return false;
    }

    //Check that the module id is valid
    if (!executionDetector->isModuleConfigured(moduleId)) {
        s2e()->getWarningsStream() << 
            moduleId << " not configured in the execution detector! " << std::endl;
        return false;
    }
    
    //Registering listener
    executionDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &ExecutionTrace::onTranslateBlockStart)
    );

    m_Modules.insert(moduleId);


    return true;
}

void ExecutionTrace::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc* desc,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (m_Modules.find(desc->id) == m_Modules.end()) {
        return;
    }

    signal->connect(
        sigc::bind(sigc::mem_fun(*this, &ExecutionTrace::onExecution),
                   (const ModuleExecutionDesc*) desc)
   );
}

void ExecutionTrace::flushTable()
{
    if (m_Trace.size() < 100) {
        return;
    }
    std::cout << "Flushing trace" << std::endl;
    foreach2(it, m_Trace.begin(), m_Trace.end()) {
        const TraceEntry &te = *it;
        std::stringstream ss;
        ss << "insert into ExecutionTrace values(" 
            << te.timestamp << ",'" <<
            te.desc->id << "'," <<
            te.pc << "," << te.tbType << "," <<
            te.pid << ");";
        s2e()->getDb()->executeQuery(ss.str().c_str());
    }
    m_Trace.clear();
}

void ExecutionTrace::onExecution(S2EExecutionState *state, uint64_t pc, const ModuleExecutionDesc* desc)
{
    ETranslationBlockType TbType = state->getTb()->s2e_tb_type;

    llvm::sys::TimeValue timeStamp = llvm::sys::TimeValue::now();
    std::stringstream ss;
    uint64_t relPc = desc->descriptor.ToNativeBase(pc);

    //Do not trace everything for performance reasons
    if (TbType == TB_CALL || TbType == TB_RET) {

    TraceEntry te;
    te.desc = desc;
    te.tbType = (unsigned)TbType;
    te.timestamp = timeStamp.msec();
    te.pc = relPc;
    te.pid = state->getPid();
    
    m_Trace.push_back(te);
    flushTable();
    }
}

} // namespace plugins
} // namespace s2e


