=======================
Annotation
=======================

The Annotations plugin combines monitoring and instrumentation capabilities to
let users annotate single machine instructions or entire function calls. The
user writes the annotation directly inside the S2E configuration file, using the Lua
language.

This plugin can be used to manipulate:
    - single instructions
    - entire function calls

It requires *FunctionMonitor*, *ModuleExecutionDetector* and an OS monitor plugin
(or the generic *RawMonitor*) as dependencies.

Setting up S2E for Annotation usage
------------------------------------

An example of practical Annotation usage is shown in the `Analyzing Windows Driver tutorial <../Windows/DriverTutorial.html>`_, as follows:

::

	plugins = {
	    "WindowsMonitor",
	    "ModuleExecutionDetector",
	    "FunctionMonitor",
	    "BlueScreenInterceptor",
	    "Annotation"
	}

	pluginsConfig = {}

	-- OS monitor configuration (Win XP)
	pluginsConfig.WindowsMonitor = {
	    version="XPSP3",
	    userMode=true,
	    kernelMode=true,
	    checked=false,
	    monitorModuleLoad=true,
	    monitorModuleUnload=true,
	    monitorProcessUnload=true
	}

	-- Module detector configuration (pcntpci5.sys driver)
	pluginsConfig.ModuleExecutionDetector = {
	    pcntpci5_sys_1 = {
	        moduleName = "pcntpci5.sys",
	        kernelMode = true,
	    },
	}

	-- Annotation configuration
	pluginsConfig.Annotation =
	{
	    ann1 = {
	        active=true,
	        module="pcntpci5_sys_1",
	        address=0x169c9,
	        instructionAnnotation="print_ebx",
	        beforeInstruction = true,
	        switchInstructionToSymbolic = false
	    },
	    ann2 = {
	        module="pcntpci5_sys_1",
	        active  = true,
	        address = 0x1233a,
	        callAnnotation = "copyup",
	        paramcount = 4
	    },
	}

	-- Annotation to fiddle with driver buffer
	function copyup (state, pluginState)
	  buf = state:readParameter(0);
	  len = state:reaParameter(3);
	  for i = 0, len - 1, 1 do
	        state:writeMemorySymb("copyup_buf", buf + i, 1);
	  end
	  state:writeRegister("eax", 1);
	  pluginState:setSkip(true);
	end

	-- Annotation to inspect driver status
	function print_ebx (state, pluginState)
	  status = state:readRegister("ebx");
	  print("Driver status: " .. status);
	end


Options
-------

Each annotation is defined in a single sub-module within an Annotation configuration block.
This plugin accepts an arbitrary number of per-module sections.
Per-module options are prefixed with *"ann_section."* in the documentation (equivalent to
*ann1* and *ann2* in the examples). Refer to the sections below for details.

Configuration options are semantically organized in three groups:
    - common for all annotations
    - specific to function call annotations
    - specific to single instruction annotations


Common options
''''''''''''''''''''''''''''''

These options are common to all types of annotations.

ann_section.module=["string"]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the module. This must match the name returned by the monitoring plugin.

ann_section.active=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Whether the annotation is active or not (default is false).

ann_section.address=[int]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The native address of the instruction or the entry-point of the function to annotate.

Function call annotation
''''''''''''''''''''''''''''''

These options have to be used in order to annotate function calls.

ann_section.callAnnotation=["string"]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the Lua function to execute when the annotation triggers.
This option also specifies that the user wants to annotate the entire function
starting at *module.address*.
The callAnnotation will be triggered twice: once when entering and again when
returning from the annotated function call.

ann_section.paramcount=[int]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The number of input parameters taken by the annotated function, under the **cdecl**
calling convention (default is 0). In fact, this assumes that all parameters are
passed on the stack, and will not work with different calling conventions.

Instruction annotation
''''''''''''''''''''''''''''''

These options have to be used in order to annotate single instructions.

ann_section.instructionAnnotation=["string"]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The name of the Lua function to execute when the annotation triggers.
This option also specifies that the user wants to annotate only the single instruction
at *module.address*.
The instructionAnnotation will be triggered just once when execution reaches the
annotated address.

ann_section.beforeInstruction=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Whether to call the annotation before or after the instruction (default is false).

ann_section.switchInstructionToSymbolic=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Whether to switch to symbolic mode before executing the annotated instruction (default is false).
Please note that symbolic execution is required in order to write symbolic values, ie. you will
need to enable this setting in order to inject symbolic values via the Lua API. Failing to do
so will result in S2E crash.

Configuration Sample
--------------------

Here below is an example of a complete Annotation configuration stanza showing how to specify
annotations for both function calls and single instructions.

::

    pluginsConfig.Annotation = {
        -- function call annotation
        ann1 = {
            module  = "modA",
            active  = true,
            address = 0x0000CAFE,
            callAnnotation = "call_ann",
            paramcount = 1
        },
        -- instruction annotation
        ann2 = {
            module  = "modB",
            active  = true,
            address = 0x0000DEAD,
            instructionAnnotation = "instr_ann",
            beforeInstruction = true,
            switchInstructionToSymbolic = true
        },
    }


Lua API For Annotations
-----------------------

All annotations have two positional parameters:
    1. the current execution state *(curState* from now on)
    2. the current plugin state (*curPlgState* from now on)

As such, the typical signature of a Lua annotation is as follows:

::

    function my_ann (curState, curPlgState)
        -- do awesome stuff here
    end


The execution state object can be manipulated using the *ExecutionState*
object's methods. Similarly, the plugin state parameter exposes the API of the
Annotation plugin, which allows annotations to manipulate the plugin's configuration
at runtime.

The next two sections show a list of all available Lua API functions.

Execution State
'''''''''''''''

    - curState:readParameter(param_no: int) -> int
	For function calls, return the value of input paramater number *param_no*.
	Similarly to the *paramcount* option, this assumes the **cdecl** calling convention
	with all parameters passed on the stack.

    - curState:writeParameter(param_no: int, p_value: int)
	For function calls, change the value of input paramater number *param_no*
	(of size *p_size*) to *p_value*.
	Similarly to the *paramcount* option, this assumes the **cdecl** calling convention
	with all parameters passed on the stack.

    - curState:readMemory(virtual_address: int, mem_size: int) -> int
	Read *mem_size* bytes from memory, starting at address *virtual_address*.
	The upper bound for *mem_size* is fixed by target architecture word size.

    - curState:writeMemory(virtual_address: int, mem_size: int, mem_value: int)
	Write *mem_size* bytes to memory, using content of *mem_value*, starting at address *virtual_address*.
	The upper bound for *mem_size* is fixed by target architecture word size.

    - curState:writeMemorySymb("sym_label": string, virtual_address: int, mem_size: int, [lower_bound: int, upper_bound: int])
	Write a symbolic value of size *mem_size* starting at address *virtual_address*.
	Additional constraints can be specified with the optional parameters,
	restricting symbolic values to the [*lower_bound* , *upper_bound*] range.
	*sym_label* is a mnemonic label used to track the symbolic value.
	Please note that the execution state must be in symbolic mode for this to
	work, ie. if you are annotating a single instruction you should take
	care of setting *switchInstructionToSymbolic=true*.
	Failing to do so will likely result in S2E crash.

    - curState:readRegister("reg_name": string) -> int
	Return the content of register *reg_name*.

    - curState:writeRegister("reg_name": string, "reg_value": int)
	Write value *reg_value* to register *reg_name*.

    - curState:writeRegisterSymb("reg_name": string, "sym_label": string)
	Write a symbolic value into register *reg_name*.
	*sym_label* is a mnemonic label used to track the symbolic value.
	Please note that the execution state must be in symbolic mode for this to
	work, ie. if you are annotating a single instruction you should take
	care of setting *switchInstructionToSymbolic=true*.
	Failing to do so will likely result in S2E crash.

    - curState:isSpeculative() -> bool
	Return whether the current state is executing in speculative mode, ie.
	it has been generated due to pre-forking in concolic mode. Such
	states could be actually discarded at a later point, if the solver
	finds them to be unreachable; for more details check the
	`Concolic Execution <../Howtos/Concolic.html>`_ documentation.

Current Plugin State
''''''''''''''''''''

    - curPlgState:isCall() -> bool
	For function call annotations, whether the annotation has been triggered on a function call.
	Always return *false* for single instruction annotations.

    - curPlgState:isReturn() -> bool
	For function call annotations, whether the annotation has been triggered on function return.
	Always return *false* for single instruction annotations.

    - curPlgState:setValue("key": string, value: int)
	Store the (*key*, *value*) item in the plugin state internal key-value storage.

    - curPlgState:getValue("key": string)
	Retrieve the value corresponding to the index *key* from the plugin state internal key-value storage.

    - curPlgState:setKill(skip: bool)
	Set the internal *isKill* flag. This will cause the current S2E state to be
	terminated after the annotation returns.

    - curPlgState:setSkip(skip: bool)
	Set the internal *isSkip* flag. For function call annotations, this will cause the current
	function to be skipped.

    - curPlgState:activateRule("ann_name": string, active: bool) -> bool
	Activate or deactivate the *ann_name* annotation. Return *true* on normal execution, *false* on errors (eg. no annotations found with such name).

    - curPlgState:exit()
	Abort S2E execution.



Lua Annotation Sample
-----------------------

Here below is an example of a complete Lua annotation showing how manipulate a function call
to inspect arguments and registers status, keeping track of values and injecting
symbolic contents.

::

	-- Annotation to fiddle with a function
	function call_ann (curState, curPlgState)
	  if plg:isCall() then
	    -- Inspect relevant input on function call
	    arg0 = state:readParameter(0)
	    eax = state:readRegister("eax")
	    print ("Calling function with with arg0=" .. arg0)
	    curPlgState:setValue("eax", eax)
	  elseif plg:isReturn() then
	    -- Compare EAX values
	    orig_eax = curPlgState::getValue("eax")
	    new_eax = state:readRegister("eax")
	    print ("Old EAX=" .. orig_eax .. ", new EAX=" .. new_eax)
	    -- Inject a symbolic value on return into EAX
	    state:writeRegisterSymb("eax", "sym_eax")
	  end
	end
