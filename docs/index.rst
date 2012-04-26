===============================================
The Selective Symbolic Execution (S²E) Platform
===============================================

.. contents::

Do not forget the `FAQ <FAQ.html>`_ if you have questions.

S²E Documentation
=================

* Getting Started

  1. `Building S2E <BuildingS2E.html>`_
  2. `Preparing a VM image for S2E <ImageInstallation.html>`_
  3. `Quicky uploading programs to the guest with s2eget <UsingS2EGet.html>`_

  4. `Testing a simple program <TestingMinimalProgram.html>`_
  5. `Testing Linux binaries <Howtos/init_env.html>`_
  6. `Analyzing large programs using concolic execution <Howtos/Concolic.html>`_
  7. `Equivalence testing <EquivalenceTesting.html>`_
  
* Analyzing Windows Device Drivers

  1. `Step-by-step tutorial <Windows/DriverTutorial.html>`_
  2. `Setting up the checked build of Windows <Windows/CheckedBuild.html>`_  
  
* Analyzing the Linux Kernel

  1. `Building the Linux kernel <BuildingLinux.html>`_
  2. `Using SystemTap with S2E <SystemTap.html>`_

* Howtos

  1. `How to use execution tracers? <Howtos/ExecutionTracers.html>`_
  2. `How to write an S2E plugin? <Howtos/WritingPlugins.html>`_
  3. `How to run S2E on multiple cores? <Howtos/Parallel.html>`_
  4. `How to debug guest code? <Howtos/Debugging.html>`_

* S2E Tools
  
  1. Available Tools
     
     1. `Fork profiler <Tools/ForkProfiler.html>`_
     2. `Translation block printer <Tools/TbPrinter.html>`_
     3. `Execution profiler <Tools/ExecutionProfiler.html>`_
     4. `Coverage generator <Tools/CoverageGenerator.html>`_
   
  2. `Supported debug information <Tools/DebugInfo.html>`_
  
* `Frequently Asked Questions <FAQ.html>`_

S²E Plugin Reference
====================


OS Event Monitors
-----------------

To implement selectivity, S2E relies on several OS-specific plugins to detect
module loads/unloads and execution of modules of interest.

* `WindowsMonitor <Plugins/WindowsInterceptor/WindowsMonitor.html>`_
* `RawMonitor <Plugins/RawMonitor.html>`_
* `ModuleExecutionDetector <Plugins/ModuleExecutionDetector.html>`_

Execution Tracers
-----------------

These plugins record various types of multi-path information during execution.
This information can be processed by offline analysis tools. Refer to
the `How to use execution tracers? <Howtos/ExecutionTracers.html>`_ tutorial to understand
how to combine these tracers.

* `ExecutionTracer <Plugins/Tracers/ExecutionTracer.html>`_
* `ModuleTracer <Plugins/Tracers/ModuleTracer.html>`_
* `TestCaseGenerator <Plugins/Tracers/TestCaseGenerator.html>`_
* `TranslationBlockTracer <Plugins/Tracers/TranslationBlockTracer.html>`_
* `InstructionCounter <Plugins/Tracers/InstructionCounter.html>`_

Selection Plugins
-----------------

These plugins allow you to specify which paths to execute and where to inject symbolic values

* `StateManager <Plugins/StateManager.html>`_ helps exploring library entry points more efficiently.
* `EdgeKiller <Plugins/EdgeKiller.html>`_ kills execution paths that execute some sequence of instructions (e.g., polling loops).
* `BaseInstructions <Plugins/BaseInstructions.html>`_ implements various custom instructions to control symbolic execution from the guest.
* *SymbolicHardware* implements symbolic PCI and ISA devices as well as symbolic interrupts and DMA. Refer to the `Windows driver testing <Windows/DriverTutorial.html>`_ tutorial for usage instructions.
* *CodeSelector* disables forking outside of the modules of interest
* *Annotation* plugin lets you intercept arbitrary instructions and function calls/returns and write Lua scripts to manipulate the execution state, kill paths, etc.

Analysis Plugins
----------------

* *CacheSim* implements a multi-path cache profiler.


Miscellaneous Plugins
---------------------

* `FunctionMonitor <Plugins/FunctionMonitor.html>`_ provides client plugins with events triggered when the guest code invokes specified functions.
* `HostFiles <UsingS2EGet.html>`_ allows to quickly upload files to the guest.

S²E Development
===============

* `Contributing to S2E <Contribute.html>`_
* `Profiling S2E <ProfilingS2E.html>`_


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

