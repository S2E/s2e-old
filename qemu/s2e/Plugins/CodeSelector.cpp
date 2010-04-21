#include <sstream>
#include <s2e/ConfigFile.h>

#include "CodeSelector.h"
#include "ModuleExecutionDetector.h"

#include <s2e/s2e.h>
#include <s2e/Utils.h>

using namespace s2e;
using namespace plugins;

S2E_DEFINE_PLUGIN(CodeSelector, 
                  "Plugin for monitoring module execution", 
                  "CodeSelector",
                  "ModuleExecutionDetector");

/**
 - When execution leaves all code regions
 */

CodeSelector::~CodeSelector()
{
    foreach2(it, m_Bitmap.begin(), m_Bitmap.end()) {
        uint8_t *Bmp = (*it).second;
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

    std::vector<std::string> moduleIds;

    moduleIds = s2e()->getConfig()->getListKeys("codeSelector");
    foreach2(it, moduleIds.begin(), moduleIds.end()) {
        m_ConfiguredModuleIds.insert(*it);
    }

    m_ExecutionDetector->onModuleTranslateBlockStart.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTranslateBlockStart));

    m_ExecutionDetector->onModuleTranslateBlockEnd.connect(
        sigc::mem_fun(*this, &CodeSelector::onModuleTranslateBlockEnd));
}

void CodeSelector::onModuleTranslateBlockStart(ExecutionSignal *signal, 
        S2EExecutionState *state,
        const ModuleExecutionDesc* desc,
        TranslationBlock *tb,
        uint64_t pc)
{
    assert(!m_Tb);
    m_Tb = tb;
    m_TbSymbexEnabled = !isSymbolic(*desc, pc); //!whatever is in bitmap for that PC
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
    assert(tb == m_Tb);

    //if current pc is symbexecable
    bool s = isSymbolic(*m_TbMod, pc);

    if (s != m_TbSymbexEnabled) {
        if (s) {
            signal->connect(sigc::mem_fun(*this, &CodeSelector::enableSymbexSignal));
        }else {
            signal->connect(sigc::mem_fun(*this, &CodeSelector::disableSymbexSignal));
        }
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
    m_Tb = NULL;
    m_TbConnection.disconnect();
}

void CodeSelector::disableSymbexSignal(S2EExecutionState *state, uint64_t pc)
{
    state->disableSymbExec();
}

void CodeSelector::enableSymbexSignal(S2EExecutionState *state, uint64_t pc)
{
    state->enableSymbExec();
}

bool CodeSelector::isSymbolic(const ModuleExecutionDesc &Desc, uint64_t absolutePc)
{
    assert(Desc.descriptor.Contains(absolutePc));
    uint8_t *bmp = getBitmap(Desc);
    
    if (!bmp) {
        return false;
    }

    uint64_t offset = Desc.descriptor.ToRelative(absolutePc);
    return bmp[offset/8] & (1<<(offset&7));

}

//XXX: might want to move to ConfigFile.cpp if lists of pairs turn out
//to be common.
bool CodeSelector::getRanges(const std::string &key, CodeSelector::Ranges &R)
{
   unsigned listSize;
   bool ok;

   ConfigFile *cfg = s2e()->getConfig();
   
   listSize = cfg->getListSize(key, &ok);
   if (!ok) {
       return false;
   }

   for (unsigned i=0; i<listSize; i++) {
       std::stringstream path;
       path << key << "[" << std::dec << i << "]";
       std::vector<uint64_t> range;
       range = cfg->getIntegerList(path.str(), range, &ok);
       if (!ok) {
            return false;
       }
       if (range.size() != 2) {
           std::cout << path.str() << " must be of (start,size) format" << std::endl;
           return false;
       }

       R.push_back(Range(range[0], range[1]));
   }

   return true;
}

bool CodeSelector::validateRanges(const ModuleExecutionDesc &Desc, const Ranges &R) const
{
    bool allValid = true;
    foreach2(it, R.begin(), R.end()) {
        const Range &r = *it;
        if ((r.first + r.second < r.first)||
            (r.first + r.second > Desc.descriptor.Size)) {
            std::cout << "Range (" << r.first << ", " << r.second <<
                ") is invalid" << std::endl;
            allValid = false;
        }
    }
    return allValid;
}

void CodeSelector::getRanges(const ModuleExecutionDesc &Desc, 
                             Ranges &include, Ranges &exclude)
{
    std::stringstream includeKey, excludeKey;
    bool ok;
    ConfigFile *cfg = s2e()->getConfig();

    include.clear();
    exclude.clear();
        
    includeKey << "codeSelector." << Desc.id << ".includeRange";
    
    std::string fk = cfg->getString(includeKey.str(), "", &ok);
    if (ok) {
        if (fk.compare("full") == 0) {
            include.push_back(Range(0, Desc.descriptor.Size));
        }else {
            std::cout << "Invalid range " << fk << ". Must be 'full' or a list of "
                "pairs of ranges" << std::endl;
        }
    }else {

        ok = getRanges(includeKey.str(), include);
        if (!ok) {
            std::cout << "No include ranges or invalid ranges specified for " << Desc.id << 
                ". Symbolic execution will be disabled." << std::endl;
        }
        if (!validateRanges(Desc, include)) {
            std::cout << "Clearing include ranges" << std::endl;
            include.clear();
        }
    }

    excludeKey << "codeSelector." << Desc.id << ".excludeRange";
    ok = getRanges(excludeKey.str(), exclude);
    if (!ok) {
        std::cout << "No exclude ranges or invalid ranges specified for " << Desc.id << std::endl;
    }
    if (!validateRanges(Desc, exclude)) {
        std::cout << "Clearing exclude ranges" << std::endl;
        exclude.clear();
    }
}


//Reads the cfg file to decide what part of the module
//should be symbexec'd.
//XXX: check syntax on initialize()?
uint8_t *CodeSelector::initializeBitmap(const ModuleExecutionDesc &Desc)
{
    Ranges include, exclude;

    getRanges(Desc, include, exclude);
    
    uint8_t *Bmp = new uint8_t[Desc.descriptor.Size/8];
    assert(Bmp);

    m_Bitmap[Desc] = Bmp;

    //By default, symbolic execution is disabled for the entire module
    memset(Bmp, 0x00, Desc.descriptor.Size/8);

    foreach2(it, include.begin(), include.end()) {
        uint64_t start = (*it).first;
        uint64_t size = (*it).second;
        for (unsigned i=start; i<size; i++) {
            Bmp[i/8] |= 1 << (i % 8);
        }
    }
    
    foreach2(it, exclude.begin(), exclude.end()) {
        uint64_t start = (*it).first;
        uint64_t size = (*it).second;
        for (unsigned i=start; i<size; i++) {
            Bmp[i/8] &= ~(1 << (i % 8));
        }
    }

    return Bmp;
}

uint8_t *CodeSelector::getBitmap(const ModuleExecutionDesc &Desc)
{
    Bitmap::iterator it = m_Bitmap.find(Desc);
    if (it != m_Bitmap.end()) {
        return (*it).second;
    }
    
    return initializeBitmap(Desc);
}

void CodeSelector::onModuleTransition(
        S2EExecutionState *state,
        const ModuleExecutionDesc *prevModule, 
        const ModuleExecutionDesc *currentModule
        )
{
    //The module was not declared, disable symbexec in it
    if (currentModule == NULL) {
        state->disableSymbExec();
        return;
    }

    //Check that we are inside an interesting module
    //This is not the same as the previous NULL check
    //(users might want to disable temporarily some declared modules)
    if (m_ConfiguredModuleIds.find(currentModule->id) == m_ConfiguredModuleIds.end()) {
        state->disableSymbExec();
        return;
    }
 
    //If the module is declared for full symbex, enable it here.
    //...

    
    //Otherwise, no need to check when to enable symbexec, because
    //all interesting basic blocks will have a call to enable symbexec
    //at their beginning.
}