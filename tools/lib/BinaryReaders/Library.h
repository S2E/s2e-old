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

    BFDLibrary();
    virtual ~BFDLibrary();

    bool addLibrary(const std::string &libName);
    bool addLibraryAbs(const std::string &libName);

    BFDInterface *get(const std::string &name);

    void setPath(const std::string &s) {
        m_libpath = s;
    }

    bool print(const ModuleInstance *ni, uint64_t pc, std::string &out, bool file, bool line, bool func);

private:
    std::string m_libpath;
    ModuleNameToBfd m_libraries;
};

}

#endif
