#ifndef S2ETOOLS_TEXTMODULE_H
#define S2ETOOLS_TEXTMODULE_H


#include <string>
#include <ostream>
#include <map>
#include <inttypes.h>

#include "ExecutableFile.h"

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
typedef std::multimap<std::string, AddressRange> FunctionNameToAddressesMap;

class TextModule:public ExecutableFile
{
protected:
    uint64_t m_imageBase;
    uint64_t m_imageSize;
    std::string m_imageName;
    bool m_inited;

    RangeToNameMap m_ObjectNames;
    FunctionNameToAddressesMap m_Functions;

    bool processTextDescHeader(const char *str);
    bool parseTextDescription(const std::string &fileName);

public:
    TextModule(const std::string &fileName);
    virtual ~TextModule();

    virtual bool initialize();
    virtual bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function);
    virtual bool inited() const;

    virtual bool getModuleName(std::string &name ) const;
    virtual uint64_t getImageBase() const;
    virtual uint64_t getImageSize() const;
};


}

#endif
