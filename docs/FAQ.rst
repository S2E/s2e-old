======================================
Frequently Asked Questions (FAQ)
======================================

.. contents::
.. sectnum::

S2E
===


How do I know what S2E is doing?
--------------------------------

1. Enable execution tracing and use the fork profiler to identify the code locations that fork the most.
   In your LUA file, enable the ``ExecutionTracer``, ``ModuleTracer`` and the ``ModuleExecutionDetector`` plugins.
   This will allow you to collect all fork locations. Additionally, you can use ``TranslationBlockTracer``  in order to
   have a detailed trace for each execution path, which you can view with the ``tbtrace`` tool. Finally, ``TranslationBlockTracer``
   allows you to use the basic block `coverage <Tools/CoverageGenerator.html>`_ tool.

2. Look at ``s2e-last/debug.txt`` and other files.
   These files list all the major events occurring during symbolic execution.

   S2E outputs "Firing timer event" into ``s2e-last/debug.txt`` once per second.
   If you do not see this event every second,
   it means that QEMU is stuck running plugin code (most likely because of a plugin bug)
   or constraint solver code (because of a complex query).
   To see which query is causing the problem, look at the query log.

3. ``run.stats`` contains many types of statistics. S2E updates this file about every second,
   when executing symbolic code. See later in this FAQ for a description of its fields.



Execution seems stuck/slow. What to do?
---------------------------------------

First, ensure that you configured S2E properly.

* If you used ``s2e_disable_all_apic_interrupts``, you probably forgot an ``s2e_enable_all_apic_interrupts`` call somewhere in your code.
  Use this functionality with care, disabling interrupts can easily hang your guest OS.

* Some constraints are hard to solve. Set a timeout in the constraint solver with ``--use-forked-stp`` and ``--max-stp-time=TimeoutInSeconds``.
  If you do not see the "Firing timer event" message periodically in the ``debug.txt`` log file, execution got stuck in the
  constraint solver.

* By default, S2E flushes the translation block cache on every state switch.
  S2E does not implement copy-on-write for this cache, therefore it must flush
  the cache to ensure correct execution. Flushing avoids clobbering in case
  there are two paths that execute different pieces of code loaded at the same memory locations.
  Flushing is *very* expensive in case of frequent state switches. In most of the cases, flushing is not necessary, e.g., if you
  execute a program that does not use self-modifying code or frequently loads/unloads libraries. In this case,
  use the ``--flush-tbs-on-state-switch=false`` option.

* Make sure your VM image is minimal for the components you want to test. In most cases, it should not have swap enabled
  and all unnecessary background deamons should be disabled. Refer to the `image installation <ImageInstallation.html>`_ tutorial for
  more information.


Second, throw hardware at your problem

* Refer to the "`How to run S2E on multiple cores <Howtos/Parallel.html>`_" tutorial for instructions.

Third, use S2E to *selectively* relax and/or overconstrain path constraints.

* Understanding what to select can be made considerably easier if you `attach a debugger <Howtos/Debugging.html>`_ to the S2E instance.

* Check that the module under analysis is not doing unnecessary calls with symbolic arguments (e.g., ``printf``).
  Use the ``s2e_get_example_*`` functions to provide a concrete value to ``printf``  without actually adding path
  constraints, to avoid disabling future paths. Unless a program reads the output of ``printf`` and takes decisions
  based on it, not adding constraints will not affect execution consistency from the point of view of the module under analysis.

* If you use a depth-first search and execution hits a polling loop, rapid forking may occur and execution may never exit the loop.
  Moreover, depending on the accumulated constraints, each iteration may be slower and slower.
  Try to use a different search strategy or kill unwanted execution paths.

* Try to relax path constraints. For example, there may be a branch that causes a bottleneck. Use the *Annotation* plugin to intercept
  that branch instruction and overwrite the branch condition with an unconstrained value. This trades execution consistency
  for execution speed. Unconstraining execution may create paths that cannot occur in real executions (i.e., false positives), but as long as there
  are few of them, or you can detect them a posteriori, this is an acceptable trade-off.


How do I deal with path explosion?
-----------------------------------

Use S2E to *selectively* kill paths that are not interesting and prevent forking outside modules of interest.
The following describes concrete steps that allowed us to explore programs most efficiently.

1. Run your program with minimum symbolic input (e.g., 1 byte) and with tracing enabled (see first section).

2. Insert more and more symbolic values until path explosion occurs (i.e., it takes too long for you to explore all the paths
   or it takes too much memory/CPU resources).

3. Extract the fork profile and identify the code locations that fork the most.

4. If forking occurs outside the module of interest, the following may help:

   * Use the CodeSelector plugin to disable forking when execution leaves the module of interest
   * Concretize some symbolic values when execution leaves the module of interest. You may need to use the ``FunctionMonitor`` plugin
     to track function calls and concretize parameters.
   * Provide example values to library functions (e.g., to ``printf``, as described previously)

5. Kill the paths that you are not interested in:

   * You may only want to explore error-free paths. For example, kill all those where library functions fail.
   * You may only be interested in error recovery code. In this case, kill all the paths in which no errors occur.
   * Write a custom plugin that probes the program's state to decide when to kill the path.
   * If you exercise multiple entry points of a library (e.g., a device driver), it may make sense to choose only
     one successful path when an entry point exits and kill all the others. Use the `StateManager <Plugins/StateManager.html>`_ plugin to suspend
     the execution of all paths that returned from a library function until one return succeeds.
   * Kill back-edges of polling loops using the `EdgeKiller <Plugins/EdgeKiller.html>`_ plugin. You can also use
     this plugin when execution enters some block of code (e.g., error recovery).

6. Prioritize paths according to a metric that makes sense for your problem.
   This may be done by writing a custom state searcher plugin. S2E comes with several examples of searchers that aim to maximize code coverage
   as fast as possible.



How to keep memory usage low?
-------------------------------
You can use several options, depending on your needs.

*  Enable the shared framebuffer. By default, each state writes to its own framebuffer, which
   may add up to several megabytes to each state. However, it often does not matter what appears on
   the screen. In such case, use the ``--state-shared-memory=true`` option.

*  Disable forking when a memory limit is reached
   using the following KLEE options: ``--max-memory-inhibit`` and  ``--max-memory=MemoryLimitInMB``.

*  Explicitly kill unneeded paths. For example, if you want to achieve high code coverage and
   know that some path is unlikely to cover any new code, kill it.


How much time is the constraint solver taking to solve constraints?
-------------------------------------------------------------------

Enable logging for constraint solving queries:

::

   s2e = {
    kleeArgs = {
      "--use-query-log", "--use-query-pc-log",  "--use-stp-query-pc-log"
   }

With this configuration S2E generates two logs: ``s2e-last/queries.pc`` and ``s2e-last/stp-queries.qlog``.
Look for "Elapsed time" in the logs.


What do the various fields in ``run.stats`` mean?
-------------------------------------------------

You can open ``run.stats`` in a spreadsheet as a CVS file.
Most of the fields are self-explanatory. Here are the trickiest ones:

* ``QueryTime`` shows how much time KLEE spent in the STP solver.

* ``CexCacheTime`` adds to that time also the time spent while looking
  for a solution in a counter-example cache (which is enabled by ``--use-cex-cache`` KLEE option).
  SolverTime shows how much time KLEE spent in total while solving queries
  (this includes all the solver optimizations that could be enabled by various solver-related KLEE options).


* ``ResolveTime`` represents time that KLEE spent resolving symbolic
  memory addresses, however in S2E this is not computed correctly yet.


* ``ForkTime`` shows how much time KLEE spent on forking states.

