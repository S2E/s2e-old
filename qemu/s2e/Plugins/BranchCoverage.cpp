extern "C" {
#include "config.h"
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}

#include "BranchCoverage.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/System/TimeValue.h>

#include <iostream>
#include <sstream>



namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BranchCoverage, "Branch Coverage plugin", "",);

void BranchCoverage::initialize()
{
    std::stringstream ss;
    std::string file;

#if 0
    bool ok=false;
    file = s2e()->getConfig()->getString(getConfigKey() + ".file", "coverage.dat", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "Invalid coverage file specified, using default."
             << std::endl;
    }
    
    ss << s2e()->getOutputDirectory();
    ss << "/" << file;

    m_Out.open(ss.str().c_str());
    if (!m_Out.is_open()) {
        s2e()->getWarningsStream() << "Could not open branch coverage file "
            << ss.str() << std::endl;
        exit(-1);
    }
#endif

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

bool BranchCoverage::initSection(const std::string &cfgKey)
{
    //Fetch the coverage type
    std::string covType = s2e()->getConfig()->getString(cfgKey + ".covtype");
    if (covType.compare("aggregated") == 0) {
        return initAggregatedCoverage(cfgKey);
    }else {
        s2e()->getWarningsStream() << "Unsupported type of coverage "
            << covType << std::endl;
        return false;
    }
}

bool BranchCoverage::createTable()
{
    const char *query = "create table BranchCoverage(" 
          "'timestamp' unsigned big int,"  
          "'moduleId' varchar(30),"
          "'sourceBr' unsigned big int,"
          "'destBr' unsigned big int,"
          "'pid' unsigned big int"
          ");";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

bool BranchCoverage::initAggregatedCoverage(const std::string &cfgKey)
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
    executionDetector->onModuleTranslateBlockEnd.connect(
        sigc::mem_fun(*this, &BranchCoverage::onTranslateBlockEnd)
    );

    m_Modules.insert(moduleId);


    return true;
}

void BranchCoverage::onTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc* desc,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    if (m_Modules.find(desc->id) == m_Modules.end()) {
        return;
    }

    signal->connect(
        sigc::bind(sigc::mem_fun(*this, &BranchCoverage::onExecution),
                   (const ModuleExecutionDesc*) desc)
    );
}

void BranchCoverage::onExecution(S2EExecutionState *state, uint64_t pc, const ModuleExecutionDesc* desc)
{
    ETranslationBlockType TbType = state->getTb()->s2e_tb_type;

    if (TbType == TB_JMP || TbType == TB_JMP_IND ||
        TbType == TB_COND_JMP || TbType == TB_COND_JMP_IND) {
            llvm::sys::TimeValue timeStamp = llvm::sys::TimeValue::now();
            std::stringstream ss;
            uint64_t instrPc = desc->descriptor.ToRelative(pc);
            uint64_t destPc = desc->descriptor.ToRelative(state->getPc());
            
            ss << "insert into BranchCoverage values(" 
                   << timeStamp.msec() << ",'" <<
                   desc->id << "'," <<
                   instrPc << "," << destPc << "," <<
                   state->getPid() << ");";
            s2e()->getDb()->executeQuery(ss.str().c_str());

    }
}

} // namespace plugins
} // namespace s2e


