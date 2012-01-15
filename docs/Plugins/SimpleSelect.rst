============
SimpleSelect
============

The SimpleSelect plugin is supposed to make it simple to select code that you
want to execute simbolically. It works best together with `init_env
<../Howtos/init_env.html>`_, but also provides some custom instructions that you
can directly use in your programs. This page describes the custom instructions.

Using SimpleSelect, you can limit symbolic execution to individual processes or
individual files. Outside your selected files, forking is turned off.

Custom Instruction
------------------

SimpleSelect defines the following custom instructions (in the ``s2e.h`` header):

.. code-block:: c

   void s2e_simpleselect_process(const char* name);
   void s2e_simpleselect_process_userspace(const char* name);
   void s2e_simpleselect_process_code(const char* name);

These instructions all limit forking to the current process. Additionally,
``s2e_simpleselect_process_userspace`` excludes all kernel-code that is invoked
by the current process. Forking only happens in userspace. The third variant,
``s2e_simpleselect_process_code`` further limits forking to one particular
binary. This means that forking is disabled in library calls invoked by your
program (e.g. ``printf`` from the c standard library).

The ``name`` parameter is used for printing nice debug messages only.
Preferably, you set it to the name of your program. The one *exception* is
``s2e_simpleselect_process_code``, where ``name`` specifies the name of the
binary that is to be selected.

Most of the time, it is easiest to have the `init_env <../Howtos/init_env.html>`_
library call these functions for you. Still, you can use them directly in your
programs as follows:

.. code-block:: c

    int main(int argc, char* argv[]) {
      ...
      s2e_simpleselect_process_code(argv[0]);
      ...
    }


Options
-------

No configuration is needed for SimpleSelect.
