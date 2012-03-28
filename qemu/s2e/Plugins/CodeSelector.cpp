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

//#define NDEBUG

extern "C" {
#include "config.h"
#include "qemu-common.h"
extern struct CPUX86State *env;
}


#include <sstream>
#include <s2e/ConfigFile.h>

#include "CodeSelector.h"
#include "Opcodes.h"
#include "ModuleExecutionDetector.h"

#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>


using namespace s2e;
using namespace plugins;

S2E_DEFINE_PLUGIN(CodeSelector,
                  "Plugin for monitoring module execution",
                  "CodeSelector",
                  "ModuleExecutionDetector");


CodeSelector::CodeSelector(S2E *s2e) : Plugin(s2e) {

}


CodeSelector::~CodeSelector()
{

}

void CodeSelector::initialize()
{
    m_executionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_executionDetector);

    ConfigFile *cfg = s2e()->getConfig();

    bool ok = false;

    //Fetch the list of modules where forking should be enabled
    ConfigFile::string_list moduleList =
            cfg->getStringList(getConfigKey() + ".moduleIds", ConfigFile::string_list(), &ok);

    if (!ok || moduleList.empty()) {
        s2e()->getWarningsStream() << "You should specify a list of modules in " <<
                getConfigKey() + ".moduleIds\n";
    }

    foreach2(it, moduleList.begin(), moduleList.end()) {
        if (m_executionDetector->isModuleConfigured(*it)) {
            m_interceptedModules.insert(*it);
        }else {
            s2e()->getWarningsStream() << "CodeSelector: " <<
                    "Module " << *it << " is not configured\n";
            exit(-1);
        }
    }

    //Attach the signals
    m_executionDetector->onModuleTransition.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTransition));

    s2e()->getCorePlugin()->onCustomInstruction.connect(
        sigc::mem_fun(*this, &CodeSelector::onCustomInstruction));
}

void CodeSelector::onModuleTransition(
        S2EExecutionState *state,
        const ModuleDescriptor *prevModule,
        const ModuleDescriptor *currentModule
        )
{
    if (!currentModule) {
        state->disableForking();
        return;
    }

    const std::string *id = m_executionDetector->getModuleId(*currentModule);
    if (m_interceptedModules.find(*id) == m_interceptedModules.end()) {
        state->disableForking();
        return;
    }

    state->enableForking();
}

void CodeSelector::onPageDirectoryChange(
        S2EExecutionState *state,
        uint64_t previous, uint64_t current
        )
{
    if (m_pidsToTrack.empty()) {
        return;
    }

    Pids::const_iterator it = m_pidsToTrack.find(current);
    if (it == m_pidsToTrack.end()) {
        state->disableForking();
        return;
    }

    //Enable forking if we track the entire address space
    if ((*it).second == true) {
        state->enableForking();
    }
}

/**
 *  Monitor privilege level changes and enable forking
 *  if execution is in a tracked address space and in
 *  user-mode.
 */
void CodeSelector::onPrivilegeChange(
        S2EExecutionState *state,
        unsigned previous, unsigned current
        )
{
    if (m_pidsToTrack.empty()) {
        return;
    }

    Pids::const_iterator it = m_pidsToTrack.find(state->getPid());
    if (it == m_pidsToTrack.end()) {
        //Not in a tracked process
        state->disableForking();
        return;
    }

    //We are inside a process that we are tracking.
    //Check now if we are in user-mode.
    if ((*it).second == false) {
        //XXX: Remove hard-coded CPL level. It is x86-architecture-specific.
        if (current == 3) {
            //Enable forking in user mode.
            state->enableForking();
        } else {
            state->disableForking();
        }
    }
}

void CodeSelector::opSelectProcess(S2EExecutionState *state)
{
    bool ok = true;
    uint32_t isUserSpace;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &isUserSpace, 4);


    if (isUserSpace) {
        //Track the current process, but user-space only
        m_pidsToTrack[state->getPid()] = false;

        if (!m_privilegeTracking.connected()) {
            m_privilegeTracking = s2e()->getCorePlugin()->onPrivilegeChange.connect(
                    sigc::mem_fun(*this, &CodeSelector::onPrivilegeChange));
        }
    } else {
        m_pidsToTrack[state->getPid()] = true;

        if (!m_privilegeTracking.connected()) {
            m_privilegeTracking = s2e()->getCorePlugin()->onPageDirectoryChange.connect(
                    sigc::mem_fun(*this, &CodeSelector::onPageDirectoryChange));
        }
    }
}

void CodeSelector::opUnselectProcess(S2EExecutionState *state)
{
    bool ok = true;
    uint32_t pid;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &pid, 4);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "CodeSelector: Could not read the pid value of the process to disable.\n";
        return;
    }

    if (pid == 0) {
        pid = state->getPid();
    }

    m_pidsToTrack.erase(pid);

    if (m_pidsToTrack.empty()) {
        m_privilegeTracking.disconnect();
        m_addressSpaceTracking.disconnect();
    }
}

bool CodeSelector::opSelectModule(S2EExecutionState *state)
{
    bool ok = true;
    //XXX: 32-bits guests only
    uint32_t moduleId;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &moduleId, sizeof(moduleId));

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "CodeSelector: Could not read the module id pointer.\n";
        return false;
    }

    std::string strModuleId;
    if (!state->readString(moduleId, strModuleId)) {
        s2e()->getWarningsStream(state)
            << "CodeSelector: Could not read the module id string.\n";
        return false;
    }

    if (m_executionDetector->isModuleConfigured(strModuleId)) {
        m_interceptedModules.insert(strModuleId);
    }else {
        s2e()->getWarningsStream() << "CodeSelector: " <<
                "Module " << strModuleId << " is not configured\n";
        return false;
    }

    s2e()->getMessagesStream() << "CodeSelector: tracking module " << strModuleId << '\n';

    return true;
}

void CodeSelector::onCustomInstruction(
        S2EExecutionState *state,
        uint64_t operand
        )
{
    if (!OPCODE_CHECK(operand, CODE_SELECTOR_OPCODE)) {
        return;
    }

    uint64_t subfunction = OPCODE_GETSUBFUNCTION(operand);

    switch(subfunction) {
        //Track the currently-running process (either whole system or user-space only)
        case 0: {
            opSelectProcess(state);
        }
        break;


        //Disable tracking of the selected process
        //The process's id to not track is in the ecx register.
        //If ecx is 0, untrack the current process.
        case 1: {
            opUnselectProcess(state);
        }
        break;

        //Adds the module id specified in ecx to the list
        //of modules where to enable forking.
        case 2: {
            if (opSelectModule(state)) {
                tb_flush(env);
                state->setPc(state->getPc() + OPCODE_SIZE);
                throw CpuExitException();
            }
        }
        break;
    }
}


#if 0

CodeSelector::CodeSelector(S2E *s2e) : Plugin(s2e) {
    m_Tb = NULL;
}


CodeSelector::~CodeSelector()
{
    foreach2(it, m_CodeSelDesc.begin(), m_CodeSelDesc.end()) {
        delete *it;
    }

    foreach2(it, m_AggregatedBitmap.begin(), m_AggregatedBitmap.end()) {
        uint8_t *Bmp = (*it).second.first;
        if (Bmp) {
            delete [] Bmp;
        }
    }
}

/**
 *  Read the cfg file and set up the regions
 *  that should be symbexeced. The regions are
 *  specifed relative to modules, at byte-level granularity.
 */
void CodeSelector::initialize()
{
    m_executionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_executionDetector);

    ConfigFile *cfg = s2e()->getConfig();

    std::vector<std::string> keyList;
    std::set<std::string> ModuleIds;

    //Find out about the modules we are interested in
    keyList = cfg->getListKeys(getConfigKey());
    foreach2(it, keyList.begin(), keyList.end()) {
        CodeSelDesc *csd = new CodeSelDesc(s2e());

        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!csd->initialize(sk.str())) {
            printf("Could not initialize code descriptor for %s\n", (*it).c_str());
            exit(-1);
        }
        m_CodeSelDesc.insert(csd);
    }

    m_executionDetector->onModuleTransition.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTransition));

    m_executionDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTranslateBlockStart));

    m_executionDetector->onModuleTranslateBlockEnd.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTranslateBlockEnd));
}

bool CodeSelector::instrumentationNeeded(const ModuleExecutionDesc &desc,
                                         uint64_t pc)
{
    ModuleToBitmap::iterator it = m_AggregatedBitmap.find(desc.descriptor.Name);
    if (it != m_AggregatedBitmap.end()) {
        const BitmapDesc &bd = (*it).second;
        uint64_t offset = desc.descriptor.ToRelative(pc);
        if (offset >= bd.second) {
            printf("Warning: descriptor size does not match the registered size. "
                "Maybe another module with same name was loaded.\n");
            return false;
        }
        return bd.first[offset/8] & (1<<(offset&7));
    }

    //There is no cached bitmap, read out all code selector descriptors that
    //match the given module name and build the aggregated bitmap.
    BitmapDesc newBmp(NULL, 0);

    foreach2(it, m_CodeSelDesc.begin(), m_CodeSelDesc.end()) {
        CodeSelDesc *cd = *it;
        if (cd->getModuleId() != desc.id) {
            continue;
        }

        cd->initializeBitmap(cd->getId(),
                desc.descriptor.NativeBase, desc.descriptor.Size);

        if(!newBmp.first) {
            newBmp.first = new uint8_t[desc.descriptor.Size/8];
            newBmp.second = desc.descriptor.Size;
        }

        for (unsigned i=0; i<desc.descriptor.Size/8; i++) {
            newBmp.first[i] |= cd->getBitmap()[i];
        }
    }

    m_AggregatedBitmap[desc.descriptor.Name] = newBmp;

    uint64_t offset = desc.descriptor.ToRelative(pc);
    return newBmp.first[offset/8] & (1<<(offset&7));
}

void CodeSelector::onModuleTranslateBlockStart(ExecutionSignal *signal,
        S2EExecutionState *state,
        const ModuleExecutionDesc* desc,
        TranslationBlock *tb,
        uint64_t pc)
{
    TRACE("%"PRIx64"\n", pc);

    //Translation may have been interrupted because of an error
    if (m_Tb) {
        m_TbConnection.disconnect();
    }

    m_Tb = tb;
    m_TbSymbexEnabled = !instrumentationNeeded(*desc, pc); //!whatever is in bitmap for that PC
    m_TbMod = desc;

    //register instruction translator now
    CorePlugin *plg = s2e()->getCorePlugin();
    m_TbConnection = plg->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &CodeSelector::onTranslateInstructionStart)
    );
}

void CodeSelector::onTranslateInstructionStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc
    )
{
    if (tb != m_Tb) {
        TRACE("%"PRIx64"\n", pc);
        assert(tb == m_Tb);
    }

    //if current pc is symbexecable
    bool s = instrumentationNeeded(*m_TbMod, pc);

    if (s != m_TbSymbexEnabled) {
        TRACE("Connecting symbex\n");
        signal->connect(sigc::mem_fun(*this, &CodeSelector::symbexSignal));
        m_TbSymbexEnabled = s;
    }

}


void CodeSelector::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleExecutionDesc*,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    TRACE("%"PRIx64" StaticTarget=%d TargetPc=%"PRIx64"\n", endPc, staticTarget, targetPc);
    m_Tb = NULL;
    m_TbConnection.disconnect();
}

void CodeSelector::symbexSignal(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(CodeSelectorState, state);
    TRACE("%p\n", plgState->m_CurrentModule);
    if (plgState->m_CurrentModule) {
        if (!plgState->isSymbolic(pc)) {
            s2e()->getExecutor()->disableSymbolicExecution(state);
        }else {
            s2e()->getExecutor()->enableSymbolicExecution(state);
        }
    }
}





void CodeSelector::onModuleTransition(
        S2EExecutionState *state,
        const ModuleExecutionDesc *prevModule,
        const ModuleExecutionDesc *currentModule
        )
{
    DECLARE_PLUGINSTATE(CodeSelectorState, state);

    //The module was not declared, disable symbexec in it
    if (currentModule == NULL) {
        plgState->m_CurrentModule = NULL;
        s2e()->getExecutor()->disableSymbolicExecution(state);
        return;
    }
    TRACE("Activating module...\n");
    //Check that we are inside an interesting module
    //This is not the same as the previous NULL check
    //(users might want to disable temporarily some declared modules)
    const CodeSelDesc *activeDesc = plgState->activateModule(this, currentModule);
    if (!activeDesc) {
        plgState->m_CurrentModule = NULL;
        s2e()->getExecutor()->disableSymbolicExecution(state);
        return;
    }
    plgState->m_CurrentModule = currentModule;
}

CodeSelectorState::CodeSelectorState()
{
    m_CurrentModule = NULL;
}

CodeSelectorState::~CodeSelectorState()
{

}

CodeSelectorState* CodeSelectorState::clone() const
{
    assert(false && "Not implemented");
    return NULL;
}

PluginState *CodeSelectorState::factory(Plugin *p, S2EExecutionState *state)
{
    return new CodeSelectorState();
}

/**
 *  Return the active module, NULL if no module could be activated
 */
const CodeSelDesc* CodeSelectorState::activateModule(CodeSelector *plugin, const ModuleExecutionDesc* mod)
{
    if (m_ActiveModDesc == mod) {
        return m_ActiveSelDesc;
    }

    ActiveModules::iterator amit = m_ActiveModules.find(*mod);
    if (amit != m_ActiveModules.end()) {
        //Do some caching first...
        m_ActiveModDesc = mod;
        m_ActiveSelDesc = (*amit).second;
        return (*amit).second;
    }

    //Active module not found.

    //1. Find the configured code selection descriptor
    const CodeSelDesc *foundDesc = NULL;
    const CodeSelector::ConfiguredCodeSelDesc &ccsd = plugin->getConfiguredDescriptors();
    foreach2(it, ccsd.begin(), ccsd.end()) {
        const CodeSelDesc *csd = *it;
        bool skip = false;

        if (csd->getModuleId() == mod->id) {
            //Check that this module is not already active
            foreach2(ait, m_ActiveModules.begin(), m_ActiveModules.end()) {
                if (*(*ait).second == *csd) {
                    skip = true;
                    break;
                }
            }
            if (skip) {
                continue;
            }
            foundDesc = csd;
            break;
        }
    }


    if (!foundDesc) {
        TRACE("Did not find descriptor for %s\n", mod->id.c_str());
        m_ActiveModDesc = mod;
        m_ActiveSelDesc = NULL;
        return m_ActiveSelDesc;
    }

    TRACE("Found configured descriptor %s for %s\n", foundDesc->getId().c_str(),
            mod->id.c_str());


    //2. If descriptor has no context, create an active descriptor
    //and return.
    if (foundDesc->getContextId().size() == 0) {
        m_ActiveModDesc = mod;
        m_ActiveSelDesc = foundDesc;
        m_ActiveModules[*mod] = foundDesc;
        return foundDesc;
    }

    TRACE("Looking for context...\n");
    const CodeSelDesc *foundCtxDesc = NULL;
    //3. Otherwise, look for parent context.
    foreach2(it, m_ActiveModules.begin(), m_ActiveModules.end()) {
        if ((*it).second->getId() != foundDesc->getContextId()) {
            continue;
        }
        if ((*it).first.descriptor.Pid != mod->descriptor.Pid) {
            continue;
        }
        foundCtxDesc = (*it).second;
        TRACE("Context found (%s)\n", foundDesc->getId().c_str());
        break;
    }

    //If not found, return NULL (and cache the value)
    if (!foundCtxDesc) {
        m_ActiveModDesc = mod;
        m_ActiveSelDesc = NULL;
        return NULL;
    }

    //4. Else, create the new active descriptor
    m_ActiveModDesc = mod;
    m_ActiveSelDesc = foundDesc;
    m_ActiveModules[*mod] = foundDesc;
    return foundDesc;
}

bool CodeSelectorState::isSymbolic(uint64_t absolutePc)
{
    if (!m_CurrentModule) {
        return false;
    }

    assert(m_ActiveModDesc);
    assert(m_ActiveSelDesc);

    assert(m_ActiveModDesc->descriptor.Contains(absolutePc));
    uint8_t *bmp = m_ActiveSelDesc->getBitmap();



    uint64_t offset = m_ActiveModDesc->descriptor.ToRelative(absolutePc);
    return bmp[offset/8] & (1<<(offset&7));
}



/////////////////

CodeSelDesc::CodeSelDesc(S2E *s2e)
{
    m_Context = NULL;
    m_Bitmap = NULL;
    m_ModuleSize = 0;
    m_s2e = s2e;
}

CodeSelDesc::~CodeSelDesc()
{
    if (m_Context) {
        delete [] m_Context;
    }
}


//XXX: might want to move to ConfigFile.cpp if lists of pairs turn out
//to be common.
bool CodeSelDesc::getRanges(const std::string &key, CodeSelDesc::Ranges &ranges)
{
   unsigned listSize;
   bool ok;

   ConfigFile *cfg = m_s2e->getConfig();

   listSize = cfg->getListSize(key, &ok);
   if (!ok) {
       return false;
   }

   for (unsigned i=0; i<listSize; i++) {
       std::stringstream path;
       path << key << "[" << std::dec << (i+1) << "]";
       std::vector<uint64_t> range;
       range = cfg->getIntegerList(path.str(), range, &ok);
       if (!ok) {
            return false;
       }
       if (range.size() != 2) {
           printf("%s must be of (start,size) format\n", path.str().c_str());
           return false;
       }

       ranges.push_back(Range(range[0], range[1]));
   }

   return true;
}

bool CodeSelDesc::validateRanges(const Ranges &R, uint64_t nativeBase, uint64_t size) const
{
    bool allValid = true;
    foreach2(it, R.begin(), R.end()) {
        const Range &r = *it;
        if ((r.first > r.second)||
            (r.second > nativeBase + size) ||
            (r.first < nativeBase)) {
                printf("Range (%#"PRIx64", %#"PRIx64") is invalid!\n",
                    r.first, r.second);
            allValid = false;
        }
        TRACE("Range (%#"PRIx64", %#"PRIx64") is valid\n", r.first, r.second);
    }
    return allValid;
}

void CodeSelDesc::getRanges(CodeSelDesc::Ranges &include, CodeSelDesc::Ranges &exclude,
                             const std::string &id,
                             uint64_t nativeBase, uint64_t size)
{
    std::stringstream includeKey, excludeKey;
    bool ok = false;
    ConfigFile *cfg = m_s2e->getConfig();

    include.clear();
    exclude.clear();

    TRACE("Reading include ranges for %s...\n", id.c_str());
    includeKey << id << ".includeRange";

    std::string fk = cfg->getString(includeKey.str(), "", &ok);
    if (ok) {
        if (fk.compare("full") == 0) {
            printf("doing full symbex on %s\n", id.c_str());
            include.push_back(Range(nativeBase, nativeBase+size-1));
        }else {
            printf("Invalid range %s. Must be 'full' or"
                " a list of pairs of ranges\n", fk.c_str());
        }
    }else {

        ok = getRanges(includeKey.str(), include);
        if (!ok) {
            printf("No include ranges or invalid ranges specified for %s. "
                "Symbolic execution will be disabled.\n", id.c_str());
        }
        if (!validateRanges(include, nativeBase, size)) {
            printf("Clearing include ranges\n");
            include.clear();
        }
    }

    TRACE("Reading exclude ranges for %s...\n", id.c_str());
    excludeKey << id << ".excludeRange";
    ok = getRanges(excludeKey.str(), exclude);
    if (!ok) {
        printf("No exclude ranges or invalid ranges specified for %s\n", id.c_str());
    }

    if (!validateRanges(exclude, nativeBase, size)) {
        printf("Clearing exclude ranges\n");
        exclude.clear();
    }
}


//Reads the cfg file to decide what part of the module
//should be symbexec'd.
//XXX: check syntax on initialize()?
void CodeSelDesc::initializeBitmap(const std::string &id,
                                   uint64_t nativeBase, uint64_t size)
{
    Ranges include, exclude;

    assert(!m_Bitmap);

    TRACE("Initing bitmap for %s\n", id.c_str());
    getRanges(include, exclude, id, nativeBase, size);

    assert(!m_Bitmap);
    m_Bitmap = new uint8_t[size/8];


    TRACE("Bitmap @%p size=%#"PRIx64"\n", m_Bitmap, size/8);

    //By default, symbolic execution is disabled for the entire module
    memset(m_Bitmap, 0x00, size/8);

    foreach2(it, include.begin(), include.end()) {
        TRACE("Include start=%#"PRIx64" end=%#"PRIx64"\n", (*it).first, (*it).second);
        uint64_t start = (*it).first - nativeBase;
        uint64_t end = (*it).second - nativeBase;
        for (unsigned i=start; i<end; i++) {
            m_Bitmap[i/8] |= 1 << (i % 8);
        }
    }

    foreach2(it, exclude.begin(), exclude.end()) {
        TRACE("Exclude start=%#"PRIx64" end=%#"PRIx64"\n", (*it).first, (*it).second);
        uint64_t start = (*it).first - nativeBase;
        uint64_t end = (*it).second - nativeBase;
        for (unsigned i=start; i<end; i++) {
            m_Bitmap[i/8] &= ~(1 << (i % 8));
        }
    }
}

bool CodeSelDesc::initialize(const std::string &key)
{
     std::stringstream ss;
     bool ok;

    ModuleExecutionDetector *executionDetector = (ModuleExecutionDetector*)m_s2e->getPlugin("ModuleExecutionDetector");
    assert(executionDetector);

    ConfigFile *cfg = m_s2e->getConfig();

    m_Id = key;

     //Fetch the module id
     ss << key << ".module";
     m_ModuleId =  cfg->getString(ss.str(), "", &ok);
     if (!ok) {
         printf("%s does not exist!\n", ss.str().c_str());
         return false;
     }

     //Retrive the context
     ss.str("");
     ss << key << ".context";
     m_ContextId =  cfg->getString(ss.str(), "", &ok);
     if (!ok) {
         printf("%s not configured!\n", ss.str().c_str());
     }

     const ConfiguredModulesById &CfgModules =
         executionDetector->getConfiguredModulesById();

     ConfiguredModulesById::iterator it;
     ModuleExecutionCfg tmp;
     tmp.id = m_ModuleId;
     it = CfgModules.find(tmp);
     if (it == CfgModules.end()) {
         printf("Module id %s not configured but is referenced in %s\n.",
             m_ModuleId.c_str(), ss.str().c_str());
         return false;
     }



     return true;
}

#endif
