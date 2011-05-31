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

* some constraints are hard to solve. Set a timeout in the constraint solver: **--use-forked-stp --max-stp-time=TimeoutInSeconds**

* you forgot a s2e_disable_all_apic_interrupts call somewhere in your code.

* you are doing unnecessary system calls with symbolic arguments (e.g. printf)

* try to reduce the number of symbolic variables


How do I know what S2E is doing? 
--------------------------------
You can look at the s2e-last/debug.txt.
This file lists all the major events occurring during symbolic execution. One of them is "Firing timer event" which is called every second by S2E from the main execution loop. If you do not see it every second, it means that QEMU is stuck running plugin code (most likely because of a plugin bug) or constraint solver code (because of a complex query). To see which query is causing the problem, look at the query log.

run.stats also contains some statistics. This file is updated every second, but only when executing symbolic code.

How to keep memory usage low?
-------------------------------
You could ask S2E to disable forking when certain memory limit is reached using the following KLEE options: **--max-memory-inhibit --max-memory=MemoryLimitInMB**

However, it is usually better to limit the number of states and killing unneeded states when appropriate.

How much time is the constraint solver taking to solve constraints?
-------------------------------------------------------------------
Try to log constraint solving queries:

::

   s2e = {
    kleeArgs = {
    "--use-query-log", "--use-query-pc-log",  "--use-stp-query-pc-log"
   }

With this configuration S2E generates two logs: queries.pc and stp-queries.qlog


What do the CexCacheTime, ForkTime, SolverTime, ResolveTime and QueryTime fields mean?
------------------------------------------------------------------------------------------------------

* **QueryTime** shows how much time KLEE spent in the STP solver. 

* **CexCacheTime** adds to that time also the time spend while looking for a solution in a counter-example cache (which is enabled by --use-cex-cache KLEE option). SolverTime shows how much time KLEE spent in total while solving queries (this includes all the solver optimisations that could be enabled by various solver-related KLEE options).

* **ResolveTime** represents time that KLEE spend resolving symbolic memory addresses, however in S2E this is not computed correctly yet.

* **ForkTime** shows how much time KLEE spend on forking (i.e., duplication of) states, however in S2E right now this does not take into account a time spent on saving/restoring states of devices. 

