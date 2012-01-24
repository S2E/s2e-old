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

#include <sstream>
#include <s2e/ConfigFile.h>

#include "CodeSelector.h"
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
    m_ExecutionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_ExecutionDetector);

    ConfigFile *cfg = s2e()->getConfig();

    bool ok = false;

    //Fetch the list of modules where forking should be enabled
    ConfigFile::string_list moduleList =
            cfg->getStringList(getConfigKey() + ".modules", ConfigFile::string_list(), &ok);

    if (!ok) {
        s2e()->getWarningsStream() << "You must specify a list of modules in " <<
                getConfigKey() + ".modules\n";
        exit(-1);
    }

    foreach2(it, moduleList.begin(), moduleList.end()) {
        if (m_ExecutionDetector->isModuleConfigured(*it)) {
            m_interceptedModules.insert(*it);
        }else {
            s2e()->getWarningsStream() << "CodeSelector: " <<
                    "Module " << *it << " is not configured\n";
            exit(-1);
        }
    }

    //Attach the signals
    m_ExecutionDetector->onModuleTransition.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTransition));

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

    if (m_interceptedModules.find(currentModule->Name) ==
        m_interceptedModules.end()) {
        state->disableForking();
        return;
    }

    state->enableForking();
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
    m_ExecutionDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_ExecutionDetector);

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

    m_ExecutionDetector->onModuleTransition.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTransition));

    m_ExecutionDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTranslateBlockStart));

    m_ExecutionDetector->onModuleTranslateBlockEnd.connect(
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
