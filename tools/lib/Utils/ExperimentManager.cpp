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

#include <llvm/System/Path.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <sys/stat.h>
#include <errno.h>

#include "ExperimentManager.h"
#include "Log.h"

namespace s2etools {

LogKey ExperimentManager::TAG = LogKey("ExperimentManager");

ExperimentManager::ExperimentManager(const std::string &outputFolder)
{
    m_outputFolder = outputFolder;
    m_autoIncrement = false;

    if (!initializeOutputDirectory()) {
        exit(-1);
    }
}

ExperimentManager::ExperimentManager(const std::string &outputFolder, const std::string prefix)
{
    m_outputFolder = outputFolder;
    m_prefix = prefix;
    m_autoIncrement = true;

    if (!initializeOutputDirectory()) {
        exit(-1);
    }
}

ExperimentManager::~ExperimentManager()
{

}

bool ExperimentManager::initializeOutputDirectory()
{
    llvm::sys::Path cwd(".");
    cwd.makeAbsolute();

    llvm::sys::Path outputDirectory(m_outputFolder);

    if (m_autoIncrement) {
        for (int i = 0; ; i++) {
            std::stringstream str;
            str <<m_prefix << "-out-" << i;

            llvm::sys::Path dirPath(cwd);
            dirPath.appendComponent(str.str());

            if(!dirPath.exists()) {
                outputDirectory = dirPath;
                break;
            }
        }
    }

    LOGINFO("X2L: output directory = \"" << outputDirectory.toString() << "\"" << std::endl);


    outputDirectory.makeAbsolute();

    if (outputDirectory != cwd) {
        if (outputDirectory.createDirectoryOnDisk()) {
            LOGERROR("Unable to create output directory" << std::endl);
            return false;
        }
    }

    if (m_autoIncrement) {
#ifndef _WIN32
        llvm::sys::Path s2eLast(m_outputFolder);
        s2eLast.appendComponent(m_prefix + "-last");

        if ((unlink(s2eLast.c_str()) < 0) && (errno != ENOENT)) {
            LOGERROR("Cannot unlink " << m_prefix << "-last" << std::endl);
            return false;
        }

        if (symlink(outputDirectory.c_str(), s2eLast.c_str()) < 0) {
            LOGERROR("Cannot make symlink " << m_prefix << "-last" << std::endl);
            return false;
        }
#endif
    }

    m_outputFolder = outputDirectory.toString();
    return true;
}

std::ostream *ExperimentManager::getOuputFile(const std::string &fileName)
{
    std::ios::openmode io_mode = std::ios::out | std::ios::trunc | std::ios::binary;

    llvm::sys::Path path(m_outputFolder);
    path.appendComponent(fileName);

    return new std::ofstream(path.c_str(), io_mode);
}

}
