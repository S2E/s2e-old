#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1


#include "ModuleParser.h"

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <sstream>
#include <iostream>

namespace s2etools {

Module::Module()
{
    m_ImageBase = 0;
    m_ImageSize = 0;
    m_ModuleName = "";
}

Module::Module(uint64_t Base, uint64_t Size, const std::string &name)
{
    m_ImageBase = Base;
    m_ImageSize = Size;
    m_ModuleName = name;
}

void Module::addRange(uint64_t start, uint64_t end, const std::string &rangeName)
{
    AddressRange ar(start, end);
    m_ObjectNames[ar] = rangeName;
}


void Module::print(std::ostream &os) const
{
    os << "Module: " << m_ModuleName << std::hex <<
            " ImageBase=0x" << m_ImageBase <<
            " ImageSize=0x" << m_ImageSize << std::endl;

    RangeToNameMap::const_iterator it;
    for (it = m_ObjectNames.begin(); it != m_ObjectNames.end(); ++it) {
        (*it).first.print(os);
        os << " " << (*it).second << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ModuleLibrary::ModuleLibrary()
{

}

ModuleLibrary::~ModuleLibrary()
{
    //Delete module descriptors?
}

bool ModuleLibrary::addModule(const Module *m)
{
    ModuleSet::iterator it = m_Modules.find(m);
    if (it != m_Modules.end()) {
        return false;
    }

    m_Modules.insert(m);
    return true;
}

const Module *ModuleLibrary::get(const std::string &name) const
{
    Module m(0,0,name);
    ModuleSet::const_iterator it = m_Modules.find(&m);
    if (it != m_Modules.end()) {
        return *it;
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


ModuleCache::ModuleCache(const ModuleLibrary *Library)
{
    m_Library = Library;
}

bool ModuleCache::loadDriver(const std::string &name, uint64_t pid, uint64_t loadBase)
{
    const Module *m = m_Library->get(name);
    if (!m) {
        return false;
    }

    ModuleInstance *mi = new ModuleInstance(pid, loadBase, m);
    m_Instances.insert(mi);

    return true;
}

bool ModuleCache::unloadDriver(uint64_t pid, uint64_t loadBase)
{
    ModuleInstance mi(pid, loadBase, NULL);
    return m_Instances.erase(&mi);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


const ModuleInstance *ModuleCache::getInstance(uint64_t pid, uint64_t pc) const
{
    ModuleInstance mi(pid, pc, NULL);
    ModuleInstanceSet::const_iterator it = m_Instances.find(&mi);
    if (it == m_Instances.end()) {
        return NULL;
    }

    return (*it);
}

void ModuleInstance::print(std::ostream &os) const
{
    os << "Instance of " << Mod->getModuleName() <<
            " Pid=0x" << std::hex << Pid <<
            " LoadBase=0x" << LoadBase << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


bool ModuleParser::processTextDescHeader(Module *m, const char *str)
{
    if (str[0] != '#') {
        return false;
    }

    std::istringstream is(str);

    std::string type;
    is >> type;

    if (type == "#ImageBase") {
        uint64_t base;
        char c;
        //Skip "0x" prefix
        is >> c >> c >> std::hex >> base;

        if (base) {
            m->setImageBase(base);
        }
    }else if (type == "#ImageName") {
        std::string s;
        is >> s;
        m->setModuleName(s);
    }else if (type == "#ImageSize") {
        uint64_t size;
        char c;
        //Skip "0x" prefix
        is >> c >> c >> std::hex >> size;

        if (size) {
            m->setImageSize(size);
        }
    }
    return true;
}

Module* ModuleParser::parseTextDescription(const std::string &fileName)
{
    FILE *f = fopen(fileName.c_str(), "r");
    if (!f) {
        return NULL;
    }

    Module *ret = new Module(0,0,"");

    while(!feof(f)) {
        char buffer[1024];
        std::string fcnName;
        uint64_t start, end;
        fgets(buffer, sizeof(buffer), f);

        if (processTextDescHeader(ret, buffer)) {
            continue;
        }

        std::istringstream is(buffer);
        char c;
        is >> c >> c >> std::hex >> start >> c >> c >>
                end;

        is >> std::noskipws >> c;
        while(is >> c) {
            if (c=='\r' || c=='\n') {
                break;
            }
            fcnName+=c;
        }

        //sscanf(buffer, "0x%"PRIx64" 0x%"PRIx64" %[^\n]s\n", &start, &end, fcnName);

        ret->addRange(start, end, fcnName);
    }

    fclose(f);
    return ret;
}

}
