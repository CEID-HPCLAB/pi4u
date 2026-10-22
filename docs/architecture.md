# TORC and Pi4U architecture

Pi4U contains several scientific algorithms that repeatedly evaluate a user-supplied model, likelihood, response function, or objective. In TORC-enabled implementations, the evaluation functions are dispatched as tasks to the [TORC-Lite](https://github.com/CEID-HPCLAB/torc_lite) runtime.

```text
                 Pi4U algorithm / driver
                          |
                  model evaluation tasks
                          |
                  TORC-Lite task runtime
                    /            \
            worker threads     MPI processes
                    \            /
                      model code
```

## Why this separation matters

- The algorithm decides which points to evaluate and when results are needed.
- TORC manages execution of tasks across available workers and MPI processes.
- A model evaluator may call a compiled library function or launch an external executable.
- Some engines allow the evaluations to be scheduled concurrently or as nested tasks.

Actual supported task patterns, synchronization points, MPI modes, and load balancing behavior vary by engine. Their source code and per-module README files are authoritative.

## Design and deployment considerations

**Thread safety.** Multiple model evaluations may run in one address space. Avoid shared mutable state or protect it appropriately.

**MPI consistency.** Build the runtime and applications against the same MPI implementation; use the scheduler's supported launcher.

**External simulations.** Use task-specific working directories, provide input data on the executing nodes, and collect results through documented files or interfaces.

**Portability.** Keep compiler/platform-specific options in the relevant Makefile instead of assuming the settings of another engine apply.

## Status of proposed extensions

Library-call interfaces, deeply nested parallel composition, broader MATLAB/R/Python bindings, and execution-time prediction/scheduling are research directions for the Pi4U continuation. This page does **not** imply that all current engines already expose a stable thread-safe library API or language bindings.
