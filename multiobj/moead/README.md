# MOEA/D with TORC parallel objective evaluation

This directory implements **MOEA/D — Multiobjective Evolutionary Algorithm Based on Decomposition** — within [Pi4U](../../README.md). It adapts the original C++ implementation by **Hui Li and Qingfu Zhang**, adding task-parallel objective evaluation through [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).

MOEA/D approximates a set of trade-off solutions by evolving a collection of related scalar optimization subproblems. This implementation supports real-valued decision variables and weight-vector construction for two or three objectives. The supplied application example has **two objectives**. Its evaluations run concurrently across MPI processes and local TORC worker threads.

## Algorithm overview

Consider a multiobjective minimization problem

$$
\min_{x\in\Omega} F(x)
=\bigl(f_1(x),\ldots,f_m(x)\bigr).
$$

A feasible solution dominates another if it is no worse in every objective and strictly better in at least one. Since competing objectives generally have different minimizers, the goal is to approximate the Pareto front rather than find one scalar optimum.

MOEA/D associates each subproblem with a weight vector. Similar weights define neighboring subproblems, which share information during mating and replacement. In this code, each subproblem stores one incumbent candidate, its weight vector, and a list of neighboring subproblem indices.

The default scalarization is the weighted Tchebycheff function:

$$
g(x\mid\lambda,z^*)
=\max_{1\le j\le m}\lambda_j\left|f_j(x)-z_j^*\right|,
$$

where $z^*$ is the best componentwise objective vector observed so far. The implementation substitutes a small positive weight (`0.00001`) when a weight component is zero.

For each stage, the implementation selects two parents from each subproblem's neighborhood, applies simulated binary crossover (SBX), and mutates the first offspring using polynomial mutation with per-variable probability `1 / Nth`. The second crossover offspring is not evaluated or retained. After evaluation, each child can replace the incumbents of multiple neighboring subproblems if it strictly improves their scalarized objective.

Bounds are used both for random initialization and in the bounded crossover/mutation operators. The code has no general nonlinear-constraint interface or feasibility comparator. Such constraints require an application-specific penalty, repair procedure, or an extension to the algorithm.

All objectives are minimized. To maximize an objective, return its negative and restore its sign when interpreting results.

### Scalarization alternatives

`scalarfunc.h` contains three scalarizations:

| Identifier | Method |
| --- | --- |
| `_TCH1` | Weighted Tchebycheff; the default. |
| `_TCH2` | Tchebycheff with objective-range normalization using the reference individuals. |
| `_PBI` | Penalty-based boundary intersection, with penalty coefficient 5. |

The method is set by `strFunctionType` in `params_init()` in `main_moea.cpp`. It is **not a key in `moead.par`**. Rebuild after changing it. `_TCH2` returns a large scalar value when an estimated objective range is zero; consider the objective scales and normalization behavior before selecting it.

## What TORC parallelizes

The executable registers `obj_eval_raw()` and initializes TORC in `MODE_MW`. TORC evaluates both the initial population and subsequent offspring populations as batches.

The main path is:

```text
TMOEAD::init_population() or TMOEAD::evolution()
    -> torc_create(..., obj_eval_raw, ...)
    -> objectives()
    -> fitfun()
```

Each task receives:

| Argument | Contents | Passing mode |
| --- | --- | --- |
| `x` | Decision-variable vector | `CALL_BY_COP` |
| `pnx` | Number of variables | `CALL_BY_COP` |
| `y` | Objective-value vector | `CALL_BY_REF` |
| `pny` | Number of objectives | `CALL_BY_COP` |
| `info` | Four application metadata integers | `CALL_BY_COP` |

The runtime selects a worker with `torc_create(-1, ...)`. A `torc_waitall()` completes each batch before its objective values are consumed. Metadata is `[0, 0, stage, candidate_index]`, with stage 1 for initialization and zero-based candidate indices.

The driver generates all offspring for a stage before submitting evaluations. After the batch finishes, it processes children in subproblem-index order, updating the ideal point and neighborhood incumbents. Parent selection, crossover, mutation, neighborhood construction, scalarization, replacement, and output remain in the driver.

**This is a synchronous, batched MOEA/D adaptation.** In the sequential loop retained under `#if 0`, each child is evaluated and updates the population before later children are generated. The active parallel implementation generates a whole stage from the current incumbents before applying any child updates, so its search trajectory need not match that sequential loop.

The number of subproblems is independent of the number of MPI processes. Each MPI rank does not maintain an independent island population. The commented OpenMP compiler flag does not enable an alternative population-evaluation backend in the active code.

## Requirements

- A C++ compiler supporting at least C++11.
- GNU Make.
- MPI compiler wrapper `mpic++` (or `mpicxx`) and an MPI launcher.
- A separately installed TORC_LITE runtime and POSIX threads.
- Optional: Python 3, NumPy, and Matplotlib for plotting.
- Optional: Gnuplot for the supplied plotting command file.

Use the same MPI installation for TORC, the application compiler wrapper, and the launcher. Standards-compliant TORC execution requires `MPI_THREAD_SERIALIZED` or higher; consult the runtime documentation for platform details.

## Install TORC_LITE

TORC is an external dependency:

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

Use `F77=mpif90` if that is the available MPI Fortran wrapper. Run `autoreconf -i` first if generated build files are missing. Add the `PATH` setting to your shell startup file if appropriate, and make the runtime installation accessible on all participating nodes.

Verify the installed helpers:

```bash
command -v mpic++
command -v torc_cflags
torc_cflags
torc_libs
```

## Build

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/multiobj/moead
make
```

The target is `main_moea`. The Makefile compiles `main_moea.cpp` and `fitfun.cpp` together using `mpic++`, adding flags from `torc_cflags` and `torc_libs`. There is no `use_torc=1` switch: this directory builds the TORC executable directly.

Despite its name, the Makefile variable `CC` selects the **C++** compiler wrapper:

```bash
make clean
make CC=mpicxx
```

To choose a C++ dialect explicitly:

```bash
make clean
make CFLAGS="-O3 -Wall -std=gnu++17 $(torc_cflags)"
```

Use an installed compiler version appropriate to your system. Rebuild C++ application libraries consistently if switching between compiler/standard-library combinations.

`scalarfunc.h` is currently missing from the target's active dependency list. After changing it, run `make clean` before rebuilding. `make clean` removes the executable and matching `.dSYM` directories, but leaves results in `ParetoFront/` intact.

## Run

Run from a directory containing `moead.par` and an output directory named `ParetoFront`:

```bash
mkdir -p ParetoFront
TORC_WORKERS=4 mpirun -np 1 ./main_moea
```

For multiple MPI processes:

```bash
TORC_WORKERS=2 mpirun -np 4 ./main_moea
```

`TORC_WORKERS` controls local worker threads per MPI process. Allocate enough CPUs for these workers, runtime communication, and any threads inside the objective. Ensure each process can access the application inputs.

The program always reads **`moead.par` from the current working directory**. It does not parse a configuration filename from the command line. If the file is absent, compiled defaults are used. For independent experiments, use separate working directories containing their own configuration and `ParetoFront/` directory, then invoke the executable by its full path.

The code performs one run, with run identifier 1, and writes:

```text
ParetoFront/DMOEA_fitfun_R1.dat
```

The filename comes from the compiled problem label `fitfun` and the fixed run identifier. Repeated runs in the same directory overwrite this file. The output stream is not checked for successful opening, so create `ParetoFront/` before running.

## Configuration: moead.par

The supplied configuration is:

```text
Nth          1
Nobj         2
Niche        20
MaxStages    16
PopSize      128
id_cx        20
id_mu        20
Bdef         -10 10
#B0          -6 6
#B1          -6 6
seed         1
```

| Key | Meaning | Compiled default |
| --- | --- | --- |
| `Nth` | Number of real decision variables; unrelated to worker count. | 1 |
| `Nobj` | Objective count. The driver handles 2 or 3; the supplied `fitfun` requires 2. | 2 |
| `Niche` | Number of nearest weight vectors in each neighborhood, including the subproblem itself. | 20 |
| `MaxStages` | Total stages, including initialization as stage 1. | 30 |
| `PopSize` | Requested population size; exact for two objectives, converted to a weight lattice for three. | 64 |
| `id_cx` | SBX distribution index. | 20 |
| `id_mu` | Polynomial-mutation distribution index. | 20 |
| `Bdef` | Default lower and upper bounds for all variables. | `-10 10` |
| `B0`, `B1`, ... | Bounds overriding `Bdef` for the corresponding zero-based variable. | No overrides |
| `seed` | Integer seed for the evolutionary random-number generator. | 1 |

Use `1 <= Nth <= 1024`, because the global bounds arrays have 1024 entries. Bounds must be finite and strictly ordered; equal bounds cause divisions by zero in mutation. Use a positive seed, positive distribution indices, and `MaxStages >= 1`. For two objectives, use `PopSize >= 2`. Ensure `1 <= Niche <=` the **actual** population size; larger neighborhoods access outside the population. A neighborhood with several members is more useful than one-member mating.

The parser uses substring matching and does not comprehensively validate values. Keep one setting per line, use the exact capitalization shown, and start comment lines with `#` in the first column. In particular, bound names are also matched as substrings: `B1` can match a line starting with `B10`. For dimensions with multi-digit bound indices, arrange explicit bound lines in increasing index order and inspect this parser before relying on complex overrides; exact-token parsing is preferable for a general application.

### Two versus three objectives

For two objectives, the lattice subdivision count is `H = PopSize - 1`, giving exactly `H+1 = PopSize` weight vectors:

$$
\lambda_i=\left(\frac{i}{H},\frac{H-i}{H}\right),\quad i=0,\ldots,H.
$$

For three objectives, integer triples summing to `H` generate a simplex lattice with

$$
N_{\mathrm{actual}}=\frac{(H+1)(H+2)}{2}.
$$

The subdivision count is obtained by inverting this relation and rounding down:

```cpp
H = static_cast<int>(
    std::floor((std::sqrt(8.0 * PopSize + 1.0) - 3.0) / 2.0)
);
```

For three objectives, `PopSize` therefore specifies a requested upper limit: the algorithm uses the largest triangular population not exceeding it. For example:

| Requested `PopSize` | `H` | Actual population |
| --- | --- | --- |
| 64 | 9 | 55 |
| 120 | 14 | 120 |
| 128 | 14 | 120 |
| 210 | 19 | 210 |
| 300 | 23 | 300 |

Choose a triangular size such as 55, 120, 210, or 300 when the requested and actual population sizes should match. Use `PopSize >= 3` for three objectives so that `H >= 1`. Validate `Niche` against the actual population size and use that size when calculating evaluation budgets. For example, `PopSize=128` and `MaxStages=16` produce 120 candidates per stage and 1920 evaluations for a three-objective application. The supplied two-objective configuration still produces exactly 128 candidates per stage.

**Setting `Nobj=3` is not sufficient to define a three-objective problem.** Update `fitfun` to write all three outputs. The current adapter initializes the objective vector to zero, so a third output left unwritten becomes a meaningless zero objective. Counts other than 2 or 3 do not launch an optimization in the current driver.

## Supplied objective

The active `fitfun.cpp` computes the sum of all decision variables,

$$
s=\sum_{i=0}^{\mathrm{Nth}-1}x_i,
$$

and returns

$$
f_1(x)=(s+2)^2-10,\qquad
f_2(x)=(s-2)^2+20.
$$

It also sleeps for 10 ms per evaluation to provide a small artificial workload. Remove or replace this delay when measuring an application.

For the supplied one-variable bounds `[-10,10]`, the Pareto set is `x[0]` in `[-2,2]`; the front is the image of that interval under the two formulas. For multiple variables whose feasible sums cover this interval, all feasible vectors with sum in `[-2,2]` are Pareto-optimal. This illustrates that different parameter vectors can map to the same objective vector.

With the supplied two-objective configuration, 128 initial candidates and 15 batches of 128 offspring produce **2048 objective evaluations**. More generally, the active path evaluates `actual_population * MaxStages` candidates for `MaxStages >= 1`. The constructor's reference-individual evaluation loop is skipped by an unconditional `continue`; those individuals do not add objective evaluations. The ideal point is subsequently updated from the evaluated initial population.

## Add an application objective

Implement the function in `fitfun.cpp`:

```cpp
void fitfun(double *x, int nx, double *result, int *winfo);
```

- Read `nx` decision variables from `x`.
- Write every objective entry configured by `Nobj` into `result`.
- Return finite values for every evaluation.
- Treat `winfo` as the four-element metadata array described above.

The function returns through the `result` array rather than its C++ return value. Its current signature does not include the output count. If the application needs that count dynamically, extend the declaration in `objective.h`, the definition in `fitfun.cpp`, and the adapter call consistently. The vector passed to `objectives()` already has the configured number of entries.

Parameters are copied into task-local vectors by `obj_eval_raw()`. All processes execute `params_init()` before TORC initialization, so a shared configuration supplies the same dimensions and bounds on every process. Additional model state, data files, and external-library initialization must likewise be available on each process that executes evaluations.

Keep the objective thread-safe. In particular, do not use the optimizer's `rnd_uni()` concurrently inside tasks: it has static mutable state. For stochastic objectives, use independent application RNG state, with seeds derived from experiment and task identifiers if reproducibility is needed. Use unique temporary paths for external simulations.

Both source files currently use normal C++ linkage. If replacing the objective with separately compiled C code, use matching `extern "C"` declarations guarded by `#ifdef __cplusplus`, compile the C source with a C compiler, and link the resulting object through the C++ MPI wrapper.

## Results and Pareto interpretation

`save_front()` writes one row per subproblem incumbent, with no header:

```text
x[0] ... x[Nth-1]  f[0] ... f[Nobj-1]
```

Thus the first objective is column `Nth+1`, using 1-based file-column numbering. With the default configuration, columns are `x[0], f1, f2`.

Despite the directory and function names, the file is **not filtered for nondominance**. It can contain duplicate solutions and dominated points because each row represents an incumbent for a particular scalar subproblem. Apply a Pareto filter when a nondominated subset is required. The code maintains no external nondominated archive, writes no per-stage population history, and provides no checkpoint/restart mechanism.

Output uses the default C++ stream precision. For accurate numerical postprocessing, increase it explicitly in `save_front()`, for example with `fout.precision(17)` before writing rows.

## Plot the objective space

The bundled Gnuplot file assumes the default one-variable layout and uses columns 2 and 3. Run it from the MOEA/D directory:

```bash
gnuplot ParetoFront/plot.gp
```

It explicitly selects the `x11` terminal. On platforms without X11, choose an available terminal such as `qt`, or use Python. If `Nth` changes, adjust the objective column numbers.

The separate Pi4U helper `plot_pareto.py` plots two selected outputs and highlights the nondominated subset. Place that script alongside the application, or invoke it by its full path:

```bash
python3 -m pip install numpy matplotlib
python3 plot_pareto.py --file ParetoFront/DMOEA_fitfun_R1.dat \
    --columns 2 3 --labels "Objective 1" "Objective 2"
```

For `Nth=2`, the two objectives occupy columns 3 and 4 instead. To save without a GUI, append `--save pareto.png --no-show`. The plot opens by default.

The helper performs nondominance filtering in the selected two outputs. With three objectives, that pairwise front differs from the projection of the three-objective nondominated set. Use a three-objective dominance filter first if the latter is required.

A minimal direct Matplotlib plot, without filtering, is:

```python
import numpy as np
import matplotlib.pyplot as plt

nth = 1  # Match Nth in moead.par.
data = np.loadtxt("ParetoFront/DMOEA_fitfun_R1.dat", ndmin=2)
plt.scatter(data[:, nth], data[:, nth + 1], s=15)
plt.xlabel("Objective 1")
plt.ylabel("Objective 2")
plt.grid(alpha=0.25)
plt.tight_layout()
plt.show()
```

## Performance and reproducibility

The program prints initialization and offspring-batch evaluation times (`init pop took ...`, `evolve pop took ...`). These measure task submission, evaluation, and synchronization; they exclude neighborhood updates and most other driver work. The `constuctor took ...` message is not an objective-evaluation benchmark because the constructor's evaluation loop is skipped.

Parallel speedup depends on evaluation cost, batch size, load variation, and task overhead. Keep enough offspring to occupy workers, and avoid oversubscribing cores with threaded numerical libraries inside each evaluation. The 10 ms delay is an illustrative workload, not a representative scientific simulation.

For comparisons, hold the configuration and seed fixed and use a deterministic objective. The driver applies returned children in a fixed order after each batch, so task completion order does not itself determine replacement order. Independent runs require different seeds and separate output directories. There is no command-line multi-run option.

## Source guide

| File | Responsibility |
| --- | --- |
| `main_moea.cpp` | Configuration parser, globals initialization, TORC startup and run selection. |
| `dmoea.h` | Weight lattice, neighborhoods, population initialization, batched evolution, replacement and output. |
| `individual.h` | Candidate/subproblem classes and raw TORC objective adapter. |
| `objective.h` | Bridge from candidate vectors to `fitfun`. |
| `fitfun.cpp` | User objective; supplied sum-based two-objective example. |
| `scalarfunc.h` | Tchebycheff, normalized Tchebycheff and PBI scalarization. |
| `recombination.h` | Bounded SBX crossover and polynomial mutation. |
| `random.h`, `common.h` | Random-number generator and vector/neighborhood utilities. |
| `global.h` | Dimensions, bounds, ideal point and other optimizer state. |
| `moead.par` | Runtime parameters and bounds. |
| `ParetoFront/` | Final output and example Gnuplot command file. |

Much of the implementation is defined directly in headers and included into the main translation unit. Splitting it into additional translation units requires addressing those definitions rather than simply including all headers from multiple `.cpp` files.

## References and credits

1. Q. Zhang and H. Li. **MOEA/D: A Multiobjective Evolutionary Algorithm Based on Decomposition.** *IEEE Transactions on Evolutionary Computation*, 11(6), 712–731, 2007. [DOI: 10.1109/TEVC.2007.892759](https://doi.org/10.1109/TEVC.2007.892759).
2. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).
3. [TORC_LITE runtime and documentation](https://github.com/CEID-HPCLAB/torc_lite).

The source header credits **Hui Li and Qingfu Zhang** for the original implementation. The Pi4U adaptation and TORC parallelization are by **Panagiotis E. Hadjidoukas**. Preserve original attribution and source notices, and consult the repository licensing information together with the separate TORC terms when redistributing the code.
