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
