#ifndef S2E_DATABASE

#define S2E_DATABASE

#define SQLITE_HAS_CODEC 0
#include "sqlite3.h"

#include <string>

namespace s2e {

class Database
{
private:
    sqlite3 *m_Handle;

    static int callback(void*,int,char**,char**);
public:
    Database(const std::string &fileName);
    ~Database();
    void *getDb() const;
    bool executeQuery(const char *query);
    int getCountOfChanges();
};

} //namespace s2e

#endif
