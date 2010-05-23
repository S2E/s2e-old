#include <iostream>
#include "LogParser.h"

using namespace s2e::plugins;

namespace s2etools
{

LogParser::LogParser()
{

}

LogParser::~LogParser()
{

}

bool LogParser::parse(const std::string &fileName)
{
    FILE *file = fopen(fileName.c_str(), "rb");
    if (!file) {
        std::cerr << "LogParser: Could not open " << fileName << std::endl;
        return false;
    }

    m_ItemOffsets.clear();

    uint64_t currentOffset = 0;
    unsigned currentItem = 0;

    while(!feof(file)) {
        s2e::plugins::ExecutionTraceItemHeader hdr;
        if (!fread(&hdr, sizeof(hdr), 1, file)) {
            std::cerr << "LogParser: Could not read header " << std::endl;
            fclose(file);
            return false;
        }

        uint8_t buffer[256];
        const ExecutionTraceAll *item = (ExecutionTraceAll*)buffer;

        if (!fread(&buffer, hdr.size, 1, file)) {
            std::cerr << "LogParser: Could not read payload " << std::endl;
            fclose(file);
            return false;
        }

        m_ItemOffsets.push_back(currentOffset);
        currentOffset += sizeof(hdr)  + hdr.size;

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
                std::cerr << "LogParser: WARNING: unsupported item type " << hdr.type <<
                        " at offset " << std::dec << currentOffset << std::endl;
                break;
        }

        ++currentItem;
    }

    fclose(file);
    return true;
}

}
