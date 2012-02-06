=========================
Translation Block Printer
=========================

The fork profiler tool outputs for each specified state the list of translation blocks that were executed in that state.
It complements this list with a test case, if it is available.
This tool is useful to quickly observe the execution sequence that led to a particular event that caused a state to terminate. 
It can also be useful for debugging.

Examples
~~~~~~~~

The following command outputs the translation block trace for paths 0 and 34.
If the ``-printRegisters`` option is specified, it will also print the contents of
the registers before and after the execution of each translation block.
Omitting the ``-pathId`` option will cause the command to output the trace for all paths.
 
  ::

      $ /home/s2e/tools/Release/bin/tbtrace -trace=s2e-last/ExecutionTracer.dat -outputdir=s2e-last/traces \
        -moddir=/home/s2e/experiments/rtl8139.sys/driver -moddir=/home/s2e/experiments/rtl8029.sys/driver \
        -pathId=0 -pathId=34
        

Required Plugins
~~~~~~~~~~~~~~~~

* ModuleExecutionDetector (only the translation blocks of those modules that are configured will be traced)
* ExecutionTracer
* TranslationBlockTracer
* ModuleTracer (for module information)

Optional Plugins
~~~~~~~~~~~~~~~~


* TestCaseGenerator (for test cases)
