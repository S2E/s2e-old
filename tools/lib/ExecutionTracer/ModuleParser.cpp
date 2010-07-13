//#define __STDC_CONSTANT_MACROS 1
//#define __STDC_LIMIT_MACROS 1
//#define __STDC_FORMAT_MACROS 1


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

    m_Functions.insert(std::pair<std::string, AddressRange>(rangeName, ar));
}

bool Module::getSymbol(std::string &out, uint64_t relPc) const
{
    AddressRange ar(relPc, relPc + 1);
    RangeToNameMap::const_iterator it = m_ObjectNames.find(ar);
    if (it == m_ObjectNames.end()) {
        return false;
    }
    out = (*it).second;
    return true;
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

ModuleLibrary::ModuleLibrary(const std::string &path)
{
    m_Path = path;
}

ModuleLibrary::~ModuleLibrary()
{
    //Delete module descriptors?
}

bool ModuleLibrary::addModule(const Module *m)
{
    assert(m);
    ModuleSet::iterator it = m_Modules.find(m);
    if (it != m_Modules.end()) {
        return false;
    }

    m_Modules.insert(m);
    return true;
}

const Module *ModuleLibrary::get(const std::string &name)
{
    Module m(0,0,name);
    ModuleSet::const_iterator it = m_Modules.find(&m);
    if (it != m_Modules.end()) {
        return *it;
    }

    //Try to load the library from the file
    std::string modFile = m_Path + "/";
    modFile += name;
    modFile += ".fcn";
    Module *mod = ModuleParser::parseTextDescription(modFile);
    if (mod) {
        bool b = addModule(mod);
        assert(b);
    }
    return mod;
}

//XXX: hard-coded for Windows
uint64_t ModuleLibrary::translatePid(uint64_t pid, uint64_t pc) const
{
    if (pc < 0x80000000) {
        return pid;
    }
    return 0;
}

void ModuleLibrary::print(std::ostream &o) const
{
    ModuleSet::iterator it;
    o << "#Available modules in the library:" << std::endl;
    for (it = m_Modules.begin(); it != m_Modules.end(); ++it) {
        const Module *m = (*it);
        const FunctionNameToAddressesMap &fcnMap = m->getFunctionMap();
        o << m->getModuleName() << " - Functions: " << fcnMap.size() << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


ModuleCache::ModuleCache(LogEvents *Events, ModuleLibrary *Library)
{
    m_Library = Library;
    Events->onEachItem.connect(
            sigc::mem_fun(*this, &ModuleCache::onItem)
            );
}

void ModuleCache::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{

    const s2e::plugins::ExecutionTraceModuleLoad &load = *(s2e::plugins::ExecutionTraceModuleLoad*)item;

    if (hdr.type == s2e::plugins::TRACE_MOD_LOAD) {
        if (!loadDriver(load.name, hdr.pid, load.loadBase, load.nativeBase, load.size)) {
            std::cout << "Could not load driver " << load.name << std::endl;
        }
    }else if (hdr.type == s2e::plugins::TRACE_MOD_UNLOAD) {
        std::cerr << "Module unloading not implemented" << std::endl;
    }else if (hdr.type == s2e::plugins::TRACE_PROC_UNLOAD) {
        std::cerr << "Process unloading not implemented" << std::endl;
    }else {
        return;
    }
}

bool ModuleCache::loadDriver(const std::string &name, uint64_t pid, uint64_t loadBase,
                             uint64_t imageBase, uint64_t size)
{
    const Module *m = m_Library->get(name);
    if (!m && !size) {
        return false;
    }

    ModuleInstance *mi;
    if (m) {
        if(m->getImageSize() != size) {
            std::cerr << "Attempt to load " << name << " with mismatching size. Check for name collisions." << std::endl;
            std::cerr << std::hex << m->getImageSize() << " " << size << std::endl << std::flush;
            return false;
        }
        mi = new ModuleInstance(m_Library, pid, loadBase, size, m);
    }else {
        mi = new ModuleInstance(m_Library, name, pid, loadBase, size, imageBase);
    }
    m_Instances.insert(mi);

    return true;
}

bool ModuleCache::unloadDriver(uint64_t pid, uint64_t loadBase)
{
    ModuleInstance mi(m_Library, pid, loadBase, 1, NULL);
    return m_Instances.erase(&mi);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


ModuleInstance::ModuleInstance(
        ModuleLibrary *lib,
        uint64_t pid, uint64_t loadBase, uint64_t size, const Module *m)
{

    //assert(m);
    Pid = lib->translatePid(pid, loadBase);
    LoadBase = loadBase;
    Mod = m;
    Size = 0;
    if (!m) {
        Size = size;
    }

}

ModuleInstance::ModuleInstance(
        ModuleLibrary *lib,
        const std::string &name, uint64_t pid, uint64_t loadBase, uint64_t size, uint64_t imageBase)
{
    Mod = new Module(imageBase, size, name);
    lib->addModule(Mod);

    Pid = lib->translatePid(pid, loadBase);
    LoadBase = loadBase;

    Size = 0;
}

const ModuleInstance *ModuleCache::getInstance(uint64_t pid, uint64_t pc) const
{
    ModuleInstance mi(m_Library, pid, pc, 1, NULL);
    ModuleInstanceSet::const_iterator it = m_Instances.find(&mi);
    if (it == m_Instances.end()) {
        return NULL;
    }

    return (*it);
}

bool ModuleInstance::getSymbol(std::string &out, uint64_t absPc) const
{
    assert(Mod);
    uint64_t relPc = absPc - LoadBase  + Mod->getImageBase();
    return Mod->getSymbol(out, relPc);
}

void ModuleInstance::print(std::ostream &os) const
{
    assert(Mod);
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
