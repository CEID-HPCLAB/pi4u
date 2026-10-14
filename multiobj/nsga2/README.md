# NSGA-II with TORC parallel objective evaluation

This directory provides a task-parallel implementation of **NSGA-II (Non-dominated Sorting Genetic Algorithm II)** within [Pi4U](../../README.md). It adapts the C implementation developed at the Kanpur Genetic Algorithms Laboratory (KanGAL, IIT Kanpur), retaining its evolutionary operators and adding parallel population evaluation through [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).

The same sources build serial, OpenMP, or TORC versions of `nsga2r`. TORC supports concurrent evaluations across MPI processes and local worker threads. This is useful when each candidate requires an expensive simulation, numerical calculation, or model evaluation.

## Algorithm

NSGA-II searches for a set of trade-off solutions to a vector-valued optimization problem. All objectives in this implementation are **minimized**. For feasible candidates, a solution dominates another if it is no worse in every objective and strictly better in at least one.

Each generation:

1. Select parents using dominance and crowding-distance comparisons.
2. Generate offspring through crossover and mutation.
3. Evaluate the offspring objectives and constraints.
4. Merge parents and offspring into a population of size `2 * popsize`.
5. Fill the next population in nondominated-front order, using crowding distance to retain diversity when a front does not fit completely.

Real variables use simulated binary crossover (SBX) and polynomial mutation. The original implementation also contains binary-variable operators. However, the current Pi4U evaluation wrapper receives only real variables; binary or mixed-variable applications require extending that wrapper and its task arguments.

The result is an approximation to the Pareto front, rather than one scalar optimum. Rank 1 denotes the first nondominated front within the population considered. It does not establish global Pareto optimality.

### Constraints and maximization

Constraint values follow the convention **`constr[j] >= 0` means satisfied**. The aggregate violation is the sum of negative constraint values. Feasible candidates have aggregate violation zero. Feasible candidates dominate infeasible candidates; between infeasible candidates, a violation closer to zero is preferred.

To maximize an objective, return its negative to NSGA-II. Restore the original sign when reporting or interpreting that objective.

## TORC execution model

`nsga2r.c` registers `test_problem_torc()` before initializing TORC in `MODE_MW`. In `eval.c`, `evaluate_pop()` creates one task for each individual, with automatic worker selection through `torc_create(-1, ...)`, then calls `torc_waitall()`.

The eight task arguments are:

| Argument | Contents | Passing mode |
| --- | --- | --- |
| `x` | `nreal` real-valued parameters | `CALL_BY_VAL` |
| `pnx` | Number of parameters | `CALL_BY_COP` |
| `obj` | `nobj` objective values | `CALL_BY_REF` |
| `pno` | Number of objectives | `CALL_BY_COP` |
| `constr` | `ncon` constraint values | `CALL_BY_RES` |
| `pncon` | Number of constraints | `CALL_BY_COP` |
| `constr_violation` | Aggregate violation | `CALL_BY_RES` |
| `info` | Four metadata integers | `CALL_BY_COP` |

The metadata is `[0, 0, generation, individual_index]`; generations start at 1 and individual indices at 0. `CALL_BY_VAL` is used for array input in this legacy TORC API; it does not change the evaluation function into a C scalar-by-value interface.

The initial population and each offspring population are evaluated as synchronous batches. Selection, crossover, mutation, nondominated sorting, crowding-distance calculations, and result writing remain in the driver. Increasing the MPI process count does not create independent evolutionary populations or islands.

Objective data required by workers must be available on every process executing tasks. The task receives the dimensions explicitly, so custom evaluation code should use those arguments instead of assuming optimizer globals are initialized on remote workers. Keep evaluations thread-safe, and use unique files or directories for external simulations.

Please not that TORC_LITE supports zero-length arguments: the task passes a zero-length constraint array when `ncon=0`.

## Requirements and TORC installation

- C99-capable compiler and GNU Make.
- MPI compiler wrapper and launcher for the TORC version.
- POSIX threads and a separately installed TORC_LITE runtime.
- An OpenMP-capable compiler for the OpenMP version.
- Gnuplot for optional live plotting, or Python with NumPy and Matplotlib for postprocessing.

Install TORC outside Pi4U:

```bash
git clone https://github.com/CEID-HPCLAB/torc_lite.git
cd torc_lite
./configure --prefix="$HOME/.local/torc" \
    CC=mpicc F77=mpifort CFLAGS="-O2 -std=gnu17"
make
make install
export PATH="$HOME/.local/torc/bin:$PATH"
cd ..
```

Use `F77=mpif90` if that is the available MPI Fortran wrapper. If generated build files are missing, run `autoreconf -i` before configuring. Keep the MPI installation used to build TORC, compile NSGA-II, and launch the executable consistent. Standards-compliant TORC execution requires `MPI_THREAD_SERIALIZED` or higher; see the runtime documentation for details.

Verify the helper scripts:

```bash
command -v mpicc
command -v torc_cflags
torc_cflags
torc_libs
```

Make the installation and application data available on all participating nodes. Add the `PATH` export to your shell startup file if appropriate.

## Build

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/multiobj/nsga2
make clean
make use_torc=1
```

The TORC setting selects `mpicc` for compiling and linking, defines `_USE_TORC_`, and obtains flags from `torc_cflags` and `torc_libs`.

The Makefile defaults to `gcc-16` for serial and OpenMP builds. Override both compiler and linker if that command is unavailable:

```bash
# Serial
make clean
make use_torc=0 use_omp=0 CC=gcc LD=gcc

# OpenMP
make clean
make use_torc=0 use_omp=1 CC=gcc LD=gcc
```

Always clean when changing execution mode: Make does not track flag changes in existing object files. Select one parallel backend at a time. If both settings are enabled, `eval.c` takes the TORC branch; this does not parallelize each TORC task with the OpenMP population loop.

## Run a two-objective example

The executable reads a configuration filename as its first argument:

```bash
TORC_WORKERS=4 mpirun -np 1 ./nsga2r config.in
TORC_WORKERS=2 mpirun -np 4 ./nsga2r config.in
```

`TORC_WORKERS` controls local workers per MPI process. Allocate CPUs accordingly, including allowance for runtime communication and any threads used inside the objective. Keep enough candidates in each evaluation batch to occupy the workers.

For the serial or OpenMP executable:

```bash
./nsga2r config.in
OMP_NUM_THREADS=4 ./nsga2r config.in
```

Use this complete, GUI-free configuration for the current objective:

```text
0.5
128
100
2
0
1
-10 10
0.9
0.5
5
10
0
0
```

The lines specify seed, population size, generations, objectives, constraints, real-variable count, bounds, crossover probability, mutation probability, crossover distribution index, mutation distribution index, binary-variable count, and live-display flag, respectively.

The existing `test.in` uses one real variable and two objectives, with live plotting enabled. `test2.in` declares two real variables, but the active objective only depends on the first.

## Configuration reference

Inputs are whitespace-separated numbers read with `fscanf`; do not add inline comments or textual labels. The parser does not robustly check every read, so supply every required value in order.

| Position | Value | Requirement |
| --- | --- | --- |
| 1 | Random seed | Strictly between 0 and 1. |
| 2 | `popsize` | At least 4 and a multiple of 4. |
| 3 | `ngen` | At least 1; includes the initial generation. |
| 4 | `nobj` | Number of objectives; use exactly 2 with the current evaluator. |
| 5 | `ncon` | Number of constraints; use 0 unless all constraint values are implemented. |
| 6 | `nreal` | Number of real variables; at least 1 for the current evaluator. |
| Next | Lower and upper bounds | One pair per real variable. |
| Next | `pcross_real`, `pmut_real` | Each in `[0,1]`; read when `nreal > 0`. |
| Next | `eta_c`, `eta_m` | Positive SBX and polynomial-mutation distribution indices. |
| Next | `nbin` | Use 0 with the present real-only evaluation interface. |
| Next | Live-display flag | 0 disables Gnuplot, 1 enables it. |

If binary variables are configured, the original parser additionally reads bit counts and bounds for each binary variable, then binary crossover and mutation probabilities. Extend the evaluation interface before using those variables in an application.

With live display enabled and two objectives, append two 1-based objective indices for the x and y axes. For more objectives, the parser first requests 2D or 3D display, followed by objective indices and, for 3D, viewing angles. Disabling live display is convenient on clusters or when using Python plotting.

Absent restart replay or early time-limit termination, the algorithm evaluates `popsize * ngen` candidates: one initial population and one offspring population for each later generation.

## Default objective and expected Pareto front

The active `test_problem_torc()` in `problemdef.c` evaluates:

$$
f_1(x)=(x_0+2)^2-10,\qquad
f_2(x)=(x_0-2)^2+20.
$$

Only `x[0]` is used. There are no active constraints. For bounds containing `[-2,2]`, the Pareto set is `x[0]` in that interval, and the objective-space front is the corresponding parametric curve. Values outside the interval are dominated by an endpoint. The offsets change the displayed objective values but not the trade-off structure.

The separate `fitfun.c` example instead uses the sum of all real variables and includes a 10 ms delay. **It is compiled but is not called by the active evaluator.** The delay therefore does not affect the default benchmark's evaluation time. This simple active objective is too cheap to demonstrate meaningful parallel speedup.

## Define an application objective

The shared serial, OpenMP, and TORC path calls:

```c
void test_problem_torc(double *x, int *pnx,
                       double *obj, int *pno,
                       double *constr, int *pncon,
                       double *constr_violation, int info[4]);
```

Use `*pnx`, `*pno`, and `*pncon` for the array lengths. Fill every objective and, when applicable, every constraint. Leave the aggregation loop at the end of the wrapper in place. For example, `constr[0] = 25.0 - obj[1]` enforces `obj[1] <= 25` under this implementation's sign convention.

The unused `test_problem()` retains the original interface, but changing it alone does not change the current evaluation path.

### Enable the separate fitfun example

The separate objective helper has the following interface:

```c
void fitfun(double *x, int nx, void *output, double *result,
            int ncon, double *constraints, int *winfo);
```

Replace the active objective assignments with:

```c
fitfun(x, nx, NULL, obj, ncon, constr, info);
```

Use `nobj=2` and `ncon=0` for that example. To generalize to arbitrary numbers of objectives, extend the helper interface to include `no` and update both declaration and definition. The wrapper already receives that count through `pno`.

In the call above, `output` is unused and set to `NULL`, while `info` is passed as application metadata through `winfo`.

## Output files

All output is written in the current working directory. Use separate directories for independent runs; fixed filenames are overwritten.

| File | Contents |
| --- | --- |
| `initial_pop.out` | Initial evaluated population. |
| `final_pop.out` | Final selected population. |
| `best_pop.out` | Feasible, rank-1 members of the final population. |
| `all_pop.out` | Selected populations across generations, with generation comments. |
| `params.out` | Configuration and operator counts. |
| `eval_db_001.txt`, etc. | Evaluated initial population or subsequent offspring batches. |
| `curgen_db_001.txt`, etc. | Selected parent population after environmental selection. |

`best_pop.out` is a set of trade-off solutions, not one best candidate or an archive of every nondominated point encountered. It can contain no data rows if no feasible solution is found.

### Column order matters

The `.out` population reports use:

```text
objectives | individual constraints | real variables | binary bits |
aggregate constraint violation | rank | crowding distance
```

The default text `eval_db_*.txt` and `curgen_db_*.txt` dumps instead use:

```text
real variables | objectives | aggregate constraint violation
```

These text dumps contain no header, individual constraint values, rank, crowding distance, or binary genes. With `nreal=1` and `nobj=2`, their columns are `x[0], f1, f2, violation`.

The Makefile enables `IODUMP=1` and `BINARY=0`. Changing to binary evaluation dumps uses a different layout (objectives first, then real variables and aggregate violation); binary files must not be read with a text plotting tool.

## Python visualization

The separate Pi4U plotting helpers `display_gen_multi.py` and `plot_pareto.py` can be placed alongside the executable or invoked by their full path. They are not dependencies of NSGA-II and are not supplied by this directory's Makefile.

```bash
python3 -m pip install numpy matplotlib
```

For the one-variable example, plot its two objectives from a selected-generation dump:

```bash
python3 plot_pareto.py curgen_db 100 1
```

The Pareto plotter defaults to columns `dim+1` and `dim+2`. For a standard `.out` report, objectives come first, so select them explicitly:

```bash
python3 plot_pareto.py --file best_pop.out --columns 1 2 \
    --labels "Objective 1" "Objective 2"
```

For a two-variable application, visualize parameter space with either objective determining point color:

```bash
python3 display_gen_multi.py curgen_db 100 2 --auto-limits
python3 display_gen_multi.py curgen_db 100 2 --color-column 4 --auto-limits
```

The parameter-space tool requires at least two parameters. Its default coloring column is `dim+1`; column numbers are absolute and 1-based. Both tools open a plot by default and support saving without a GUI, for example `--save pareto.png --no-show`.

The Pareto helper computes nondominance only in the selected pair of objectives and does not enforce constraints. For constrained problems, use feasible rows. When there are more than two objectives, a pairwise nondominated subset is different from projecting the complete multiobjective rank-1 set; use `best_pop.out` to identify the latter before plotting.

## Initial populations, dumps, and restart code

If `user_initial_pop.txt` exists, it supplies `popsize * nreal` real values in individual-major order, instead of random real-variable initialization. Provide exactly one candidate per row with `nreal` values and ensure values satisfy the bounds. Objectives are subsequently evaluated.

The source includes replay code that reads `eval_db_*.txt`, but `checkpoint_restart` is initialized to 0 and has no command-line switch. Existing dumps therefore do not automatically restart a run. The text dumps omit individual constraints and binary state, so this replay path needs further work for general constrained or mixed-variable applications.

`JOBMAXTIME`, if defined to a positive value at compilation, enables a generation-level remaining-time check. It is not a general scheduler integration or a complete checkpoint facility.

`make clean` removes the executable and object files. **`make clear` deletes `eval_db_*.txt`, `curgen_db_*.txt`, and all `*.out` files in the current directory.** Archive results before using it.

## Performance and reproducibility

Use expensive, independent objectives to evaluate scalability. The driver prints the elapsed population evaluation time as `Gen ... took ... ms`. Whole-run time also includes evolutionary operations, output, and an unconditional one-second sleep after generation 1.

Use the same seed, configuration, and deterministic objective when comparing backends. Objective-internal random numbers, external file collisions, or unsynchronized mutable state can introduce differences even when the evolutionary seed is fixed. Avoid oversubscribing CPUs with both TORC workers and internally threaded model libraries.

## Source map

| Files | Role |
| --- | --- |
| `nsga2r.c` | Configuration, TORC initialization, generation loop, output and replay logic. |
| `eval.c` | Serial, OpenMP, and TORC population evaluation. |
| `problemdef.c`, `fitfun.c` | Active evaluation wrapper and optional separate application example. |
| `dominance.c`, `rank.c`, `fillnds.c`, `crowddist.c` | Constraint-aware dominance, ranking, survival and diversity. |
| `tourselect.c`, `crossover.c`, `mutation.c` | Selection and variation operators. |
| `report.c`, `display.c` | Output, dumps and live Gnuplot display. |
| `Readme_nsga`, `Revision`, `Notice` | Original implementation documentation and notices. |

## References and credits

1. K. Deb, A. Pratap, S. Agarwal, and T. Meyarivan. **A Fast and Elitist Multiobjective Genetic Algorithm: NSGA-II.** *IEEE Transactions on Evolutionary Computation*, 6(2), 182–197, 2002. [DOI: 10.1109/4235.996017](https://doi.org/10.1109/4235.996017).
2. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).
3. [TORC_LITE runtime and installation documentation](https://github.com/CEID-HPCLAB/torc_lite).

The original implementation is credited to **Kalyanmoy Deb and KanGAL, IIT Kanpur**. The random-number code is credited in `Notice` to **David E. Goldberg**. The Pi4U parallel adaptation is by **Panagiotis E. Hadjidoukas**; contact: `phadjido@ceid.upatras.gr`.

Preserve `Notice` and the original source notices. The bundled KanGAL notice permits academic use and modification and places restrictions on commercial use. Consult those terms together with the Pi4U and separate TORC licenses; do not assume that the directory inherits an unrestricted license solely from the enclosing project.
