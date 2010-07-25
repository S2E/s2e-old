#ifndef S2ETOOLS_LIBRARY_H

#define S2ETOOLS_LIBRARY_H

#include "BFDInterface.h"

#include "lib/ExecutionTracer/ModuleParser.h"
#include <string>
#include <inttypes.h>

namespace s2etools
{

class BFDLibrary
{
public:
    typedef std::map<std::string, s2etools::BFDInterface*> ModuleNameToBfd;
    typedef std::vector<std::string> PathList;

    BFDLibrary();
    virtual ~BFDLibrary();

    bool addLibrary(const std::string &libName);
    bool addLibraryAbs(const std::string &libName);

    BFDInterface *get(const std::string &name);

    void setPath(const std::string &s);

    bool print(
            const std::string &modName, uint64_t loadBase, uint64_t imageBase,
            uint64_t pc, std::string &out, bool file, bool line, bool func);

    bool print(const ModuleInstance *ni, uint64_t pc, std::string &out, bool file, bool line, bool func);

    bool findLibrary(const std::string &libName, std::string &abspath);
private:
    PathList m_libpath;
    //std::string m_libpath;
    ModuleNameToBfd m_libraries;


};

}

#endif
