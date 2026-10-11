# AMaLGaM optimization with TORC

This directory provides **TORC-parallel, single-objective AMaLGaM optimization** as part of [Pi4U](../../README.md). It combines Peter A. N. Bosman's full-covariance AMaLGaM implementation with task-based evaluation of application objectives through [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).

The two parallel executables share the same objective interface but offer different control over population sizes and restarts:

| Executable | Purpose |
| --- | --- |
| `AMaLGaM-Full_torc` | Full-covariance AMaLGaM with explicitly configured population size, population count, and adaptation settings. |
| `AMaLGaM-Full-Free_torc` | Full-covariance AMaLGaM with automatic population settings and a restart schedule; a useful starting point when tuning those settings is inconvenient. |
| `AMaLGaM-Full-Free` | Serial parameter-free implementation retained for comparison. |

“Free” means that the algorithm manages several search-strategy parameters automatically. The objective, dimension, initialization range, evaluation budget, and termination settings still need to be supplied.

## Algorithm overview

AMaLGaM stands for **Adapted Maximum-Likelihood Gaussian Model Iterated Density-Estimation Evolutionary Algorithm**. It is a derivative-free estimation-of-distribution algorithm (EDA) for continuous optimization.

Each population maintains a Gaussian model. After ranking candidates, the algorithm selects a fraction of the best solutions and estimates their mean and covariance. New candidates are sampled from an adapted version of this distribution. The full covariance matrix captures dependencies between parameters, including directions that are not aligned with the coordinate axes.

For selected points $x_1,\ldots,x_m$, the basic estimates have the form

$$
\mu = \frac{1}{m}\sum_{k=1}^{m}x_k,
\qquad
\Sigma = \frac{1}{m}\sum_{k=1}^{m}(x_k-\mu)(x_k-\mu)^T.
$$

AMaLGaM supplements these estimates with distribution-multiplier adaptation and anticipated mean shift. These mechanisms regulate exploration and use the direction of recent mean movement to help sustain progress. The implementation uses Cholesky factorization for Gaussian sampling, truncation selection, and preservation of a best solution in each population.

Multiple populations can explore different parts of the search space. In the parameter-free variant, a restart schedule increases population size and alternates between single-population and multiple-population configurations. The code starts from a base population size of approximately `17 + 3 * dim^1.5`, with selection fraction `0.35`, multiplier decrease `0.9`, and standard-deviation-ratio threshold `1.0`.

The word “Full” refers to the covariance model. These files implement the non-incremental full-covariance variant; other AMaLGaM variants described in the literature are not represented by these executable names.

## What TORC parallelizes

Both TORC versions register `installedProblemEvaluation()` and initialize TORC in `MODE_MW`. They create an evaluation task for each candidate during population initialization and during subsequent generations.

Each task receives:

1. A problem identifier.
2. A copy of the candidate parameter vector.
3. Output storage for the objective value.
4. Output storage for the constraint violation.
5. Five application metadata integers.

Inputs use `CALL_BY_COP`, and objective/constraint outputs use `CALL_BY_RES`. `torc_waitall()` ensures that evaluation results are available before selection, distribution adaptation, or population processing continues. The evaluation counter is updated when the driver submits a task.

Sampling, model estimation, covariance factorization, and adaptation remain in the driver. This is synchronous batch evaluation, with concurrent objective tasks across MPI processes and local TORC workers. The number of algorithmic populations is independent of the MPI process count: `nop` or the parameter-free `pop` argument does not allocate MPI ranks.

Speedup depends primarily on evaluation cost, batch size, and variation in evaluation duration. Cheap benchmark functions can be dominated by scheduling overhead. Full covariance storage grows quadratically with dimension, and dense factorization has cubic cost, which can become significant even when evaluations are parallel.

## Requirements

- A C compiler and GNU Make.
- MPI, with `mpicc` and `mpirun` or `mpiexec`.
- POSIX threads and a separately installed TORC_LITE runtime.
- Python 3, NumPy, and Matplotlib for the Python plotting tool (optional).

The default Himmelblau objective uses only the C math library. Some alternative probability-density examples in `fitfun.c` require GSL or additional application helpers; those dependencies are not included in the default linker flags.

Use one consistent MPI installation for TORC, these executables, and the launcher. TORC requires `MPI_THREAD_SERIALIZED` or `MPI_THREAD_MULTIPLE` for standards-compliant execution. Consult the TORC repository for installation and platform-specific details.

## Install TORC_LITE

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

Use `F77=mpifort` if that is the MPI Fortran wrapper available on your system. The explicit C dialect accommodates legacy TORC function declarations on recent GCC/Clang compilers. The generated `configure` script is included; Autoconf and Automake are needed when regenerating build files.

Check the installed helpers:

```bash
command -v mpicc
command -v torc_cflags
command -v torc_libs
torc_cflags
torc_libs
```

The application Makefile invokes `torc_cflags` and `torc_libs` automatically. Add the `PATH` export to your shell startup file for future sessions, and make the runtime installation available on all participating cluster nodes.

## Build

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/singleopt/AMaLGaM
make AMaLGaM-Full_torc AMaLGaM-Full-Free_torc
```

These targets use `mpicc`. This directory does not use a `use_torc=1` switch; the `_torc` target names select parallel versions. Plain `make` also builds the serial executable and the legacy C visualization tool.

The supplied Makefile does not add an optimization level by default. For an optimized GCC/Clang build:

```bash
make clean
make AMaLGaM-Full_torc AMaLGaM-Full-Free_torc \
    TCFLAGS="$(torc_cflags) -O3 -std=gnu17 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE"
```

Both sources include `fitfun.c` directly. After editing it, run `make clean` and rebuild: the current target dependencies do not list that included file.

## Quick start: parameter-free TORC version

The following example evaluates the current two-dimensional Himmelblau objective, writes statistics and populations, and enables termination at objective value `1e-8`:

```bash
export TORC_WORKERS=4
mpirun -np 1 ./AMaLGaM-Full-Free_torc -s -w -v -r \
    0 2 -6 6 0 10000 1e-8 1e-12 4
```

For four MPI processes with two workers each:

```bash
export TORC_WORKERS=2
mpirun -np 4 ./AMaLGaM-Full-Free_torc -s -w -v -r \
    0 2 -6 6 0 10000 1e-8 1e-12 4
```

`TORC_WORKERS` sets the workers per MPI process. Ensure it reaches every launched process and choose a process/thread layout consistent with the resources allocated by your scheduler.

## Command-line options

Both versions accept these options before the positional arguments:

| Option | Meaning |
| --- | --- |
| `-?` | Print usage and exit; quote it in a shell command as `'-?'`. |
| `-P` | Print the installed problem labels and exit. |
| `-s` | Write statistics every generation. |
| `-w` | Write populations, selections, and best solutions every generation. |
| `-v` | Print the configuration before the run. |
| `-r` | Enable value-to-reach termination. |
| `-g` | Full version only: override selected strategy parameters with built-in guidelines. |

### Parameter-free version

```text
AMaLGaM-Full-Free_torc [options] pro dim low upp rot eva vtr tol pop
```

| Argument | Meaning |
| --- | --- |
| `pro` | Problem identifier; use `0` for the current application wrapper. |
| `dim` | Number of continuous parameters. |
| `low`, `upp` | Common lower and upper initialization limits. |
| `rot` | Objective rotation angle; use `0` for the supplied workflow. |
| `eva` | Maximum evaluation budget. |
| `vtr` | Objective value to reach, active only with `-r`. |
| `tol` | Fitness-variance tolerance used to identify converged populations. |
| `pop` | Maximum number of algorithmic populations in the restart schedule. |

The code requires all nine arguments even when `-r` is absent. A converged population/run can trigger another start; the overall evaluation budget and enabled target criterion govern the parameter-free search.

### Explicitly configured Full version

```text
AMaLGaM-Full_torc [options] pro dim low upp rot tau pop nop dmd srt eva vtr imp tol
```

The first five arguments and `eva`, `vtr`, and `tol` have the meanings above. The additional strategy arguments are:

| Argument | Meaning |
| --- | --- |
| `tau` | Fraction selected by truncation selection. |
| `pop` | Number of candidates per population. |
| `nop` | Number of algorithmic populations. |
| `dmd` | Distribution-multiplier decrease factor, between zero and one. |
| `srt` | Standard-deviation-ratio threshold for variance adaptation. |
| `imp` | Maximum no-improvement stretch while the multiplier is at most one. |

Example with 100 candidates in each of four populations:

```bash
export TORC_WORKERS=4
mpirun -np 1 ./AMaLGaM-Full_torc -s -w -v -r \
    0 2 -6 6 0 0.35 100 4 0.9 1.0 10000 1e-8 25 1e-12
```

With `-g`, all fourteen positional arguments are still required, although `tau`, `pop`, `dmd`, `srt`, and `imp` are overridden by the guideline settings. Read the printed `-v` configuration to see the effective values.

## Objective function and constraints

Both TORC versions call the application interface in [fitfun.c](fitfun.c):

```c
double fitfun(double *x, int N, void *output, int *info);
```

The engine minimizes `-fitfun(x, N, NULL, info)`. Therefore, return the negative of a loss you want to minimize, or return a score you want to maximize. The logs contain the minimized, negated value.

The selected `_USE_HIMMELBLAU_` implementation returns

$$
-0.1\sum_{i=0}^{N-2}\left[(x_i^2+x_{i+1}-11)^2+(x_i+x_{i+1}^2-7)^2\right].
$$

For `N=2`, the optimizer consequently minimizes `0.1` times the standard Himmelblau function. Its optimum objective value is zero. Use at least two dimensions for this test case.

Enable exactly one objective macro in `fitfun.c`, or replace its function body with your application. The `pro` argument and `-P` output are legacy wrappers: the current label is always “Rosenbrock,” and changing `pro` does not select a different `fitfun.c` objective.

The `info` array contains five integers: start, phase, generation, population, and individual. In the parameter-free version, phase is `0` during initialization and `1` during generation evaluation. In the Full version, start and phase are currently set to zero for both evaluation phases. These identifiers can help create evaluation-specific files; include process or run identifiers as needed for concurrent applications.

The default wrapper returns zero constraint violation. The algorithm has feasibility-aware comparison machinery, but custom constraint handling must be implemented in the evaluation wrapper to return a meaningful nonnegative aggregate violation. `output` is passed as `NULL` and does not provide the constraint result automatically.

### Initialization limits and actual bounds

`low` and `upp` define where the initial population is sampled. They are not enforced box constraints throughout optimization. The current installed-problem bounds are approximately `[-1e308, 1e308]` in every coordinate. To restrict the search to physical parameter ranges, implement appropriate per-coordinate bounds in the installed-problem bound functions and check the sampling/repair behavior for your application.

Keep `rot=0` for the standard application path. Nonzero rotation uses a rotation matrix initialized in the optimizer; distributed use requires that this matrix also be initialized on every process executing evaluation tasks.

Objective evaluations must be safe to run concurrently: use independent temporary files, avoid unsynchronized shared mutable state, and initialize model data on every MPI process.

## Results and plotting

Population and best-solution files have no column labels. Each row contains `dim` parameter values, then the minimized objective, then the aggregate constraint violation.

| File | Contents |
| --- | --- |
| `statistics.dat` | Generation/evaluation counts, objective and constraint statistics, and population-specific information. |
| `all_populations_generation_00000.dat` | All candidates at a numbered generation. |
| `population_00000_generation_00000.dat` | Candidates for one population and generation. |
| `selection_00000_generation_00000.dat` | Selected candidates for a population. |
| `best_generation_00000.dat` | Best candidate in that generation. |
| `all_populations_generation_final.dat` | Populations at the end of a run. |
| `population_00000_generation_final.dat` | Final candidates for one population. |
| `best_generation_final.dat` | Best candidate in the final populations (`Full`), or the tracked best-so-far solution (`Full-Free`). |

`-s` and `-w` enable intermediate output; the run also writes final statistics and solutions. The parameter-free executable reuses generation filenames across starts, so later starts can overwrite earlier files. Use separate working directories for independent experiments and archive per-start output if the full restart history is needed.

The included Python plotter opens a window by default:

```bash
python3 -m pip install numpy matplotlib
python3 display_gen.py all_populations_generation 0 2 --auto-limits
```

This reads `all_populations_generation_00000.dat`, plots parameters 1 and 2, and colors points by column 3 (the objective). For higher dimensions:

```bash
python3 display_gen.py all_populations_generation 10 3 1 3 --auto-limits
python3 display_gen.py all_populations_generation 10 3 --all-pairs --auto-limits
```

Save without a GUI:

```bash
python3 display_gen.py all_populations_generation 0 2 \
    --auto-limits --save population.png --no-show
```

The plotter defaults to `[-6,6]` axes unless `--auto-limits` or explicit limits are supplied. The legacy `display_gen` executable uses Gnuplot; Python visualization avoids that dependency.

`make clean` removes executables. Both `make clear` and `make cleanall` delete all `*.dat` files in the current directory, including saved optimization results.

## Reproducibility and practical settings

The default source path uses a fixed random seed of `280675`. There is no command-line seed option. The commented Makefile flag `_RANDOMIZE_` differs from the `RANDOMIZE` macro tested in the sources; moreover, `RANDOMIZE` also switches the evaluation wrapper to a built-in Rosenbrock branch. For independent runs of a custom objective, change the seed initialization directly or introduce a dedicated seed option rather than enabling that macro blindly.

Evaluation budgets are checked around algorithmic batches, so treat `eva` as a termination budget rather than a guarantee that submission stops exactly at that count. Increase the available worker count only when batches contain enough evaluations and the objective is costly enough to amortize task overhead. Coordinate TORC workers with any threading inside numerical libraries or external models.

## References and credits

1. P. A. N. Bosman, J. Grahl, and D. Thierens. **Benchmarking Parameter-Free AMaLGaM on Functions With and Without Noise.** *Evolutionary Computation*, 21(3), 445–469, 2013. [DOI: 10.1162/EVCO_a_00094](https://doi.org/10.1162/EVCO_a_00094).
2. P. A. N. Bosman. **On Empirical Memory Design, Faster Selection of Bayesian Factorizations and Parameter-Free Gaussian EDAs.** *GECCO 2009*, 389–396. This is the reference cited in the source headers.
3. [Original AMaLGaM software and description](https://homepages.cwi.nl/~bosman/source_code.php).
4. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).
5. [TORC_LITE runtime and documentation](https://github.com/CEID-HPCLAB/torc_lite).

The original algorithm implementation is credited to **Peter A. N. Bosman**, with **Jörn Grahl** and **Dirk Thierens** contributing to the underlying research. The Pi4U adaptation and TORC parallelization are by **Panagiotis E. Hadjidoukas and collaborators**.

Preserve the original copyright and source notices. The AMaLGaM files contain their own author-specific notice; consult it alongside the Pi4U project license and the separate TORC licensing terms.
