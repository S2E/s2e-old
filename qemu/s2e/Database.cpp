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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include <iostream>

#include "Database.h"
#include <stdlib.h>

using namespace s2e;

Database::Database(const std::string &fileName)
{
    int ret = sqlite3_open(fileName.c_str(), &m_Handle);
    
    if (!m_Handle) {
        std::cerr << "Could not allocate memory for database" << std::endl;
        exit(-1);
    }
    
    if (ret != SQLITE_OK) {
        std::cerr << "Could not open database " << fileName << std::endl;
        std::cerr << "Error " << sqlite3_errmsg(m_Handle) << std::endl;
        sqlite3_close(m_Handle);
        exit(-1);
    }
}

Database::~Database()
{
    sqlite3_close(m_Handle);
}

void *Database::getDb() const
{
    return m_Handle;
}

int Database::callback(void *db,int nbColumns,char**,char**)
{
    return 0;
}

//Returns the number of rows that where changed by the
//last insert, delete, or update operation.
int Database::getCountOfChanges()
{
    return sqlite3_changes(m_Handle);
}

bool Database::executeQuery(const char *query)
{
    int res;
    char *errMsg = NULL;

    res = sqlite3_exec(m_Handle,
        query,
        &Database::callback,
        this,
        &errMsg);

    if (res != SQLITE_OK) {
        std::cerr << "Error executing query " << query << std::endl;
        std::cerr << errMsg << std::endl;
    }

    if (errMsg) {
        sqlite3_free(errMsg);
    }

    return res == SQLITE_OK;
}
