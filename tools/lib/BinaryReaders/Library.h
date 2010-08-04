#ifndef S2ETOOLS_LIBRARY_H

#define S2ETOOLS_LIBRARY_H

#include "ExecutableFile.h"

#include "lib/ExecutionTracer/ModuleParser.h"
#include <string>
#include <inttypes.h>

namespace s2etools
{

class Library
{
public:
    typedef std::map<std::string, s2etools::ExecutableFile*> ModuleNameToExec;
    typedef std::vector<std::string> PathList;

    Library();
    virtual ~Library();

    bool addLibrary(const std::string &libName);
    bool addLibraryAbs(const std::string &libName);

    ExecutableFile *get(const std::string &name);

    void setPath(const std::string &s);

    bool print(
            const std::string &modName, uint64_t loadBase, uint64_t imageBase,
            uint64_t pc, std::string &out, bool file, bool line, bool func);

    bool print(const ModuleInstance *ni, uint64_t pc, std::string &out, bool file, bool line, bool func);
    bool getInfo(const ModuleInstance *ni, uint64_t pc, std::string &file, uint64_t &line, std::string &func);

    bool findLibrary(const std::string &libName, std::string &abspath);

    static uint64_t translatePid(uint64_t pid, uint64_t pc);
private:
    PathList m_libpath;
    //std::string m_libpath;
    ModuleNameToExec m_libraries;


};

}

#endif
