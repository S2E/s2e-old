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

#ifndef S2ETOOLS_TEXTMODULE_H
#define S2ETOOLS_TEXTMODULE_H


#include <string>
#include <ostream>
#include <map>
#include <inttypes.h>

#include "ExecutableFile.h"

namespace s2etools
{

struct AddressRange
{
    uint64_t start, end;

    AddressRange() {
        start = 0;
        end = 0;
    }

    AddressRange(uint64_t s, uint64_t e) {
        start = s;
        end = e;
    }


    bool operator()(const AddressRange &p1, const AddressRange &p2) const {
        return p1.end < p2.start;
    }

    bool operator<(const AddressRange &p) const {
        return end < p.start;
    }

    void print(std::ostream &os) const {
        os << "Start=0x" << std::hex << start <<
              " End=0x" << end;
    }

};

typedef std::map<AddressRange, std::string> RangeToNameMap;
typedef std::multimap<std::string, AddressRange> FunctionNameToAddressesMap;

class TextModule:public ExecutableFile
{
protected:
    uint64_t m_imageBase;
    uint64_t m_imageSize;
    std::string m_imageName;
    bool m_inited;

    RangeToNameMap m_ObjectNames;
    FunctionNameToAddressesMap m_Functions;

    bool processTextDescHeader(const char *str);
    bool parseTextDescription(const std::string &fileName);

public:
    TextModule(const std::string &fileName);
    virtual ~TextModule();

    virtual bool initialize();
    virtual bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);
    virtual bool inited() const;

    virtual bool getModuleName(std::string &name ) const;
    virtual uint64_t getImageBase() const;
    virtual uint64_t getImageSize() const;
};


}

#endif
