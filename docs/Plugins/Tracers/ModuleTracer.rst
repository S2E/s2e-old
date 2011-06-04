============
ModuleTracer
============

The ModuleTracer records load events for modules specified by the `ModuleExecutionDetector <../ModuleExecutionDetector.html>`_ plugin.
ModuleTracer is required by offline analysis tools to map program counters to specific modules, e.g., to display user-friendly debug information.

Options
-------

This plugin does not have any option.


Required Plugins
----------------

* `ExecutionTracer <ExecutionTracer.html>`_
* `ModuleExecutionDetector <../ModuleExecutionDetector.html>`_

Configuration Sample
--------------------

::

    pluginsConfig.ModuleTracer = {}

