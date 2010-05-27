#ifndef S2ETOOLS_BFDINTERFACE_H
#define S2ETOOLS_BFDINTERFACE_H

extern "C" {
#include <bfd.h>
}

#include <string>
#include <map>
#include <inttypes.h>

namespace s2etools
{

struct BFDSection
{
    uint64_t start, size;

    bool operator < (const BFDSection &s) const {
        return start + size < s.start;
    }
};

class BFDInterface
{
private:
    typedef std::map<BFDSection, asection *> Sections;

    static bool s_bfdInited;
    bfd *m_bfd;
    asymbol **m_symbolTable;
    std::string m_fileName;
    Sections m_sections;


    bool initialize();

    static void initSections(bfd *abfd, asection *sect, void *obj);

public:
    BFDInterface(const std::string &fileName);
    ~BFDInterface();

    bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);

};

}

#endif
