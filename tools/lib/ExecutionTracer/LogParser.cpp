#include <iostream>
#include <cassert>
#include "LogParser.h"

using namespace s2e::plugins;

namespace s2etools
{

void LogEvents::processItem(unsigned currentItem,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         void *data)
{
    const ExecutionTraceAll *item = (ExecutionTraceAll*)data;

    onEachItem.emit(currentItem, hdr, (void*)data);


    assert(hdr.type < TRACE_MAX);
    switch(hdr.type) {
        case TRACE_MOD_LOAD:
            onModuleLoadItem.emit(currentItem, hdr, (*item).moduleLoad);
            break;

        case TRACE_MOD_UNLOAD:
            onModuleUnloadItem.emit(currentItem, hdr, (*item).moduleUnload);
            break;

        case TRACE_CALL:
            onCallItem.emit(currentItem, hdr, (*item).call);
            break;

        case TRACE_RET:
            onReturnItem.emit(currentItem, hdr, (*item).ret);
            break;

        case TRACE_TB_START:

        case TRACE_TB_END:

        case TRACE_MODULE_DESC:

        default:
            
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

LogParser::LogParser()
{
    m_File = NULL;
}

LogParser::~LogParser()
{
    if (m_File) {
        fclose(m_File);
    }
}



bool LogParser::parse(const std::string &fileName)
{
    FILE *file = fopen(fileName.c_str(), "rb");
    if (!file) {
        std::cerr << "LogParser: Could not open " << fileName << std::endl;
        return false;
    }
    m_File = file;

    m_ItemOffsets.clear();

    uint64_t currentOffset = 0;
    unsigned currentItem = 0;

    while(!feof(file)) {
        s2e::plugins::ExecutionTraceItemHeader hdr;
        if (!fread(&hdr, sizeof(hdr), 1, file)) {
            std::cerr << "LogParser: Could not read header " << std::endl;
            //fclose(file);
            return false;
        }

        uint8_t buffer[256];

        if (!fread(&buffer, hdr.size, 1, file)) {
            std::cerr << "LogParser: Could not read payload " << std::endl;
            //fclose(file);
            return false;
        }

        m_ItemOffsets.push_back(currentOffset);
        currentOffset += sizeof(hdr)  + hdr.size;

        processItem(currentItem, hdr, buffer);

        ++currentItem;
    }

    //fclose(file);
    return true;
}

bool LogParser::getItem(unsigned index, s2e::plugins::ExecutionTraceItemHeader &hdr, void *data)
{
    if (!m_File) {
        assert(false);
        return false;
    }

    if (index >= m_ItemOffsets.size() ) {
        assert(false);
        return false;
    }

    if (fseek(m_File, m_ItemOffsets[index], SEEK_SET)) {
        std::cerr << "LogParser: Could not seek to  " << m_ItemOffsets[index] << std::endl;
        assert(false);
        return false;
    }

    if (!fread(&hdr, sizeof(s2e::plugins::ExecutionTraceItemHeader), 1, m_File)) {
        std::cerr << "LogParser: Could not read header " << std::endl;
        assert(false);
        return false;
    }

    if (!fread(data, hdr.size, 1, m_File)) {
        std::cerr << "LogParser: Could not read payload " << std::endl;
        assert(false);
        return false;
    }

    return true;
}

}
