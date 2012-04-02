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
#define __STDC_FORMAT_MACROS 1


#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <inttypes.h>
#include "BasicBlockListParser.h"

namespace s2etools
{

bool BasicBlockListParser::parseListing(llvm::sys::Path &listingFile, BasicBlocks &blocks)
{
    std::filebuf file;
    if (!file.open(listingFile.c_str(), std::ios::in)) {
        return false;
    }

    std::istream is(&file);

    char line[1024];
    bool hasErrors = false;
    while(is.getline(line, sizeof(line))) {
        //Grab the start and the end
        uint64_t start, end;
        sscanf(line, "0x%"PRIx64" 0x%"PRIx64"", &start, &end);

        //Grab the function name
        std::string fcnName = line;
        fcnName.erase(fcnName.find_last_not_of(" \n\r\t")+1);
        fcnName.erase(0, fcnName.find_last_of(" \t")+1);

        BasicBlock bb(start, end-start+1);
        if (blocks.find(bb) != blocks.end()) {
            std::cerr << "BasicBlockListParser: bb start=0x" << std::hex
                      << bb.start << " size=0x" << bb.size << " overlaps an existing block"<< std::endl;
            hasErrors = true;
            continue;
        }

        bb.function = fcnName;

        blocks.insert(bb);
    }

    return !hasErrors;
}

}
