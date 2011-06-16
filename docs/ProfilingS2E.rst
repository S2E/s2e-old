=============
Profiling S2E
=============

This page explains how to profile and optimize S2E itself.

Running OProfile
================

1. Recompile STP, KLEE and QEMU with ``-fno-omit-frame-pointer`` option in ``CFLAGS`` and ``CXXFLAGS``
2. Run QEMU as usual with the workload you want to profile
3. Start OProfile using the following commands::

    $ sudo opcontrol --reset
    $ sudo opcontrol --no-vmlinux --callgraph=128 --start

4. Wait for some time to get statistics (remember, this is statistical profiling, time is important)
5. Stop OProfile using the following command::

    $ sudo opcontrol --stop

6. Now you can use ``opreport`` to generate various profiling reports

Viewing results with ``kcachegrind``
====================================

You can convert results to kcachegrind-readable format with the following command::

    $ opreport -gdf | op2calltree

However, callgraph information is not preserved by this convertion tool.

Generating callgraphs with ``gprof2dot`` and ``graphviz``
=========================================================

1. Download the ``gprof2dot`` tool from http://code.google.com/p/jrfonseca/wiki/Gprof2Dot
2. Run the following commands::

    $ opreport -lcD smart image:/path/to/qemu | \
      ./gprof2dot.py -f oprofile -n 1 -e 1 -s > prof.dot
    $ dot prof.dot -Tpng -o prof.png

Now you can view the generated ``prof.png`` file. You can change its verbosity by modifying ``-n`` and ``-e`` options
(minimal percentage of nodes and edges to show) or removing  the ``-s`` option (strip function arguments).

