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

#include <iostream>
#include <fstream>
#include <llvm/Support/CommandLine.h>

#include "Log.h"

using namespace llvm;

namespace {
cl::opt<std::string>
    LogFile("logfile", cl::desc("Where to write log output. If not specified, written to stderr."));

cl::opt<int>
        LogLevel("loglevel", cl::desc("Logging verbosity"), cl::init(LOG_WARNING));

cl::opt<bool>
        LogAll("logall", cl::desc("Logging verbosity"), cl::init(true));

cl::list<std::string>
        LogItems("log", llvm::cl::value_desc("log-item"), llvm::cl::Prefix, llvm::cl::desc("Item to log"));

cl::list<std::string>
        NoLogItems("nolog", llvm::cl::value_desc("nolog-item"), llvm::cl::Prefix, llvm::cl::desc("Disable log for this item"));
}

namespace s2etools {


struct nullstream:std::ostream {
    nullstream(): std::ios(0), std::ostream(0) {}
};

static nullstream s_null;
static std::ofstream s_logfile;
bool Logger::s_inited = false;
Logger::TrackedKeys* Logger::s_trackedKeysFast = NULL;
Logger::KeyToString* Logger::s_trackedStrings = NULL;
Logger::StringToKey* Logger::s_trackedKeys = NULL;
unsigned Logger::s_currentKey = 0;

void Logger::Initialize()
{
    if (s_inited) {
        return;
    }

    AllocateStructs();

    //First check whether we need to log everything
    if (LogAll) {
        foreach(it, s_trackedKeys->begin(), s_trackedKeys->end()) {
            s_trackedKeysFast->insert((*it).second);
        }
    }

    //Add all extra items
    foreach(it, LogItems.begin(), LogItems.end()) {
        if (s_trackedKeys->find(*it) != s_trackedKeys->end()) {
            s_trackedKeysFast->insert((*s_trackedKeys)[*it]);
        }
    }

    //No check the items that we don't want to log
    foreach(it, NoLogItems.begin(), NoLogItems.end()) {
        if (s_trackedKeys->find(*it) != s_trackedKeys->end()) {
            s_trackedKeysFast->erase((*s_trackedKeys)[*it]);
        }
    }

    if (LogFile.size() > 0) {
        s_logfile.open(LogFile.c_str(), std::ios::binary);
    }
    s_inited = true;
}

void Logger::AllocateStructs()
{
    if (s_trackedKeys) {
        return;
    }

    s_trackedKeys = new StringToKey();
    s_trackedKeysFast = new TrackedKeys();
    s_trackedStrings = new KeyToString();
}

unsigned Logger::Key(const std::string &s)
{
    AllocateStructs();

    StringToKey::iterator it = s_trackedKeys->find(s);
    if (it == s_trackedKeys->end()) {
        unsigned ret;

        (*s_trackedKeys)[s] = s_currentKey;
        (*s_trackedStrings)[s_currentKey] = s;
        ret = s_currentKey++;
        return ret;
    }else {
        return (*it).second;
    }
}

LogKey::LogKey(const std::string &tag)
{
    m_key = Logger::Key(tag);
    m_tag = tag;
}

int DoLog(int logLevel, const LogKey &k)
{
    Logger::Initialize();
    if (!k.isTracked()) {
        return 0;
    }

    if (logLevel < LogLevel) {
        return 0;
    }
    return 1;
}

std::ostream& Log(int logLevel, const LogKey &k)
{
    Logger::Initialize();
    if (!k.isTracked()) {
        return s_null;
    }

    if (logLevel < LogLevel) {
        return s_null;
    }

    if (LogFile.size() == 0) {
        return std::cerr;
    }

    return s_logfile;
}

}
