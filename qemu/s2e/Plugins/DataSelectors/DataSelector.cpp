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

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <sstream>  
#include "DataSelector.h"

using namespace s2e;
using namespace plugins;
using namespace klee;

void DataSelector::initialize()
{
    //Check that the interceptor is there
    m_ExecDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_ExecDetector);

  
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

#if 0
    if (Sections.size() > 1) {
        s2e()->getWarningsStream() << "Only one service can be handled currently..." << std::endl;
        exit(-1);
    }
#endif

    foreach2(it, Sections.begin(), Sections.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << '\n';
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the sections\n";
        exit(-1);
    }

}

ref<Expr> DataSelector::getNonNullCharacter(S2EExecutionState *s, Expr::Width w)
{
    ref<Expr> symbVal = s->createSymbolicValue("", w);
    return NeExpr::create(symbVal, ConstantExpr::create(0,w));
}

ref<Expr> DataSelector::getUpperBound(S2EExecutionState *s, uint64_t upperBound, Expr::Width w)
{
    ref<Expr> symbVal = s->createSymbolicValue("", w);
    return UltExpr::create(symbVal, ConstantExpr::create(upperBound,w));
}

bool DataSelector::makeUnicodeStringSymbolic(S2EExecutionState *s, uint64_t address)
{
    do {
        uint16_t car;
        SREADR(s,address,car);

        if (!car) {
            return true;
        }

        ref<Expr> v = getNonNullCharacter(s, Expr::Int16);
        s2e()->getMessagesStream() << v << '\n';
        s->writeMemory(address, v); 
        address+=sizeof(car);
    }while(1);
    
    return true;
}

bool DataSelector::makeStringSymbolic(S2EExecutionState *s, uint64_t address)
{
    do {
        uint8_t car;
        SREADR(s,address,car);

        if (!car) {
            return true;
        }

        ref<Expr> v = getNonNullCharacter(s, Expr::Int8);
        s2e()->getMessagesStream() << v << '\n';
        s->writeMemory(address, v); 
        address+=sizeof(car);
    }while(1);
    
    return true;
}

klee::ref<klee::Expr> DataSelector::getOddValue(S2EExecutionState *s, klee::Expr::Width w)
{
    ref<Expr> symbVal = s->createSymbolicValue("", w);
    ref<Expr> e1 = MulExpr::create(ConstantExpr::create(2,w), symbVal);
    ref<Expr> e2 = AddExpr::create(e1, ConstantExpr::create(1,w));
    return e2;
}

klee::ref<klee::Expr> DataSelector::getOddValue(S2EExecutionState *s, klee::Expr::Width w, uint64_t upperBound)
{
    ref<Expr> e1 = getOddValue(s, w);
    return UltExpr::create(e1, ConstantExpr::create(upperBound,w));
}
