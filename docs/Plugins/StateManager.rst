============
StateManager
============

StateManager divides the path exploration in multiple steps by implementing the notion of execution barriers.
A barrier is a user-defined code location. Whenever a path reaches the barrier, StateManager suspends it.
When all paths reached the barrier or a timeout occurred, StateManager kills all paths except one that reached the barrier.
StateManager then resumes the remaining path.

StateManager is useful to explore execution paths in modules that have multiple entry points (e.g., library or device drivers).
A client plugin may catch all returns from entry points and check the status code of the returned function. If the function failed,
the client plugin may kill the path and otherwise invoke the ``StateManager::succeedState()`` method to suspend the execution of the path.
When there are no more paths to execute, StateManager automatically kills all but one successful.

NdisHandlers is a plugin that uses StateManager to exercise entry points of device drivers.
You can refer to NdisHandlers as an example of how to use StateManager.


StateManager has the following limitations:

- It is not possible to choose which paths to kill. StateManager kills all but one without any guarantee about which one is kept.
- StateManager does not handle multiple barriers properly. Distinct barriers are considered to be just one.

Options
-------

timeout=[seconds]
~~~~~~~~~~~~~~~~~

If no more new code is covered after the specified number of seconds, kill all states except one successful.
If the timeout is zero, continue exploration indefinitely until a client plugin explicitly instructs StateManager to kill the states.


Required Plugins
----------------

StateManager only exposes an API that must be called from a client plugin to be useful.
A client plugin would typically use the `FunctionMonitor <FunctionMonitor.html>`_ plugin to intercept all function returns, check the status of the
returned function, and decide whether to kill the execution path or suspend it using StateManager.

Configuration Sample
--------------------

::

    pluginsConfig.StateManager = {
        timeout = 60
    }

