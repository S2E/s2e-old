#ifndef S2ETOOLS_EXECUTABLEFILE_H
#define S2ETOOLS_EXECUTABLEFILE_H


#include <string>
#include <inttypes.h>

namespace s2etools
{

class ExecutableFile
{
protected:
    std::string m_fileName;

public:
    ExecutableFile(const std::string &fileName);
    virtual ~ExecutableFile();

    virtual bool initialize() = 0;
    virtual bool getInfo(uint64_t addr, std::string &source, uint64_t &line, std::string &function) = 0;
    virtual bool inited() const = 0;

    static ExecutableFile *create(const std::string &fileName);

    virtual bool getModuleName(std::string &name ) const = 0;
    virtual uint64_t getImageBase() const  = 0;
    virtual uint64_t getImageSize() const  = 0;
};


}

#endif
