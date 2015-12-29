/*
 * ForkController.h
 *
 *  Created on: 2015.12.24
 *      Author: Epeius
 */

#ifndef FORKCONTROLLER_H_
#define FORKCONTROLLER_H_
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
namespace s2e {
namespace plugins {

class ForkController : public Plugin
{
    S2E_PLUGIN

    ModuleExecutionDetector *m_detector;
    RangeEntries m_forkRanges;

public:
	virtual ~ForkController();
	ForkController(S2E* s2e): Plugin(s2e) {
		m_detector = NULL;
	}
    void initialize();

public:
    void slotExecuteBlockStart(S2EExecutionState* state, uint64_t pc);
    void slotExecuteBlockEnd(S2EExecutionState* state, uint64_t pc);

      void onTranslateBlockStart(ExecutionSignal*,
              S2EExecutionState*,
              TranslationBlock*,
              uint64_t );
      void onTranslateBlockEnd(
              ExecutionSignal *signal,
              S2EExecutionState* state,
              TranslationBlock *tb,
              uint64_t endPc,
              bool staticTarget,
              uint64_t targetPc);
};

} // namespace plugins
} // namespace s2e

#endif /* FORKCONTROLLER_H_ */
