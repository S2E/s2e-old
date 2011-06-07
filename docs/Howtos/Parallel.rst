============
Parallel S2E
============

S2E can be run in multi-process mode.

Append ``-s2e-max-processes XX`` to the command line, where ``XX`` is the maximum number of S2E instances you would like to have.
Also add the ``-nographic`` option.

It works by forking an S2E instance on every branch instruction where both outcomes can happen.

How do I process generated traces?
----------------------------------


In multi-process mode, S2E outputs traces in the ``s2e-last/XX folders``, where ``XX`` is the sequence number of the S2E instance.
This number is incremented each time a new instance is launched. Note that instances can also terminate, e.g., if they
finished exploring their respective subtree.

Each trace contains a subtree of the global execution tree. Therefore, you must process the traces in the relative order
of their creation. The relative order is defined by the sequence number of the instance. This can be done by specifying
multiple ``-trace`` arguments to the offline analysis tools. For example, generating the forkprofile of a multi-core run can be done
as follows:

::

      $ /home/s2e/tools/Release/bin/forkprofiler -outputdir=s2e-last/
      -trace=s2e-last/0/ExecutionTracer.dat -trace=s2e-last/1/ExecutionTracer.dat \
      -trace=s2e-last/2/ExecutionTracer.dat -trace=s2e-last/3/ExecutionTracer.dat





Limitations
-----------

* S2E can only run on a shared-memory architecture. S2E cannot start on one machine and fork new instances on other machines for now.
  This limitation will be fixed soon.
* Because S2E uses the ``fork`` system call, S2E cannot run on Windows in multi-core mode.
