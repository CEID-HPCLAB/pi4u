# ABC by Subset Simulation with TORC

This directory implements **Approximate Bayesian Computation by Subset Simulation (ABC-SubSim)** within [Pi4U](../../README.md). It provides serial and TORC task-parallel execution, a replaceable application discrepancy function, and population files for postprocessing.

The associated research is described in **Kulakova et al. (2016), Approximate Bayesian Computation for Granular and Molecular Dynamics Simulations**, which presents an efficient ABC approach using a variant of subset simulation. The broader task-parallel framework is described in **Hadjidoukas et al. (2015), Exploiting Task-Based Parallelism in Bayesian Uncertainty Quantification**. Full citations are provided below.

## Method

ABC is useful for Bayesian inference when data can be simulated for a parameter vector but evaluating the likelihood is difficult. Given observed data `y_obs`, simulated data `y`, summaries `S`, and a discrepancy `rho`, the ABC posterior has the form

$$
\pi_\epsilon(\theta\mid y_{\mathrm{obs}})\propto
\pi(\theta)\int p(y\mid\theta)
\mathbf{1}\!\left\{\rho(S(y),S(y_{\mathrm{obs}}))<\epsilon\right\}\,dy.
$$

Small tolerances give closer agreement with observations but make direct prior rejection sampling inefficient. ABC-SubSim introduces intermediate tolerances and samples a sequence of nested acceptance regions. At each level, good samples seed short MCMC chains targeting the next conditional distribution.

This implementation:

1. Samples an initial population from the prior and evaluates its discrepancy.
2. Sorts candidates by discrepancy and chooses an intermediate tolerance near a lower population quantile.
3. Estimates a proposal covariance from the population and tunes its scale using short trial chains.
4. Starts short MCMC chains from the best candidates to form a new population.
5. Repeats until a stopping criterion or the level limit is reached.

The chain length is fixed to 5 in the constructor. The number of seeds is `floor(population_size / 5)`, so the nominal retained fraction is approximately 0.2. Choose a population size divisible by 5 so that the chains refill the population exactly.

Unlike the generic formula above, the sampler itself only sees parameters and a scalar discrepancy. Simulation, observed data, summaries, and any treatment of simulator randomness belong in the application callback. The supplied deterministic Rosenbrock example is a functional test of nested discrepancy regions, not a complete statistical observation model.

## What runs in parallel

With TORC enabled, parallel work includes:

- Independent evaluations of prior samples.
- Independent covariance-scale tuning trials.
- Independent short MCMC chains at a subset level.

Steps within each MCMC chain remain sequential. TORC workers execute entire chain tasks, which call the application directly for successive candidates. This avoids requiring a separate remote task for every transition.

The driver synchronizes between phases, broadcasts the selected covariance to participating MPI processes, and collects generated states in a central database. Remote chain tasks return states through database-update tasks on rank 0. A mutex protects allocation of database row indices. Work stealing is enabled around the prior-evaluation and production-chain phases.

Population rows are stored in arrival order, so their order can change with scheduling. Do not assume the file concatenates complete chains or that row order is a time-series order. MPI processes are not separate independent ABC runs.

## Dependencies

- A C++11 compiler and GNU Make.
- GSL and its CBLAS library.
- POSIX threads.
- For parallel execution: MPI and a separately installed [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).
- Optional: Gnuplot for the legacy visualizer; Python, NumPy, and Matplotlib for postprocessing.

On Debian/Ubuntu, install GSL and build tools with:

```bash
sudo apt install build-essential libgsl-dev
```

On macOS/Homebrew:

```bash
brew install gsl
```

The Makefile includes `/opt/homebrew/include` and `/opt/homebrew/lib` for Apple Silicon Homebrew. Adjust those paths for Intel Homebrew or custom installations, or remove them on Linux. Apple's `g++` command normally invokes Clang; either Clang or GCC can compile the C++11 sources.

## Install TORC_LITE

Skip this step for a serial build.

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

Use `F77=mpif90` if that is the available MPI Fortran wrapper. Run `autoreconf -i` first if generated build files are missing. Check `torc_cflags` and `torc_libs`, and use the same MPI installation for the runtime, application wrapper, and launcher. Standards-compliant TORC execution requires `MPI_THREAD_SERIALIZED` or higher.

## Build

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/inference/ABC_SubSim

# Serial sampler only
make abc_subsim_torc.exe

# TORC sampler only
make clean
make use_torc=1 abc_subsim_torc.exe
```

**The executable is named `abc_subsim_torc.exe` in both modes.** The name alone does not establish whether it uses TORC. The default `use_torc=0` builds the serial path; `use_torc=1` selects `mpic++`, defines `_USE_TORC_`, and adds the TORC helper flags.

Clean when switching modes: the target name does not change, so Make may otherwise reuse an executable from the previous mode. Plain `make` also builds the legacy `display_gen.exe` visualization program.

For portable serial GSL flags on Homebrew:

```bash
make clean
make abc_subsim_torc.exe \
    CPPFLAGS="-std=c++11 -O2 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread -I$(brew --prefix gsl)/include" \
    LDFLAGS="-L$(brew --prefix gsl)/lib -lgsl -lgslcblas -pthread"
```

For a Linux TORC build with GSL in standard paths:

```bash
make clean
make use_torc=1 abc_subsim_torc.exe \
    CPPFLAGS="-std=c++11 -O2 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread -D_USE_TORC_=1 $(torc_cflags)" \
    LDFLAGS="-lgsl -lgslcblas $(torc_libs) -pthread"
```

When overriding `CPPFLAGS` on the command line, retain the TORC definition and include flags explicitly, as above. There is no separate library to install from this directory.

## Run and command-line arguments

```text
abc_subsim_torc.exe population_size output_prefix final_tolerance max_levels \
    min_acceptance discrepancy_std_stop tolerance_change seed [restart_file]
```

| Argument | Meaning |
| --- | --- |
| `population_size` | Samples per population; use a multiple of 5, with enough samples for tuning and covariance estimation. |
| `output_prefix` | Filename prefix; `curgen_db` produces `curgen_db_000.txt`, etc. |
| `final_tolerance` | Requested discrepancy tolerance; see the sign restrictions below. |
| `max_levels` | Loop limit including the initial population at level 0; use at least 2. |
| `min_acceptance` | Stop if the reported mean chain acceptance falls below this value; the code restores the preceding population. |
| `discrepancy_std_stop` | Stop if the population standard deviation of discrepancies is below this value. Called `weights std` in the usage message. |
| `tolerance_change` | Stored as `tolerance_decrease`, but not used by the active level loop. |
| `seed` | Integer RNG seed; 0 selects time-based seeding. |
| `restart_file` | Optional population file used instead of a fresh prior population. |

The repository's example invocation is:

```bash
# Serial build
./abc_subsim_torc.exe 2000 curgen_db 0 30 0.05 0 0 2

# TORC build
TORC_WORKERS=4 mpirun -np 1 ./abc_subsim_torc.exe \
    2000 curgen_db 0 30 0.05 0 0 2

# Four MPI processes, two local workers each
TORC_WORKERS=2 mpirun -np 4 ./abc_subsim_torc.exe \
    2000 curgen_db 0 30 0.05 0 0 2
```

These reproduce the example settings; the supplied log discrepancy and zero final tolerance require the interpretation discussed below. For a conventional nonnegative ABC discrepancy, return the raw discrepancy from `fitfun` and choose a strictly positive final tolerance appropriate to the application.

Ensure all workers can access model inputs and any external simulator. The RNG array has **48 entries per process** (`MAX_WORKERS` in `random.h`); keep local worker indices below 48 or increase that capacity consistently before rebuilding. Allocate CPUs for TORC workers, communication, and any model-internal threads.

`runme.sh` deletes existing `curgen_db*.txt` files before launching the example and then calls a Python plotter. Review it before use if results already exist. Use separate working directories for independent experiments.

## Define the prior and application discrepancy

### problem.h

The supplied problem has:

```cpp
int PROBDIM = 2;
double lower_bounds[] = {-6.0, -6.0, -6.0};
double upper_bounds[] = {+6.0, +6.0, +6.0};
double normalization[] = {1, 1, 1};
int enforce_positivity[] = {0, 0, 0};
```

Only the first `PROBDIM` entries are used. Resize all related arrays when increasing dimension. The default prior is uniform and independent over the configured bounds. Both prior sampling and prior-density evaluation use bounds multiplied by `normalization`. Positivity flags provide an additional rejection condition.

Changing the prior requires updating `generate_sample_by_prior()` and `evaluate_prior()` consistently. The log-prior helper is present, but the active standard-ABC acceptance path uses prior densities.

### fitfun.cpp

The callback is:

```cpp
double fitfun(double *x, int N, void *output, int *info);
```

Return a discrepancy for which **smaller means closer agreement**. Unlike a log-posterior callback in other Pi4U inference modules, this value is used directly for ranking and threshold acceptance. `output` is passed as `NULL`; `info` supplies task metadata.

The default implementation returns

$$
\log R(x),\qquad
R(x)=\sum_{i=0}^{N-2}\left[100(x_{i+1}-x_i^2)^2+(x_i-1)^2\right].
$$

Although the logarithm preserves ordering for positive `R`, it can produce negative discrepancies and `-infinity` at `R=0`. The current acceptance code uses `discrepancy_candidate / tolerance < 1`, which is equivalent to the desired `discrepancy_candidate < tolerance` **only for positive tolerances**. A zero tolerance causes division by zero and a negative tolerance reverses the inequality. For standard ABC use, return a nonnegative discrepancy and stop at a positive threshold, or revise and validate the comparisons to support transformed discrepancies explicitly.

Initialization/finalization hooks are available for model setup and cleanup. `fitfun_initialize(NULL)` runs before TORC initialization on each MPI process. Make immutable model data available there as appropriate. Keep the objective thread-safe and give external simulations independent file paths.

Task metadata distinguishes level, chain or tuning task, and within-chain step. Production-chain calls use `[level, chain_index, step, 0]`; tuning calls use their own task indices and place the trial step in the fourth entry. The supplied callback constructs a task-name string from these integers but does not actually create a directory.

## Proposal covariance and tuning

The population covariance is estimated at each level, unless `_FIXED_COV_` is enabled at compilation. The active tuner considers 10 covariance scales, using 5 seed points and 5 trials per scale: 250 short trial tasks per level. Each trial is currently set to one MCMC proposal.

It selects the first scale whose measured acceptance is in `[0.2, 0.5]`, with a fallback when no scale meets that range. These tuning settings are source-level constants, not command-line parameters. The standard-deviation scale grid has squared values from `1/256` through `2`.

The production phase starts `population_size / 5` chains. Each retains its seed and performs four transitions, producing five stored states. Its reported acceptance statistic divides accepted transitions by 5 rather than 4, so its maximum is 0.8. Interpret `min_acceptance` using that implementation convention.

Tuning evaluations are additional work beyond the production populations. The printed evaluation counter is not incremented by the supplied `run_problem()` callback, so it is not currently a reliable count of model calls.

## Outputs and restart

Population files contain one row per sample:

```text
parameter_1 ... parameter_PROBDIM discrepancy
```

Files have no header and use six digits after the decimal point. With the supplied two-dimensional problem, column 3 is the discrepancy. Repeated states from rejected transitions are valid samples and should not be removed.

The level loop writes the previous population before generating the next one. Its final write also uses `level-1`; when a stopping condition breaks the loop, that write can overwrite the previously written file at the same index. Treat suffixes as the current implementation's output labels, not a guaranteed complete archive of every level.

Restart from a population file by appending its path:

```bash
TORC_WORKERS=4 mpirun -np 1 ./abc_subsim_torc.exe \
    2000 resumed_db 0 30 0.05 0 0 2 curgen_db_005.txt
```

The file must contain the requested number of rows and `PROBDIM+1` numeric columns, matching the same problem and discrepancy. The restart reader loads stored parameters and discrepancies without reevaluating them, then begins a new level sequence. It does not restore RNG state, covariance tuning history, or the original level counter; this is population initialization from saved data, not an exact checkpoint continuation.

## Plotting

The optional legacy `display_gen.exe` uses Gnuplot. The separate Pi4U Python helpers can also be placed alongside the sampler or invoked by their full paths:

```bash
python3 -m pip install numpy matplotlib

# Two parameters, colored by discrepancy (column PROBDIM+1)
python3 display_gen_multi.py curgen_db 0 2 --auto-limits \
    --color-label "Discrepancy"

# Plain scatter of any two parameter columns
python3 plot_columns.py curgen_db_005.txt --columns 1 2 \
    --labels "Parameter 1" "Parameter 2"
```

Both open a plot by default; append `--save samples.png --no-show` for GUI-free output. Inspect successive populations, tolerance evolution, sample diversity, and independent runs. These files are populations with dependent chain samples, not independent draws or a single ordered chain suitable for naive time-series diagnostics.

## Implementation notes

The following details matter when extending or validating this research implementation:

- **Threshold indexing:** the tolerance is the average of sorted entries at zero-based indices `pivot` and `pivot+1`, while production uses the best `pivot` seeds. This differs from placing the threshold immediately between the last retained and first discarded values; review ties and quantile indexing when changing the retention policy.
- **Covariance scale applied twice:** the active tuning function repeats the same `gsl_matrix_scale(covariance, scale[my_index]*scale[my_index])` statement. The resulting covariance multiplier is the fourth power of the selected standard-deviation scale, rather than the square tested by tuning. Review this before interpreting tuning as an exact calibration of the production proposal.
- **Bounds and MCMC proposals:** candidates are redrawn until inside prior support, then accepted with the prior-density ratio and discrepancy test. This support-truncated random walk generally has a state-dependent proposal normalization near boundaries; the prior ratio alone does not account for it. Validate the intended conditional stationary distribution, especially for tight bounds or nonuniform priors.
- **Normalization mutates live samples:** `write_array_to_file()` divides stored coordinates in place. With the default all-one normalization this is harmless, but nonunit values can change the subsequent population state. Use a copy for output before enabling nontrivial normalization.
- **Stopping controls:** `tolerance_change` is unused. The printed QOI-change message refers to a condition that compares a nonnegative relative change with zero, so that stop is inactive. `weights std` means discrepancy standard deviation, not an importance-weight diagnostic. The final-tolerance stop also uses a ratio and assumes positive tolerances.
- **Experimental code paths:** `PROBABILISTIC` is disabled and its alternate acceptance implementation is commented out; it is not a ready-to-use probabilistic-ABC switch. Covariance Cholesky failures and malformed inputs also need application-level validation.

These limitations concern the implementation and its defaults, not the ABC-SubSim method in the cited paper. Preserve the intended target distribution when repairing or generalizing the sampler. The literature's evidence-estimation capability is not an automatically produced output of this program.

## Reproducibility and cleanup

The implementation allocates a GSL `ranlux389` generator for each local worker and seeds streams on participating MPI ranks. A fixed seed aids reproducibility, but task scheduling and worker assignment can change which random stream generates a particular chain; serial and parallel populations need not match row for row.

`make clean` removes `.exe` files and `.dSYM` directories. `make clear` deletes `curgen_db*.txt`. `make cleanall` removes both matching executables and those population files. Custom output prefixes are not necessarily removed by these targets.

## Source map

| File | Responsibility |
| --- | --- |
| `abc_subsim.cpp` | Driver, population database, tolerance selection, covariance tuning, MCMC chains, TORC orchestration and output. |
| `abc_subsim.h` | Sampler class and fixed chain-length settings. |
| `problem.h` | Dimension, bounds, normalization, prior and application adapter. |
| `fitfun.cpp`, `fitfun.h` | Replaceable discrepancy and model lifecycle hooks. |
| `random.h` | Worker-local random generators and multivariate Gaussian helpers. |
| `display_gen.c`, `gnuplot_i.c`, `gnuplot_i.h` | Legacy Gnuplot visualization. |
| `runme.sh` | Example launch and plotting commands. |

## References

1. M. Chiachio, J. L. Beck, J. Chiachio, and G. Rus. **Approximate Bayesian Computation by Subset Simulation.** *SIAM Journal on Scientific Computing*, 36(3), A1339–A1358, 2014. [DOI: 10.1137/130932831](https://doi.org/10.1137/130932831).
2. S.-K. Au and J. L. Beck. **Estimation of small failure probabilities in high dimensions by subset simulation.** *Probabilistic Engineering Mechanics*, 16(4), 263–277, 2001. [DOI: 10.1016/S0266-8920(01)00019-4](https://doi.org/10.1016/S0266-8920(01)00019-4).
3. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).
4. [TORC_LITE runtime and documentation](https://github.com/CEID-HPCLAB/torc_lite).

### Related publications by the Pi4U team

5. L. Kulakova, P. Angelikopoulos, P. E. Hadjidoukas, C. Papadimitriou, and P. Koumoutsakos. **Approximate Bayesian Computation for Granular and Molecular Dynamics Simulations.** *Proceedings of the Platform for Advanced Scientific Computing Conference (PASC ’16)*, Article 4, pp. 1–12, ACM, 2016. [DOI: 10.1145/2929908.2929918](https://doi.org/10.1145/2929908.2929918).
6. P. E. Hadjidoukas, P. Angelikopoulos, L. Kulakova, C. Papadimitriou, and P. Koumoutsakos. **Exploiting Task-Based Parallelism in Bayesian Uncertainty Quantification.** *Euro-Par 2015: Parallel Processing*, Lecture Notes in Computer Science, vol. 9233, pp. 532–544, Springer, 2015. [DOI: 10.1007/978-3-662-48096-0_41](https://doi.org/10.1007/978-3-662-48096-0_41).

The PASC paper is the directly relevant application reference for this ABC-SubSim work; the Euro-Par paper describes the broader task-parallel Bayesian computing framework. Preserve original author and copyright notices in the source files, including the ETH Zurich notices, and consult repository and dependency licenses when redistributing.
