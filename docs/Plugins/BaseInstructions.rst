================
BaseInstructions
================

This plugin implements various custom instructions to control the behavior of symbolic execution from within the guest OS.
S2E extends the x86 instruction set with a custom opcode. This opcode takes an 8-bytes operand
that is passed to plugins that listen for custom instructions. The content of the operand is plugin specific.

::

   #S2E custom instruction format
   0f 3f XX XX YY YY YY YY YY YY

   XX: 16-bit instruction code. Each plugin should have a unique one.
   YY: 6-bytes operands. Freely defined by the instruction code.

``s2e.h`` defines a basic set of custom instructions. You can extend this by assigning an unused instruction code
to your custom instruction. S2E does not track instruction code allocation. S2E calls all the plugins that listen for
a custom opcode in the order of their registration.

Creating symbolic values and concretizing them
----------------------------------------------

.. code-block:: c

    /** Make the content of the specified buffer symbolic */
    void s2e_make_symbolic(void* buf, int size, const char* name);


    /** Concretize the expression
    /** This function adds path constraints */
    void s2e_concretize(void* buf, int size);


    /** Get an example value for the expression stored in buf */
    /** This function does NOT add path constraints. */
    void s2e_get_example(void* buf, int size);


    /** Return an example value for the expression passed in val */
    /** It is meant to be used in printf-like functions*/
    unsigned s2e_get_example_uint(unsigned val);


Controlling path exploration
----------------------------

These functions control the path exploration from within the guest.
The guest can enable/disable forking as well as kill states at any point in the code.
When forking is disabled, S2E follows only one branch outcome, even if
both outcomes are feasible.

.. code-block:: c

    /** Enable forking on symbolic conditions. */
    void s2e_enable_forking(void);

    /** Disable forking on symbolic conditions. */
    void s2e_disable_forking(void);

    /** Terminate current state. */
    void s2e_kill_state(int status, const char* message)

    /** Get the current execution path/state id. */
    unsigned s2e_get_path_id(void);


Printing messages
-----------------

These custom instructions allow you to print messages and symbolic values
to the S2E log file. This is useful for debugging.

.. code-block:: c

    /** Print a message to the S2E log. */
    void s2e_message(const char* message);

    /** Print a warning to the S2E log and S2E stdout. */
    void s2e_warning(const char* message);

    /** Print a symbolic expression to the S2E log. */
    void s2e_print_expression(const char* message, int expression);



S2E configuration
-----------------

.. code-block:: c

    /** Get S2E version or 0 when running without S2E. */
    int s2e_version();


    /** Get the current S2E_RAM_OBJECT_BITS configuration macro */
    int s2e_get_ram_object_bits();


Controlling interrupt behavior
------------------------------

These functions allow to speed up execution in some circumstances by
limiting the number of concrete/symbolic switches. *They can easily hang
your system. Use with care.*

.. code-block:: c

    /** Disable timer interrupt in the guest. */
    void s2e_disable_timer_interrupt();


    /** Enable timer interrupt in the guest. */
    void s2e_enable_timer_interrupt();


    /** Disable all APIC interrupts in the guest. */
    void s2e_disable_all_apic_interrupts();


    /** Enable all APIC interrupts in the guest. */
    void s2e_enable_all_apic_interrupts();
