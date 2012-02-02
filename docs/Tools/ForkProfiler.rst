=============
Fork Profiler
=============

The fork profiler tool lists all the program counters that caused a fork. 
This allows to quickly identify the hot spots that cause path explosion problems. 

Examples
~~~~~~~~

The following command will generate a ``forkprofile.txt`` file and place it in
the ``s2e-last`` folder.

  ::

      $ /home/s2e/tools/Release/bin/forkprofiler -trace=s2e-last/ExecutionTracer.dat -outputdir=s2e-last/ \
        -moddir=/home/s2e/experiments/rtl8139.sys/driver -moddir=/home/s2e/experiments/rtl8029.sys/driver


Required Plugins
~~~~~~~~~~~~~~~~

* ExecutionTracer

Optional Plugins
~~~~~~~~~~~~~~~~

* ModuleTracer (for debug information)

