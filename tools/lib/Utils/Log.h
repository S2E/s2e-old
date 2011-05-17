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

#ifndef S2ETOOLS_LOG_H
#define S2ETOOLS_LOG_H

#include <ostream>
#include <string>
#include <llvm/ADT/DenseSet.h>
#include <map>

#include "Utils.h"

#define LOG_DEBUG    0
#define LOG_INFO     1
#define LOG_WARNING  3
#define LOG_ERROR    4

namespace s2etools {

class LogKey;

class Logger {
friend class LogKey;

public:
    typedef llvm::DenseSet<unsigned> TrackedKeys;
    typedef std::map<unsigned, std::string> KeyToString;
    typedef std::map<std::string, unsigned> StringToKey;

private:
    Logger();
    static bool s_inited;
    static unsigned s_currentKey;

    //These have to be pointers because of static intializing issues
    static TrackedKeys *s_trackedKeysFast;
    static KeyToString *s_trackedStrings;
    static StringToKey *s_trackedKeys;

    static void AllocateStructs();

public:
    static void Initialize();
    static unsigned Key(const std::string &s);
};

class LogKey {
private:
    unsigned m_key;
    std::string m_tag;
public:
    LogKey(const std::string &tag);
    inline bool isTracked() const {
        return Logger::s_trackedKeysFast->count(m_key);
    }
    inline const std::string &getTag() const {
        return m_tag;
    }
};

/** Get the logging stream */
std::ostream& Log(int logLevel, const LogKey &k);

#define __LOG_SUFFIX(level) s2etools::Log(level, TAG) << std::dec << '[' << level << "] " << \
TAG.getTag() << ":" << __FUNCTION__ << " - "

#define LOGDEBUG() __LOG_SUFFIX(LOG_DEBUG)
#define LOGWARNING() __LOG_SUFFIX(LOG_WARNING)
#define LOGINFO() __LOG_SUFFIX(LOG_INFO)
#define LOGERROR() __LOG_SUFFIX(LOG_ERROR)

}

#endif
