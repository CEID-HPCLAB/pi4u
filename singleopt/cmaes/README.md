# CMA-ES in Pi4U

**Single-objective optimization with serial or TORC-parallel objective evaluation.**

This directory provides the CMA-ES optimization engine of [Pi4U](../../README.md). It builds on [Nikolaus Hansen's c-cmaes](https://github.com/CMA-ES/c-cmaes): the short example becomes `engine_cmaes.c`, the application objective is placed in `fitfun.c`, and the engine adds features for scientific optimization workflows.

These additions include parameter bounds, task metadata, generation data files, replay of previously evaluated generations, progress reports, and execution timings. An experimental asynchronous engine is also included.

For the standalone TORC integration of the original c-cmaes examples, see our [c-cmaes-torc repository](https://github.com/CEID-HPCLAB/c-cmaes-torc). That repository provides parallel short, boundary, and restart examples; this Pi4U module builds on the short example to provide an application engine with a separate `fitfun.c` and the additional workflow features described below.

## Organization

| File | Purpose |
| --- | --- |
| [engine_cmaes.c](engine_cmaes.c) | Main engine, supporting serial and synchronous TORC execution. |
| [engine_cmaes_async.c](engine_cmaes_async.c) | Experimental asynchronous engine using TORC tasks and callbacks. |
| [fitfun.c](fitfun.c) | Application objective; supplied examples include Rosenbrock, Rastrigin, and Cigtab. |
| [cmaes.c](cmaes.c), [cmaes.h](cmaes.h), [cmaes_interface.h](cmaes_interface.h) | CMA-ES implementation and C interface. |
| [cmaes_initials.par](cmaes_initials.par) | Initial distribution, dimension, population size, seed, and stopping criteria. |
| [cmaes_signals.par](cmaes_signals.par) | Output and stopping instructions, reread during optimization. |
| [cmaes_bounds.txt](cmaes_bounds.txt) | Bounds read by the main engine. |
| [Makefile](Makefile) | Serial and TORC builds. |
| [plotcmaesdat.m](plotcmaesdat.m), [cmaplt.py](cmaplt.py) | Plotting tools for CMA-ES logs. |

Both engines include `fitfun.c` directly. It is tracked as a build dependency, rather than compiled as a separate translation unit.

## Optimization workflow

CMA-ES is a derivative-free method for optimizing continuous parameters. It samples candidate solutions from a multivariate normal distribution, evaluates them, and adapts the distribution's mean, covariance, and step size.

The main engine repeats the following steps:

1. Sample a population of `lambda` candidates.
2. Resample candidates that violate the parameter bounds.
3. Evaluate the population, either serially or with one TORC task per candidate.
4. Wait for all evaluations and update the search distribution.
5. Read control signals, report the best solution, and save generation data.

TORC distributes objective evaluations across MPI processes and local workers. Work stealing is enabled during evaluation in the main engine. The CMA-ES update occurs after the whole population has completed.

Parallel execution is useful when objective evaluations are expensive. The supplied benchmark functions are inexpensive and primarily demonstrate the interface.

## Requirements and TORC installation

A serial build requires a C compiler, GNU Make, the C math library, and POSIX threads. A parallel build additionally requires MPI and an external installation of [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).

Install TORC_LITE under your home directory:

```bash
git clone https://github.com/CEID-HPCLAB/torc_lite.git
cd torc_lite
./configure --prefix="$HOME/.local/torc" \
    CC=mpicc F77=mpif90 CFLAGS="-O2 -std=gnu17"
make
make install
export PATH="$HOME/.local/torc/bin:$PATH"
cd ..
```

Use `F77=mpifort` if that is the Fortran wrapper provided by your MPI installation. The explicit C dialect accommodates legacy TORC function declarations on recent GCC/Clang compilers. The TORC repository includes generated configuration files; Autoconf and Automake are needed if those files must be regenerated.

Verify the helper scripts:

```bash
torc_cflags
torc_libs
```

The application Makefile uses these scripts to obtain the include and linker flags. Add the `PATH` export to your shell startup file for future sessions. Build TORC and Pi4U with the same MPI installation, and use its launcher when running. Standards-compliant TORC execution requires `MPI_THREAD_SERIALIZED` or `MPI_THREAD_MULTIPLE`; see its documentation for platform details.

## Build and run

From the Pi4U repository root:

```bash
cd singleopt/cmaes
```

Build and run the serial engine:

```bash
make clean
make use_torc=0
./engine_cmaes
```

Serial mode is the default. To build just the main engine with TORC:

```bash
make clean
make use_torc=1 engine_cmaes
export TORC_WORKERS=4
mpirun -np 1 ./engine_cmaes
```

For four MPI processes with two workers each:

```bash
export TORC_WORKERS=2
mpirun -np 4 ./engine_cmaes
```

`TORC_WORKERS` sets the workers per MPI process; `-np` sets the process count. Ensure the worker setting reaches all processes and choose a layout consistent with the available cores. The main engine has at most `lambda` candidate evaluations available concurrently per generation.

Run from a directory containing the parameter files. Independent runs should use separate directories to keep their output files separate. Run `make clean` when switching between serial and TORC builds because they share executable names.

## Objective-function convention

The application interface is:

```c
double fitfun(double *x, int N, void *output, int *info);
```

| Argument | Meaning |
| --- | --- |
| `x` | Candidate parameter vector of length `N`. |
| `N` | Problem dimension. |
| `output` | Optional application data pointer; these engines pass `NULL`. |
| `info` | Four integers: generation, chain, step, and task identifiers. |

The engines set `info` to `{0, 0, step, task_index}`. These identifiers can help organize evaluation-specific files or diagnostics.

**The engine minimizes `-fitfun(x, ...)`, so it maximizes the value returned by `fitfun()`.** To minimize a loss `L(x)`, return `-L(x)` from `fitfun()`. The supplied benchmarks follow this convention. The logged CMA-ES fitness and `BEST` reports contain the negated value sent to the optimizer.

For example, to minimize a sum of squares:

```c
double fitfun(double *x, int N, void *output, int *info)
{
    double loss = 0.0;
    (void)output;
    (void)info;
    for (int i = 0; i < N; ++i)
        loss += x[i] * x[i];
    return -loss;
}
```

Rosenbrock is selected by default in `fitfun.c`. To change the built-in objective, enable exactly one of `_USE_ROSENBROCK_`, `_USE_RASTRIGIN_`, or `_USE_CIGTAB_`, then rebuild. For a custom application, replace the function body and set its dimension in `cmaes_initials.par`.

Concurrent evaluations should use independent working data and output files. Initialize required model data on every MPI process and make shared state safe for concurrent worker access.

## Configuration and bounds

The supplied `cmaes_initials.par` sets:

| Setting | Value |
| --- | --- |
| Dimension | `N 2` |
| Initial point | `(0, 0)` |
| Initial standard deviations | `(3, 3)` |
| Population size | `lambda 128` |
| Random seed | `seed 3` |

Adjust these values for your objective and computational budget. The legacy `function number` and `restarts` lines refer to the original upstream restart example; these Pi4U engines select the objective through `fitfun.c`.

The main engine reads zero-based parameter bounds from `cmaes_bounds.txt`:

```text
B0 -3 3
B1 -6 6
```

Missing parameter bounds default to `[-6, 6]`. Candidates outside the box are resampled, rather than clipped. Choose a feasible initial point and suitable initial standard deviations to avoid excessive resampling. The asynchronous engine currently uses hardcoded `[-6, 6]` bounds and does not read this file.

`cmaes_signals.par` controls printing, log files, and termination during execution. For example, uncomment `stop now` to request termination when the engine next reads the file.

## Generation dumps and replay

The main engine writes one file per generation:

```text
curgen_db_000.txt
curgen_db_001.txt
...
```

Each file contains `lambda` rows, with `N` parameter values followed by the minimized fitness value `-fitfun(x, ...)`.

To replay saved generations and continue evaluating after the last available file:

```bash
./engine_cmaes -cr
```

Or, with a TORC build:

```bash
mpirun -np 4 ./engine_cmaes -cr
```

Replay starts at generation zero, substitutes the saved populations and fitness values, and reconstructs updates without reevaluating those populations. When the next generation file is absent, normal evaluation resumes. Keep the dimension, population size, configuration, and saved files consistent with the original run.

This is generation-data replay; the files do not store the complete optimizer and random-generator state. Their values are also written with limited precision, so continuation should not be assumed identical to an uninterrupted run.

Additional source switches in `engine_cmaes.c` control best-solution reports (`VERBOSE`), generation dumps (`_IODUMP_`), optional resume output (`_RESTART_`), and a wall-time limit (`JOBMAXTIME`, disabled at zero). Change these in the source and rebuild. The asynchronous engine does not implement `-cr` replay.

## Experimental asynchronous engine

Build both engines with:

```bash
make clean
make use_torc=1
```

Run the asynchronous engine with:

```bash
export TORC_WORKERS=4
mpirun -np 1 ./engine_cmaes_async
```

This version submits detached evaluations and processes results through callbacks on the master process. It updates CMA-ES after batches of `lambda` completed evaluations and launches additional candidates while other evaluations remain in flight. Consequently, a batch can contain candidates sampled from different distribution updates, and its behavior differs from the synchronous generation loop.

The implementation is experimental. Its candidate storage has a fixed `MAXDIM` of 16, so dimensions must not exceed 16 without modifying that storage. It also uses TORC-specific scheduler and callback operations, including an internal scheduler entry point; verify compatibility with the TORC version being used. Use the main engine as the starting point for application integration and baseline comparisons.

The current Makefile lists `engine_cmaes.c` as a dependency for the asynchronous target but omits `engine_cmaes_async.c`. After editing the asynchronous source, use `make clean` before rebuilding until that dependency is corrected.

## Results and plotting

The engine prints the best solution, termination reason, and timing information. The main engine reports total, initialization, processing, evaluation/replay, and finalization times.

Typical files include:

| File | Contents |
| --- | --- |
| `actparcmaes.par` | Actual optimizer parameters. |
| `allcmaes.dat` | Final optimizer state and results. |
| `outcmaesfit.dat` | Fitness, step size, and convergence statistics. |
| `outcmaesaxlen.dat` | Search-distribution axis lengths. |
| `outcmaesstddev.dat` | Coordinate standard deviations. |
| `outcmaesxmean.dat` | Distribution mean. |
| `outcmaesxrecentbest.dat` | Recent best candidate. |
| `curgen_db_*.txt` | Evaluated candidate populations and fitness values. |

The `outcmaes*.dat` files are controlled by `cmaes_signals.par`. From the run directory, use MATLAB/Octave:

```matlab
plotcmaesdat;
```

Or use a Python 3 compatible version of `cmaplt.py` with NumPy and Matplotlib:

```bash
python3 cmaplt.py
```

The legacy plotting script in this directory uses removed Matplotlib APIs and needs the compatibility fixes for current Python environments. The updated script also supports image export with `--save convergence.png --no-show`.

`make clean` removes the executables and object files. `make clear` removes generated `*.dat` files, generation dumps, and `actparcmaes.par` in the current directory; copy results you want to retain before using it.

## Credits and maintenance

The CMA-ES implementation is by **Nikolaus Hansen**. The Pi4U engine and TORC integration were developed by **Panagiotis E. Hadjidoukas and Pi4U collaborators**.

Keep the application engine and `fitfun.c` under Pi4U maintenance. 

The license notice in `cmaes.c` offers Apache License 2.0 or GNU LGPL 2.1 or later for that library. Consult the Pi4U project license and individual source notices for the other components, and the separate TORC_LITE repository for its licensing terms.
