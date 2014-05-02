===============================================
The Selective Symbolic Execution (S²E) Platform
===============================================

.. contents::

Do not forget the `FAQ <FAQ.rst>`_ if you have questions.

S²E Documentation
=================

* Getting Started

  1. `Building S2E <BuildingS2E.rst>`_
  2. `Preparing a VM image for S2E <ImageInstallation.rst>`_
  3. `Quickly uploading programs to the guest with s2eget <UsingS2EGet.rst>`_

  4. `Testing a simple program <TestingMinimalProgram.rst>`_
  5. `Testing Linux binaries <Howtos/init_env.rst>`_
  6. `Analyzing large programs using concolic execution <Howtos/Concolic.rst>`_
  7. `Equivalence testing <EquivalenceTesting.rst>`_
  
* Analyzing Windows Device Drivers

  1. `Step-by-step tutorial <Windows/DriverTutorial.rst>`_
  2. `Setting up the checked build of Windows <Windows/CheckedBuild.rst>`_  
  
* Analyzing the Linux Kernel

  1. `Building the Linux kernel <BuildingLinux.rst>`_
  2. `Using SystemTap with S2E <SystemTap.rst>`_

* Howtos

  1. `How to use execution tracers? <Howtos/ExecutionTracers.rst>`_
  2. `How to write an S2E plugin? <Howtos/WritingPlugins.rst>`_
  3. `How to run S2E on multiple cores? <Howtos/Parallel.rst>`_
  4. `How to debug guest code? <Howtos/Debugging.rst>`_

* S2E Tools
  
  1. Available Tools
     
     1. `Fork profiler <Tools/ForkProfiler.rst>`_
     2. `Trace printer <Tools/TbPrinter.rst>`_
     3. `Execution profiler <Tools/ExecutionProfiler.rst>`_
     4. `Coverage generator <Tools/CoverageGenerator.rst>`_
   
  2. `Supported debug information <Tools/DebugInfo.rst>`_
  
* `Frequently Asked Questions <FAQ.rst>`_

S²E Plugin Reference
====================


OS Event Monitors
-----------------

To implement selectivity, S2E relies on several OS-specific plugins to detect
module loads/unloads and execution of modules of interest.

* `WindowsMonitor <Plugins/WindowsInterceptor/WindowsMonitor.rst>`_
* `RawMonitor <Plugins/RawMonitor.rst>`_
* `ModuleExecutionDetector <Plugins/ModuleExecutionDetector.rst>`_

Execution Tracers
-----------------

These plugins record various types of multi-path information during execution.
This information can be processed by offline analysis tools. Refer to
the `How to use execution tracers? <Howtos/ExecutionTracers.rst>`_ tutorial to understand
how to combine these tracers.

* `ExecutionTracer <Plugins/Tracers/ExecutionTracer.rst>`_
* `ModuleTracer <Plugins/Tracers/ModuleTracer.rst>`_
* `TestCaseGenerator <Plugins/Tracers/TestCaseGenerator.rst>`_
* `TranslationBlockTracer <Plugins/Tracers/TranslationBlockTracer.rst>`_
* `InstructionCounter <Plugins/Tracers/InstructionCounter.rst>`_

Selection Plugins
-----------------

These plugins allow you to specify which paths to execute and where to inject symbolic values

* `StateManager <Plugins/StateManager.rst>`_ helps exploring library entry points more efficiently.
* `EdgeKiller <Plugins/EdgeKiller.rst>`_ kills execution paths that execute some sequence of instructions (e.g., polling loops).
* `BaseInstructions <Plugins/BaseInstructions.rst>`_ implements various custom instructions to control symbolic execution from the guest.
* *SymbolicHardware* implements symbolic PCI and ISA devices as well as symbolic interrupts and DMA. Refer to the `Windows driver testing <Windows/DriverTutorial.rst>`_ tutorial for usage instructions.
* *CodeSelector* disables forking outside of the modules of interest
* `Annotation <Plugins/Annotation.rst>`_ plugin lets you intercept arbitrary instructions and function calls/returns and write Lua scripts to manipulate the execution state, kill paths, etc.

Analysis Plugins
----------------

* *CacheSim* implements a multi-path cache profiler.


Miscellaneous Plugins
---------------------

* `FunctionMonitor <Plugins/FunctionMonitor.rst>`_ provides client plugins with events triggered when the guest code invokes specified functions.
* `HostFiles <UsingS2EGet.rst>`_ allows to quickly upload files to the guest.

S²E Development
===============

* `Contributing to S2E <Contribute.rst>`_
* `Profiling S2E <ProfilingS2E.rst>`_


S²E Publications
================

* `S2E: A Platform for In Vivo Multi-Path Analysis of Software Systems
  <http://dslab.epfl.ch/proj/s2e>`_.
  Vitaly Chipounov, Volodymyr Kuznetsov, George Candea. 16th Intl. Conference on
  Architectural Support for Programming Languages and Operating Systems
  (`ASPLOS <http://asplos11.cs.ucr.edu/>`_), Newport Beach, CA, March 2011.

* `Testing Closed-Source Binary Device Drivers with DDT
  <http://dslab.epfl.ch/pubs/ddt>`_. Volodymyr Kuznetsov, Vitaly Chipounov,
  George Candea. USENIX Annual Technical Conference (`USENIX
  <http://www.usenix.org/event/atc10/>`_), Boston, MA, June 2010.

* `Reverse Engineering of Binary Device Drivers with RevNIC
  <http://dslab.epfl.ch/pubs/revnic>`_. Vitaly Chipounov and George Candea. 5th
  ACM SIGOPS/EuroSys European Conference on Computer Systems (`EuroSys
  <http://eurosys2010.sigops-france.fr/>`_), Paris, France, April 2010.

* `Selective Symbolic Execution <http://dslab.epfl.ch/pubs/selsymbex>`_. Vitaly
  Chipounov, Vlad Georgescu, Cristian Zamfir, George Candea. Proc. 5th Workshop
  on Hot Topics in System Dependability, Lisbon, Portugal, June 2009

