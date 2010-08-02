#ifndef S2ETOOLS_BFDINTERFACE_H
#define S2ETOOLS_BFDINTERFACE_H

extern "C" {
#include <bfd.h>
}

#include <string>
#include <map>
#include <set>
#include <inttypes.h>

#include "ExecutableFile.h"

namespace s2etools
{


struct BFDSection
{
    uint64_t start, size;

    bool operator < (const BFDSection &s) const {
        return start + size < s.start;
    }
};

class BFDInterface : public ExecutableFile
{
private:
    typedef std::map<BFDSection, asection *> Sections;
    typedef std::set<uint64_t> AddressSet;

    static bool s_bfdInited;
    bfd *m_bfd;
    asymbol **m_symbolTable;
    std::string m_moduleName;
    Sections m_sections;
    AddressSet m_invalidAddresses;

    uint64_t m_imageBase;

    static void initSections(bfd *abfd, asection *sect, void *obj);

public:
    BFDInterface(const std::string &fileName);
    virtual ~BFDInterface();

    bool initialize();
    bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);
    bool inited() const {
        return m_bfd != NULL;
    }

    virtual bool getModuleName(std::string &name ) const;
    virtual uint64_t getImageBase() const;
    virtual uint64_t getImageSize() const;
};

}

#endif
