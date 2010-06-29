///XXX: Do not use, deprecated

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
        if (*it == "triggerAfter") {
            continue;
        }

        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str())) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the execution trace sections"
            <<std::endl;
        exit(-1);
    }

    m_Trace.reserve(10000);
    m_Ticks = 0;
    m_StartedTrace = false;

    m_StartTick = s2e()->getConfig()->getInt(getConfigKey() + ".triggerAfter");
    s2e()->getMessagesStream() << "Triggering execution trace after " << m_StartTick <<
        " seconds" << std::endl;

    m_TimerConnection = s2e()->getCorePlugin()->onTimer.connect(
        sigc::mem_fun(*this, &ExecutionTrace::onTimer)
    );
}

bool ExecutionTrace::createTable()
{
    const char *query = "create table ExecutionTrace("
          "'timestamp' unsigned big int,"
          "'moduleId' varchar(30),"
          "'tbpc' unsigned big int,"
          "'tbtype' tinyint,"
          "'pid' unsigned big int"
          "); create index if not exists executiontraceidx on executiontrace(moduleid, tbpc);";

    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}


bool ExecutionTrace::initSection(const std::string &cfgKey)
{
    //Fetch the coverage type
    std::string covType = s2e()->getConfig()->getString(cfgKey + ".tracetype");
    if (covType.compare("translationblocks") == 0) {
        m_TraceType = TRACE_TYPE_TB;
        return initTbTrace(cfgKey);
    }else if (covType.compare("instructioncount") == 0) {
        m_TraceType = TRACE_TYPE_INSTR;
        return initInstrCount(cfgKey);
    }else
    {
        s2e()->getWarningsStream() << "Unsupported type of tracing "
            << covType << std::endl;
        return false;
    }
}

bool ExecutionTrace::initInstrCount(const std::string &cfgKey)
{
    if (!initTbTrace(cfgKey)) {
        return false;
    }
    m_TotalExecutedInstrCount = 0;
    return true;
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
     m_ExecutionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    if(!m_ExecutionDetector) {
        s2e()->getWarningsStream() << "You must configure the moduleExecutionDetector plugin! " << std::endl;
        return false;
    }

    //Check that the module id is valid
    if (!m_ExecutionDetector->isModuleConfigured(moduleId)) {
        s2e()->getWarningsStream() <<
            moduleId << " not configured in the execution detector! " << std::endl;
        return false;
    }


    m_Modules.insert(moduleId);


    return true;
}

void ExecutionTrace::onTimer()
{
    flushTable();
    flushInstrCount();
    m_Ticks++;

    if (m_StartedTrace) {
        return;
    }

    if (m_Ticks < m_StartTick) {
        return;
    }

    s2e()->getMessagesStream() << "Starting tracing" << std::endl;

    if (m_TraceType == TRACE_TYPE_TB) {
    m_ExecutionDetector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this, &ExecutionTrace::onTranslateBlockStart)
        );
    }else{

      s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &ExecutionTrace::onTranslateInstructionStart)
        );
    }


    m_DetectedModule = false;
    m_StartedTrace = true;
}

/////////////////////////////////////////////////////////////////////////////////////
void ExecutionTrace::onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{

    if (!m_DetectedModule) {
        ModuleExecutionDesc md;
        if (!m_ExecutionDetector->getCurrentModule(state, &md)) {
            return;
        }


        s2e()->getDebugStream() << md.id << std::endl;
        if (m_Modules.find(md.id) == m_Modules.end()) {
            return;
        }
        m_DetectedModule = true;
    }
    //s2e()->getDebugStream() << "Translating "<< std::hex << pc << std::endl;

    signal->connect(
        sigc::mem_fun(*this, &ExecutionTrace::onTraceInstruction)
    );

}

void ExecutionTrace::onTraceInstruction(S2EExecutionState* state, uint64_t pc)
{
    //s2e()->getDebugStream() << "Executing "<< std::hex << pc << std::endl;
    ++m_TotalExecutedInstrCount;
}

void ExecutionTrace::flushInstrCount()
{
    if (m_TraceType == TRACE_TYPE_INSTR) {
        s2e()->getMessagesStream() << "EXECTRACE " << std::dec <<
            m_TotalExecutedInstrCount << " instructions " << std::endl ;
    }
}

/////////////////////////////////////////////////////////////////////////////////////

void ExecutionTrace::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc* desc,
        TranslationBlock *tb,
        uint64_t pc)
{
    /*if (m_Modules.find(desc->id) == m_Modules.end()) {
        return;
    }*/



    signal->connect(
        sigc::bind(sigc::mem_fun(*this, &ExecutionTrace::onExecution),
                   (const ModuleExecutionDesc*) desc)
   );
}



void ExecutionTrace::flushTable()
{
    s2e()->getDebugStream() << "Flushing execution trace..." << std::endl;

    s2e()->getDb()->executeQuery("begin transaction;");
    foreach2(it, m_Trace.begin(), m_Trace.end()) {
        const TraceEntry &te = *it;
        char buffer[512];
        sprintf(buffer, "insert into executiontrace values(%"PRIu64",'%s',%"PRIu64",%d,"
            "%"PRIu64");", te.timestamp, te.desc->id.c_str(), te.pc, te.tbType, te.pid);

        s2e()->getDb()->executeQuery(buffer);
    }
    s2e()->getDb()->executeQuery("end transaction;");

    m_Trace.clear();
}

void ExecutionTrace::onExecution(S2EExecutionState *state, uint64_t pc, const ModuleExecutionDesc* desc)
{
    ETranslationBlockType TbType = state->getTb()->s2e_tb_type;


    //Do not trace everything for performance reasons
    //if (TbType == TB_CALL || TbType == TB_RET)
    {

    llvm::sys::TimeValue timeStamp = llvm::sys::TimeValue::now();
    std::stringstream ss;
    uint64_t relPc = desc->descriptor.ToNativeBase(pc);

    TraceEntry te;
    te.desc = desc;
    te.tbType = (unsigned)TbType;
    te.timestamp = timeStamp.usec();
    te.pc = relPc;
    te.pid = state->getPid();

    if (m_Trace.size() > 0) {
        //Do not save duplicates (should be configurable)
        const TraceEntry &b = m_Trace.back();
        if (b.pc == te.pc) {
            return;
        }
    }

    m_Trace.push_back(te);

    if (m_Trace.size()<10000) {
        return;
    }

    flushTable();


#if 0
    ss << "insert into ExecutionTrace values("
            << te.timestamp << ",'" <<
            te.desc->id << "'," <<
            te.pc << "," << te.tbType << "," <<
            te.pid << ");";
        s2e()->getDb()->executeQuery(ss.str().c_str());

#else
#if 0
    s2e()->getDebugStream() << "TR " << te.timestamp << " " <<
            te.desc->id << " " <<
            te.pc << " " << te.tbType << " " <<
            te.pid << std::endl;
#endif
    //m_Trace.push_back(te);
    //flushTable();
#endif

    }

}

} // namespace plugins
} // namespace s2e


