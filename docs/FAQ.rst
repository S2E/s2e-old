======================================
FAQ (Frequently Asked Questions)
======================================

.. contents::
.. sectnum::

S2E
========================


Execution seems stuck/slow. What to do?
---------------------------------------------------
Several things may be going on in your execution:

* Some constraints are hard to solve. Set a timeout in the constraint solver: **--use-forked-stp --max-stp-time=TimeoutInSeconds**
  If you do not see the "Firing timer event" message periodically in the ``debug.txt`` log file, execution got stuck in the
  constraint solver.

* If you used ``s2e_disable_all_apic_interrupts``, you probably forgot an ``s2e_enable_all_apic_interrupts`` call somewhere in your code

* You are doing unnecessary system calls with symbolic arguments (e.g. ``printf``)

* Try to reduce the number of symbolic variables


How do I know what S2E is doing? 
--------------------------------
You can look at the ``s2e-last/debug.txt``.
This file lists all the major events occurring during symbolic execution.
One of them is "Firing timer event" which is called every second
by S2E from the main execution loop. If you do not see it every second,
it means that QEMU is stuck running plugin code (most likely because of a plugin bug)
or constraint solver code (because of a complex query).
To see which query is causing the problem, look at the query log.

``run.stats`` also contains many types statistics. This file is updated every second,
but only when executing symbolic code.



How to keep memory usage low?
-------------------------------
You can use several options, depending on your needs.

1. Enabling the shared framebuffer. By default, each state writes to its own framebuffer, which
may add up to several megabytes to each state. However, it often does not matter what appears on
the screen. In such case, use the **--state-shared-memory=true** option.

2. Disable forking when certain memory limit is reached
using the following KLEE options: **--max-memory-inhibit --max-memory=MemoryLimitInMB**

3. Explicitely kill unneeded states. E.g., if you want to achieve high code coverage and
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
------------------------------------------------------------------------------------------------------

* **QueryTime** shows how much time KLEE spent in the STP solver. 

* **CexCacheTime** adds to that time also the time spend while looking for a solution in a counter-example cache (which is enabled by --use-cex-cache KLEE option). SolverTime shows how much time KLEE spent in total while solving queries (this includes all the solver optimizations that could be enabled by various solver-related KLEE options).

* **ResolveTime** represents time that KLEE spend resolving symbolic memory addresses, however in S2E this is not computed correctly yet.

* **ForkTime** shows how much time KLEE spend on forking (i.e., duplication of) states, however in S2E right now this does not take into account the time spent on saving/restoring states of devices. 

