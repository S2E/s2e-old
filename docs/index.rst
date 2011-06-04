================================================
The Selective Symbolic Execution (S²E) Framework
================================================


S²E Documentation
=================

* Getting Started

  1. `Building S2E <BuildingS2E.html>`_
  2. `Preparing a VM image for S2E <ImageInstallation.html>`_
  3. `Quicky uploading programs to the guest with s2eget <UsingS2EGet.html>`_

  4. `Testing a simple program <TestingMinimalProgram.html>`_
  5. `Equivalence testing <EquivalenceTesting.html>`_
  
* Analyzing Windows Device Drivers

  1. `Step-by-step tutorial <Windows/DriverTutorial.html>`_
  2. `Setting up the checked build of Windows <Windows/CheckedBuild.html>`_  
  
* Analyzing the Linux Kernel

  1. `Building the Linux kernel <BuildingLinux.html>`_
  2. `Using SystemTap with S2E <SystemTap.html>`_

* Howtos

  1. `How to use execution tracers? <ExecutionTracers.html>`_
  2. `How to write an S2E plugin? <WritingPlugins.html>`_

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

* `WindowsMonitor <Plugins/WindowsInterceptor/WindowsMonitor.html>`_
* `FunctionMonitor <Plugins/FunctionMonitor.html>`_

S²E Development
===============

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

