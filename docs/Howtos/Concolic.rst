=================================================
Analyzing Large Programs Using Concolic Execution
=================================================

In the previous tutorials, we have seen how to use symbolic execution in order
to explore different program paths. In practice, symbolic execution
requires the S2E user to inject symbolic values in the program's inputs so that
the symbolic execution engine can follow both outcomes of the conditional branches.
This allows covering parts of the program that might be hard to reach otherwise
(e.g., by guessing the inputs, or using random testing).

Unfortunately, symbolic execution may get stuck at the start of an
execution and have a hard time reaching deep paths. This is caused by the
path selection heuristics and by the constraint solver. Path selection heuristics
may not know very well which execution paths to choose so that execution
goes deeper. For example, in a loop that depends on a symbolic condition, the heuristics
may blindly keep selecting paths that would never exit the loop. Even if the path
selection heuristics knew which path to select to go through the whole program,
invoking the constraint solver on every branch that depends on symbolic input may
become increasingly slower with larger and larger path depths.

To alleviate this, S2E also implements *concolic execution*.
Concolic execution works exactly as traditional symbolic execution: it propagates
symbolic inputs through the system, allowing conditional branches to fork new paths
whenever necessary. The key difference is that concolic execution augments these
symbolic values with *concrete* values (hence the term *concolic*). The concrete values
give a hint to the search heuristics about which paths to follow first. In practice,
the S2E user launches the program with concrete arguments that would drive the
program down the path that reaches interesting parts of that program, which S2E would
then thoroughly explore. More practical details are provided in the next sections
of this tutorial.

Concolic execution allows the program under analysis to run to completion
(without getting stuck in the middle) while exploring additional paths along the main concrete path.
On each branch that depends on a symbolic value, the engine follows in priority the one that
would have been followed had the program been executed with concrete values. When the first path
that corresponds to the initial concrete values terminates, the engine will pick another path,
recompute a new set of concrete values, and proceed similarly until this second path terminates.
Of course, custom path selection plugins can optimize the selection for different needs
(high code coverage, bug finding, etc.).

There are two key optimizations to make this feasible: (1) *never* call the constraint solver during the execution
of a path and (2) perform speculative forking. Whenever execution reaches
a branch that depends on symbolic input, S2E forks a new state regardless of the actual
feasibility of that forked state. This avoids calling the constraint solver.
Since S2E stores concrete values for every symbolic variable, the execution engine can
readily evaluate the outcome of every branch condition by substituting symbolic values with concrete ones,
and chose the corresponding concrete path.


S2E invokes the constraint solver when selecting a new state to run. Since all states result
from speculative forks, it is necessary to check whether the states are actually feasible
before running them. For that, S2E invokes the constraint solver to compute the concrete inputs
that would allow execution to reach that state. If the solver fails to find the inputs,
S2E discards the state. Otherwise, S2E uses the computed concrete inputs to resume
concolic execution.




Setting up S2E for Concolic Execution
=====================================

The following is a minimal S2E configuration file that enables concolic execution.

::

    s2e = {
        kleeArgs = {
            "--use-concolic-execution=true",
            "--use-dfs-search=true"
        }
    }


It instructs the S2E engine to enable concolic execution and to use the depth-first path selection heuristic (aka path *searcher*).
The DFS heuristic works well with concolic execution, because it naturally lets the current "concrete"
path run to completion. It is possible to use any existing searcher in concolic mode.
However, it may be better to design new searchers with concolic execution in mind in order to improve
exploration efficiency.


Executing Programs in Concolic Mode
===================================

Using custom instructions
-------------------------

The ``s2e_make_concolic`` custom instruction injects symbolic values while keeping the original concrete values.
It is used in the same way as ``s2e_make_symbolic``. It reads the original concrete values from memory, stores them in an internal cache,
and overwrites the memory with symbolic values. The cache maps the symbolic values to the actual
concrete values and allows the substitution of symbolic inputs with the concrete ones during
expression evaluation (e.g., at fork points).


Using the ``init_env`` plugin
-----------------------------

The `init_env <init_env.html>`_ library enables symbolic execution without modifying the program's source code.
This library also supports concolic execution with the ``--concolic`` switch, that can be added right before the concrete program arguments.
The following example invokes the ``tr`` Unix utility via the ``tr ab ab ab`` command. The library automatically assigns
symbolic arguments to all arguments while keeping the concrete ``ab`` values.


::

   LD_PRELOAD=/home/s2e/init_env.so tr --concolic ab ab ab


FAQ
===

* *Can I use s2e_make_symbolic in concolic mode?*

  Yes. S2E will automatically assign default concrete values satisfying the path constraints during concolic execution.

* *I have cryptographic routines in my code. Can concolic execution get through them?*

  Yes, but. Concolic execution will use the initial concrete values to get through cryptographic routines without getting stuck in the constraint solver.
  However, it is very likely to get stuck when checking the feasibility of a newly-selected deep speculative state (and computing new sets of concrete inputs).
  Concolic execution does not magically make constraint solving easier, it merely defers it to a later point.

* *Concolic execution seems to fork a lot more states than symbolic execution. Why?*

  This is due to speculative forking. In normal symbolic execution, the execution engine only forks a new path
  if the solver reports that both are feasible. In concolic execution, forking is done regardless of the feasibility.
  The execution engine prunes the infeasible paths later, after the path selection heuristic selects one for execution.

* *I implemented custom plugins to aggressively prune paths because symbolic execution was getting stuck. Are these plugins still useful?*

  Yes, reducing state space by discarding uninteresting paths is always useful. Concolic execution does not solve the path explosion
  problem by itself. It merley helps getting to deep parts of the program faster.
