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

#ifndef CODE_SELECTOR_PLUGIN_H

#define CODE_SELECTOR_PLUGIN_H

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <inttypes.h>
#include <set>
#include <string>

#include "ModuleExecutionDetector.h"

namespace s2e {
namespace plugins {

class CodeSelector:public Plugin
{
    S2E_PLUGIN
public:
    typedef std::set<std::string> Modules;

    //process id => true if track the entire address space
    //process id => false if track the user-space only
    typedef std::map<uint64_t, bool> Pids;

private:
    ModuleExecutionDetector *m_executionDetector;
    Modules m_interceptedModules;
    Pids m_pidsToTrack;

    sigc::connection m_addressSpaceTracking;
    sigc::connection m_privilegeTracking;

    void onModuleTransition(
        S2EExecutionState *state,
        const ModuleDescriptor *prevModule,
        const ModuleDescriptor *currentModule
    );

    void onPageDirectoryChange(
        S2EExecutionState *state,
        uint64_t previous, uint64_t current
    );

    void onPrivilegeChange(
        S2EExecutionState *state,
        unsigned previous, unsigned current
    );

    void onCustomInstruction(
        S2EExecutionState *state,
        uint64_t operand
    );

    void opSelectProcess(S2EExecutionState *state);
    void opUnselectProcess(S2EExecutionState *state);

    bool opSelectModule(S2EExecutionState *state);

public:
    CodeSelector(S2E* s2e);

    virtual ~CodeSelector();
    void initialize();
};

#if 0
class CodeSelDesc
{
    S2E *m_s2e;
    struct CodeSelDesc *m_Context;
    std::string m_Id;
    std::string m_ModuleId;
    std::string m_ContextId;
    uint8_t *m_Bitmap;
    unsigned m_ModuleSize;

public:
    typedef std::pair<uint64_t, uint64_t> Range;
    typedef std::vector<Range> Ranges;

    CodeSelDesc(S2E *);
    ~CodeSelDesc();

    bool getRanges(const std::string &key, Ranges &R);
    bool validateRanges(const Ranges &R, uint64_t NativeBase, uint64_t Size) const;
    void getRanges(CodeSelDesc::Ranges &include, CodeSelDesc::Ranges &exclude,
                             const std::string &id,
                             uint64_t nativeBase, uint64_t size);
    bool initialize(const std::string &key);

    void initializeBitmap(const std::string &id,
                          uint64_t nativeBase, uint64_t size);

    const std::string& getModuleId() const {
        return m_ModuleId;
    }

    const std::string& getId() const {
        return m_Id;
    }

    uint8_t *getBitmap() const {
        return m_Bitmap;
    }


    const std::string& getContextId() const {
        return m_ContextId;
    }

    bool operator==(const CodeSelDesc&csd) const {
        return m_Id == csd.m_Id;
    }

};

/**
 *  TBD: remove descritptors when modules are unloaded.
 */
class CodeSelector:public Plugin
{
    S2E_PLUGIN
public:
    typedef std::pair<uint8_t*,unsigned> BitmapDesc;
    typedef std::map<std::string, BitmapDesc > ModuleToBitmap;
    typedef std::set<CodeSelDesc*> ConfiguredCodeSelDesc;
    typedef std::map<ModuleExecutionDesc, CodeSelDesc *> RunTimeModuleMap;

private:
    ModuleExecutionDetector *m_ExecutionDetector;

    ConfiguredCodeSelDesc m_CodeSelDesc;

    //Keep state accross tb translation signals
    //to put the right amount of calls to en/disablesymbexec.
    TranslationBlock *m_Tb;
    bool m_TbSymbexEnabled;
    const ModuleExecutionDesc* m_TbMod;
    sigc::connection m_TbConnection;


    //It may happen that a module is instrumented in one process,
    //and not in the others, yet they share the same code.
    //The bitmap aggregates all the points where instrumentation
    //is needed, over all the module instances.
    //Disambiguation is done at runtime.
    //Maps a module name to its bitmap.
    ModuleToBitmap m_AggregatedBitmap;


    void onModuleTransition(
        S2EExecutionState *state,
        const ModuleExecutionDesc *prevModule,
        const ModuleExecutionDesc *currentModule
     );


    bool instrumentationNeeded(const ModuleExecutionDesc &desc,
                                         uint64_t pc);



public:
    CodeSelector(S2E* s2e);

    virtual ~CodeSelector();
    void initialize();
    const ConfiguredCodeSelDesc& getConfiguredDescriptors() const {
        return m_CodeSelDesc;
    }

private:

    void onModuleTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        const ModuleDescriptor &,
        TranslationBlock *tb,
        uint64_t pc);

    void onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc);

    void onTranslateInstructionStart(
            ExecutionSignal *signal,
            S2EExecutionState *state,
            TranslationBlock *tb,
            uint64_t pc
        );

    void symbexSignal(S2EExecutionState *state, uint64_t pc);

};

class CodeSelectorState : public PluginState
{
public:
    typedef std::map<ModuleExecutionDesc, const CodeSelDesc*, ModuleExecutionDesc> ActiveModules;
private:

    //Null when execution is outside of any symbexcable module.
    //This takes into account shared libraries
    const ModuleDescriptor *m_CurrentModule;

    ActiveModules m_ActiveModules;
    const ModuleExecutionDesc *m_ActiveModDesc;
    const CodeSelDesc *m_ActiveSelDesc;
public:
    const CodeSelDesc* activateModule(CodeSelector *c, const ModuleExecutionDesc* mod);

    CodeSelectorState();
    virtual ~CodeSelectorState();
    virtual CodeSelectorState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *state);

    bool isSymbolic(uint64_t absolutePc);

    friend class CodeSelector;
};

#endif


}
}


#endif
