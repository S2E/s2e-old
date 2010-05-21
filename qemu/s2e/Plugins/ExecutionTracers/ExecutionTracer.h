#ifndef S2E_PLUGINS_EXECTRACER_H
#define S2E_PLUGINS_EXECTRACER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

struct ExecutionTraceItem{
    uint64_t timeStamp;
    uint8_t  size;  //Size of the payload
    uint8_t  type;
    uint8_t  payload[];
};

/**
 *  This plugin manages the binary execution trace file.
 *  It makes sure that all the writes properly go through it.
 *  Each write is encapsulated in an ExecutionTraceItem before being
 *  written to the file.
 */
class ExecutionTracer : public Plugin
{
    S2E_PLUGIN
public:
    ExecutionTracer(S2E* s2e): Plugin(s2e) {}

    void initialize();

    void writeData(void *data, unsigned size);
private:

};

/**
 *  Base class for all types of tracers.
 *  Handles the basic boilerplate (e.g., common config options).
 */
class EventTracer : public Plugin
{
    S2E_PLUGIN
protected:
    EventTracer(S2E* s2e): Plugin(s2e) {}

    virtual void initialize();

private:

};


class CallRetTracer : public EventTracer
{
public:
    virtual void initialize();
};


enum ExecTraceEntryType {
    TRACE_CALL=0, TRACE_RET,
    TRACE_TB_START,
    TRACE_TB_END,
    TRACE_MODULE_DESC
};

struct ExecTraceModuleDesc {
    char Name[32];
    uint64_t LoadBase;
    uint64_t
};

struct ExecTraceCall {

};

struct ExecTraceRet {

};

struct ExecTraceTbStart {

};

struct ExecTraceTbEnd {

};

struct ExecTraceJump {

};

struct ExecTraceMemory {

};


struct ExecutionTraceEntry {
    ExecTraceEntryType type;
    uint32_t stateId;
};

enum TraceType {
    TRACE_TYPE_TB,
    TRACE_TYPE_INSTR
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
