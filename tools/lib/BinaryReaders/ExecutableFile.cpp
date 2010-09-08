#include "ExecutableFile.h"
#include "BFDInterface.h"
#include "TextModule.h"

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
    delete bfd;

    //Check if there is a text description of the binary
    TextModule *tm = new TextModule(fileName);
    if (tm->initialize() && tm->inited()) {
        return tm;
    }
    delete tm;

    return NULL;

}

}
