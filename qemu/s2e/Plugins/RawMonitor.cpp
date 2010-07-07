/**
 *  This plugin provides the means of manually specifying the location
 *  of modules in memory.
 *
 *  This allows things like defining poritions of the BIOS.
 */

#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include "RawMonitor.h"

#include <sstream>

using namespace std;

using namespace s2e;
using namespace s2e::plugins;

S2E_DEFINE_PLUGIN(RawMonitor, "Plugin for monitoring raw module events", "Interceptor");

RawMonitor::~RawMonitor()
{

}

bool RawMonitor::initSection(const std::string &cfgKey, const std::string &svcId)
{
    Cfg cfg;

    bool ok;
    cfg.name = s2e()->getConfig()->getString(cfgKey + ".name", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".name" << std::endl;
        return false;
    }

    cfg.size = s2e()->getConfig()->getInt(cfgKey + ".size", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".size" << std::endl;
        return false;
    }

    cfg.start = s2e()->getConfig()->getInt(cfgKey + ".start", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".start" << std::endl;
        return false;
    }

    cfg.nativebase = s2e()->getConfig()->getInt(cfgKey + ".nativebase", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".nativebase" << std::endl;
        return false;
    }

    m_cfg.push_back(cfg);
    return true;
}

void RawMonitor::initialize()
{
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    foreach2(it, Sections.begin(), Sections.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the WindowsService sections"
            <<std::endl;
        exit(-1);
    }

    m_onTranslateInstruction = s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
        sigc::mem_fun(*this, &RawMonitor::onTranslateInstructionStart));

}

void RawMonitor::onTranslateInstructionStart(ExecutionSignal *signal,
                                                   S2EExecutionState *state,
                                                   TranslationBlock *tb,
                                                   uint64_t pc)
{
    CfgList::const_iterator it;

    for (it = m_cfg.begin(); it != m_cfg.end(); ++it) {
        ModuleDescriptor md;
        const Cfg &c = *it;
        md.Name = c.name;
        md.NativeBase = c.nativebase;
        md.LoadBase = c.start;
        md.Size = c.size;

        s2e()->getDebugStream() << "RawMonitor loaded " << c.name << " " <<
                std::hex << "0x" << c.start << " 0x" << c.size << std::dec << std::endl;
        onModuleLoad.emit(state, md);
    }

    m_onTranslateInstruction.disconnect();
}


bool RawMonitor::getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I)
{
    return false;
}

bool RawMonitor::getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E)
{
    return false;
}

bool RawMonitor::isKernelAddress(uint64_t pc) const
{
    return false;
}

uint64_t RawMonitor::getPid(S2EExecutionState *s, uint64_t pc)
{
    return s->getPid();
}
