======================================
FAQ (Frequently Asked Questions)
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

You can look at the ``s2e-last/debug.txt``.
This file lists all the major events occurring during symbolic execution.
One of them is "Firing timer event" which is called every second
by S2E from the main execution loop. If you do not see it every second,
it means that QEMU is stuck running plugin code (most likely because of a plugin bug)
or constraint solver code (because of a complex query).
To see which query is causing the problem, look at the query log.

``run.stats`` also contains many types statistics. This file is updated every second,
but only when executing symbolic code.



Execution seems stuck/slow. What to do?
---------------------------------------
Several things may be going on in your execution:

* Some constraints are hard to solve. Set a timeout in the constraint solver with ``--use-forked-stp`` and ``--max-stp-time=TimeoutInSeconds``.
  If you do not see the "Firing timer event" message periodically in the ``debug.txt`` log file, execution got stuck in the
  constraint solver.

* If you used ``s2e_disable_all_apic_interrupts``, you probably forgot an ``s2e_enable_all_apic_interrupts`` call somewhere in your code.
  Use this functionality with care, disabling interrupts can hang your system

* You are doing unnecessary system calls with symbolic arguments (e.g. ``printf``).
  Use the ``s2e_get_example_*`` functions to provide a concrete value to ``printf``  without actually adding path
  constraints, to prevent disabling future paths.

* Try to reduce the number of symbolic variables

* If you use a depth-first search and execution hits a polling loop, rapid forking may occur and execution may never exit the loop.
  Moreover, depending on the accumulated constraints, each iteration may be slow.
  Try to use a different search strategy or kill the unwanted execution paths.


How do I deal with state explosion?
-----------------------------------

Use the *selective* aspect of S2E to kill states that are not interesting and prevent forking outside modules of interest.
The following describes concrete steps that allowed us to explore programs most efficiently.

1. Run your program with minimum symbolic input (e.g., 1 byte) and with tracing enabled (see first section).

2. Insert more and more symbolic values until state explosion occurs (i.e., it takes too long for you to explore all the paths
   or it takes too much memory/CPU resources).

3. Extract the fork profile and identify the code locations that fork the most.

4. If forking occurs outside the module of interest, a few options are:

   * Use the ``CodeSelector`` plugin to disable forking when execution leaves the module of interest
   * Concretize some symbolic values when execution leaves the module of interest. You may need to use the ``FunctionMonitor`` plugin
     to track function calls and concretize parameters.
   * Provide example values to the library (e.g., to ``printf``, as described previously)

5. Prioritize states according to a metric that makes sense for your problem.
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

*  Explicitly kill unneeded states. E.g., if you want to achieve high code coverage and
   know that some state is unlikely to cover any new code, kill it.


How much time is the constraint solver taking to solve constraints?
-------------------------------------------------------------------

First, enable logging for constraint solving queries:

::

   s2e = {
    kleeArgs = {
    "--use-query-log", "--use-query-pc-log",  "--use-stp-query-pc-log"
   }

With this configuration S2E generates two logs: ``s2e-last/queries.pc`` and ``s2e-last/stp-queries.qlog``
Look for "Elapsed time" in the logs.


What do the various fields in ``run.stats`` mean?
-------------------------------------------------

You can open ``run.stats`` in a spreadsheet as a CVS file.
Most of the fields are self-explanatory. Here are the trickiest ones:

* ``QueryTime`` shows how much time KLEE spent in the STP solver.

* ``CexCacheTime`` adds to that time also the time spend while looking
  for a solution in a counter-example cache (which is enabled by ``--use-cex-cache`` KLEE option).
  SolverTime shows how much time KLEE spent in total while solving queries
  (this includes all the solver optimizations that could be enabled by various solver-related KLEE options).


* ``ResolveTime`` represents time that KLEE spend resolving symbolic
  memory addresses, however in S2E this is not computed correctly yet.


* ``ForkTime`` shows how much time KLEE spend on forking (i.e., duplication of) states,
  however in S2E right now this does not take into account the time spent on saving/restoring states of devices.

