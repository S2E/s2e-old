#ifndef S2ETOOLS_EXECTRACER_MODULEPARSER_H
#define S2ETOOLS_EXECTRACER_MODULEPARSER_H

#include <string>
#include <map>
#include <set>
#include <inttypes.h>
#include <ostream>

namespace s2etools
{

struct AddressRange
{
    uint64_t start, end;

    AddressRange() {
        start = 0;
        end = 0;
    }

    AddressRange(uint64_t s, uint64_t e) {
        start = s;
        end = e;
    }


    bool operator()(const AddressRange &p1, const AddressRange &p2) const {
        return p1.end < p2.start;
    }

    bool operator<(const AddressRange &p) const {
        return end < p.start;
    }

    void print(std::ostream &os) const {
        os << "Start=0x" << std::hex << start <<
              " End=0x" << end;
    }

};

typedef std::map<AddressRange, std::string> RangeToNameMap;

class Module
{
private:
    std::string m_ModuleName;
    RangeToNameMap m_ObjectNames;
    uint64_t m_ImageBase;
    uint64_t m_ImageSize;
public:

    Module();
    Module(uint64_t Base, uint64_t Size, const std::string &name);

    void addRange(uint64_t start, uint64_t end, const std::string &rangeName);

    void setImageBase(uint64_t base) {
        m_ImageBase = base;
    }

    void setImageSize(uint64_t size) {
        m_ImageSize = size;
    }

    uint64_t getImageBase() const {
        return m_ImageBase;
    }

    uint64_t getImageSize() const {
        return m_ImageSize;
    }

    const std::string &getModuleName() const {
        return m_ModuleName;
    }

    void setModuleName(const std::string &s) {
        m_ModuleName = s;
    }

    void print(std::ostream &os) const;

    bool operator<(const Module &m) const {
        return m_ModuleName < m.m_ModuleName;
    }


};

struct ModuleCmpName {
    bool operator()(const Module *m1, const Module *m2) const {
        return m1->getModuleName() < m2->getModuleName();
    }
};

typedef std::set<const Module*, ModuleCmpName> ModuleSet;

//Collection of module descriptors.
//It is static and populated from module files
class ModuleLibrary
{
private:
    ModuleSet m_Modules;
public:
    ModuleLibrary();
    ~ModuleLibrary();
    bool addModule(const Module *m);
    const Module *get(const std::string &name) const;
};

struct ModuleInstance
{
    uint64_t Pid;
    uint64_t LoadBase;
    const Module *Mod;

    ModuleInstance(uint64_t pid, uint64_t base, const Module *m)  {
        Pid = pid;
        LoadBase = base;
        Mod = m;
    }

    bool operator<(const ModuleInstance& s) const {
        if (Pid == s.Pid) {
            return LoadBase + Mod->getImageSize() < s.LoadBase;
        }
        return Pid < s.Pid;
    }

    void print(std::ostream &os) const;

};

struct ModuleInstanceCmp {
    bool operator()(const ModuleInstance *s1, const ModuleInstance *s2) const {
        if (s1->Pid == s2->Pid) {
            return s1->LoadBase + s1->Mod->getImageSize() < s2->LoadBase;
        }
        return s1->Pid < s2->Pid;
    }
};

typedef std::set<ModuleInstance*, ModuleInstanceCmp> ModuleInstanceSet;

//Represents all the loaded modules at a given time
class ModuleCache
{
private:
    const ModuleLibrary *m_Library;
    ModuleInstanceSet m_Instances;

public:
    ModuleCache(const ModuleLibrary *Library);
    bool loadDriver(const std::string &name, uint64_t pid, uint64_t loadBase);
    bool unloadDriver(uint64_t pid, uint64_t loadBase);

    const ModuleInstance *getInstance(uint64_t pid, uint64_t pc) const;

};

/**
 *  Provides a collection of functions to parse modules files.
 *  For now, only provides support for text files that encode
 *  the function addresses in a file.
 */
class ModuleParser
{
private:
    static bool processTextDescHeader(Module *m, const char *str);
public:
    static Module* parseTextDescription(const std::string &fileName);
};


}

#endif
