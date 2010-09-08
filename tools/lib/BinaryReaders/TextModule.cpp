#include "TextModule.h"

#include <stdio.h>
#include <sstream>
#include <cassert>

namespace s2etools {

TextModule::TextModule(const std::string &fileName):ExecutableFile(fileName)
{
    m_inited = false;
    m_imageBase = 0;
    m_imageSize = 0;
}

TextModule::~TextModule()
{

}

bool TextModule::initialize()
{
    if (m_inited) {
        return true;
    }

    if (!parseTextDescription(m_fileName + ".fcn")) {
        return false;
    }

    m_inited = true;
    return true;
}

bool TextModule::getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function)
{
    AddressRange ar;
    ar.start = addr;
    ar.end = addr + 1;

    RangeToNameMap::const_iterator it = m_ObjectNames.find(ar);
    if (it == m_ObjectNames.end()) {
        return false;
    }

    function = (*it).second;

    return true;
}

bool TextModule::inited() const
{
    return m_inited;
}


bool TextModule::getModuleName(std::string &name) const
{
    if (!m_inited) {
        return false;
    }
    name = m_imageName;
    return true;
}

uint64_t TextModule::getImageBase() const
{
    return m_imageBase;
}

uint64_t TextModule::getImageSize() const
{
    return m_imageSize;
}


bool TextModule::processTextDescHeader(const char *str)
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
            m_imageBase = base;
        }

    }else if (type == "#ImageName") {
        std::string s;
        is >> s;
        m_imageName = s;
    }else if (type == "#ImageSize") {
        uint64_t size;
        char c;
        //Skip "0x" prefix
        is >> c >> c >> std::hex >> size;

        if (size) {
            m_imageSize = size;
        }
    }
    return true;
}

bool TextModule::parseTextDescription(const std::string &fileName)
{
    FILE *f = fopen(fileName.c_str(), "r");
    if (!f) {
        return false;
    }


    while(!feof(f)) {
        char buffer[1024];
        std::string fcnName;
        uint64_t start, end;
        fgets(buffer, sizeof(buffer), f);

        if (processTextDescHeader(buffer)) {
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

        AddressRange ar(start, end);
        m_ObjectNames[ar] = fcnName;
        m_Functions.insert(std::make_pair(fcnName, ar));
    }

    fclose(f);
    return true;
}


}
