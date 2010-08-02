#include "ExecutableFile.h"
#include "BFDInterface.h"

namespace s2etools
{

ExecutableFile::ExecutableFile(const std::string &fileName)
{
    m_fileName = fileName;
}

ExecutableFile::~ExecutableFile()
{

}

ExecutableFile *ExecutableFile::create(const std::string &fileName)
{
    //Try to see if we can open the binary using BFD
    BFDInterface *bfd = new BFDInterface(fileName);
    if (bfd->initialize() && bfd->inited()) {
        return bfd;
    }

    return NULL;

}

}
