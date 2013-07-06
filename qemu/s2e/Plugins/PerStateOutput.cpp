/*
 * S2E Selective Symbolic Execution Platform
 *
 * Copyright (c) 2013, Diego Biurrun <diego@biurrun.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include <sstream>
#include <string>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <s2e/S2E.h>
#include <s2e/Utils.h>

#include "PerStateOutput.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(PerStateOutput,
                  "Split output into per state subdirectories",
                  "PerStateOutput",);

void PerStateOutput::resetOutputDirectory(int state_id)
{
    std::stringstream next_state_stream;
    next_state_stream << state_id;

    llvm::sys::Path output_path(llvm::sys::path::parent_path(s2e()->getOutputDirectoryBase()));
    output_path.appendComponent("state_" + next_state_stream.str());

    std::string output_dir = output_path.str();
    s2e()->setOutputDirectoryBase(output_dir);
    s2e()->initOutputDirectory(output_dir, 0, 1);
}

PerStateOutput::PerStateOutput(S2E* s2e): Plugin(s2e)
{
}

void PerStateOutput::initialize()
{
    llvm::sys::Path output_path(s2e()->getOutputDirectoryBase());
    output_path.appendComponent("state_0");
    std::string output_dir(output_path.str());
    s2e()->setOutputDirectoryBase(output_dir);
    s2e()->initOutputDirectory(output_dir, 0, 1);

    s2e()->getCorePlugin()->onStateSwitch.connect(
        sigc::mem_fun(*this, &PerStateOutput::onStateSwitch));
}

void PerStateOutput::onStateSwitch(S2EExecutionState *current,
                                   S2EExecutionState *next)
{
    resetOutputDirectory(next->getID());
}

} // namespace plugins
} // namespace s2e
