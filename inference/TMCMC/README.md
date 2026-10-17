# Transitional Markov Chain Monte Carlo (TMCMC)

This directory contains the C implementation of **Transitional Markov Chain Monte Carlo (TMCMC)** in [Pi4U](https://github.com/CEID-HPCLAB/pi4u). TMCMC generates samples from a Bayesian posterior through a sequence of increasingly concentrated intermediate distributions. It also estimates the model evidence, which can be used for Bayesian model comparison.

The implementation supports serial execution, shared-memory execution with OpenMP, and task-based parallel execution with [TORC-Lite](https://github.com/CEID-HPCLAB/torc_lite). The TORC version distributes independent likelihood evaluations and Markov chains across MPI processes and local worker threads. It is particularly useful when each likelihood evaluation involves an expensive simulation.

The [Pi4U documentation](https://phadjido.github.io/pi4u/) provides further background and historical examples. Its installation instructions, target names, and directory structure describe an earlier version of Pi4U. The commands below follow the standalone `Makefile` in this directory.

## Contents

- [Method](#method)
- [Source files](#source-files)
- [Requirements and installation](#requirements-and-installation)
- [Build and run](#build-and-run)
- [Configuration](#configuration)
- [Defining a likelihood](#defining-a-likelihood)
- [Output and analysis](#output-and-analysis)
- [Restarting a calculation](#restarting-a-calculation)
- [Practical guidance](#practical-guidance)
- [References](#references)

## Method

Let $\theta$ denote the unknown parameters, $D$ the observations, $\pi_0(\theta)$ the prior density, and $L(\theta)=p(D\mid\theta)$ the likelihood. Bayes' theorem gives

$$
\pi(\theta\mid D)=\frac{L(\theta)\pi_0(\theta)}{Z},
\qquad
Z=\int L(\theta)\pi_0(\theta)\,d\theta.
$$

TMCMC connects the prior to the posterior through

$$
\pi_j(\theta)\propto\pi_0(\theta)L(\theta)^{p_j},
\qquad 0=p_0<p_1<\cdots<p_m=1.
$$

Each transition proceeds as follows:

1. Choose the next exponent adaptively, using the coefficient of variation of the incremental importance weights and the target `TolCOV`.
2. Compute and normalize the weights
   $$w_i=L(\theta_i)^{p_{j+1}-p_j},\qquad \bar w_i=w_i/\sum_k w_k.$$
3. Estimate a weighted population covariance and resample the current population to select chain leaders and chain lengths.
4. Run Markov chains targeting the next intermediate distribution. Independent chains can execute concurrently.
5. Repeat until the exponent reaches one, subject to the `MaxStages` limit.

The active proposal is a Gaussian random walk:

$$
\theta'=\theta+\varepsilon,\qquad
\varepsilon\sim\mathcal N(0,\texttt{bbeta}\,\Sigma_j).
$$

Here, $\Sigma_j$ is the weighted population covariance. **`bbeta` multiplies the covariance**, so `bbeta = 0.04` corresponds to a standard-deviation multiplier of $0.2$.

For an admissible proposal, the Metropolis acceptance probability is

$$
\alpha=\min\!\left[1,
\exp\!\left(\log\pi_0(\theta')-\log\pi_0(\theta)
+p_{j+1}\left[\log L(\theta')-\log L(\theta)\right]\right)\right].
$$

Proposals outside the configured bounds are rejected without a likelihood evaluation. Rejected transitions retain the current state, so repeated rows are expected in a sampled population.

The evidence estimate accumulates incremental normalizing constants:

$$
\log\widehat Z=
\sum_{j=0}^{m-1}\log\left[\frac{1}{N_j}
\sum_{i=1}^{N_j}\exp\left((p_{j+1}-p_j)\log L(\theta_i)\right)\right].
$$

The implementation evaluates these weights with an exponential shift for numerical stability. A final population should be interpreted as posterior samples only when its exponent is one. Reaching the stage limit earlier leaves an intermediate distribution.

## Source files

| File | Purpose |
| --- | --- |
| `engine_tmcmc.c` | Configuration, initialization, population transitions, task creation, chain evolution, and restart control. |
| `engine_tmcmc.h` | Shared configuration, run information, and database structures. |
| `tmcmc_stats.c` | Adaptive exponents, importance weights, resampling, covariance calculations, and prior densities. |
| `tmcmc_db.c` | Population and evaluation databases and their text output. |
| `auxil.c` | Random sampling, numerical utilities, and supporting routines. |
| `fitfun.c`, `fitfun.h` | User likelihood and initialization/finalization interface. |
| `gsl_headers.h` | GSL includes used by the implementation. |
| `tmcmc.par` | Runtime parameters. |
| `Makefile` | Serial, TORC, and OpenMP build options. |

Keep the supporting files and subdirectories from the repository together; the executable is built from several translation units.

## Requirements and installation

Required for all builds:

- A C compiler with C99 support, GNU Make, and POSIX threads.
- [GNU Scientific Library (GSL)](https://www.gnu.org/software/gsl/), including its CBLAS library.

TORC execution additionally requires an MPI installation and TORC-Lite. OpenMP execution requires a compiler and runtime with OpenMP support.

### Linux

For example, on Debian or Ubuntu with MPICH:

```bash
sudo apt install build-essential libgsl-dev mpich libmpich-dev gfortran
```

Use one consistent MPI installation for TORC, the application, and the launcher. Check `mpicc -show` and the paths reported by `command -v mpicc mpifort mpirun` when multiple installations are present.

### macOS with Homebrew

```bash
brew install gsl mpich gcc
```

The Makefile includes `/opt/homebrew/include` and `/opt/homebrew/lib`, which match a typical Apple Silicon Homebrew installation. On Intel Macs or with another installation prefix, pass the actual GSL paths as shown below.

For the optional OpenMP build, Homebrew GCC provides a convenient compiler/runtime combination. The command named `gcc` on macOS may invoke Apple Clang; explicitly choose the installed GNU compiler when needed. For MPICH, an environment setting such as `MPICH_CC=gcc-16` selects that compiler behind `mpicc`, provided that executable is installed. This setting is specific to MPICH.

### Install TORC-Lite

Install TORC separately; it is not bundled with this directory.

```bash
git clone https://github.com/CEID-HPCLAB/torc_lite.git
cd torc_lite
# Run autoreconf -i first only if the generated configure script is absent.
./configure --prefix="$HOME/.local/torc" CC=mpicc F77=mpifort
make
make install
export PATH="$HOME/.local/torc/bin:$PATH"
```

Add that `PATH` setting to your shell configuration for future sessions. Confirm that the installed helpers work:

```bash
torc_cflags
torc_libs
```

TORC uses MPI and POSIX threads. Follow its own installation and MPI thread-support guidance, and make the same installation available on all compute nodes.

## Build and run

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/inference/TMCMC
```

The executable is named `engine_tmcmc` in every build mode. **Run `make clean` before switching modes**, because changing a make variable does not itself invalidate an existing executable.

### Serial

```bash
make clean
make
./engine_tmcmc > run.log 2>&1
```

### TORC parallel execution

```bash
make clean
make use_torc=1
TORC_WORKERS=2 mpirun -np 4 ./engine_tmcmc > run.log 2>&1
```

This example requests four MPI processes and two TORC workers per process. Allow CPU resources for runtime communication as well as workers, and use the launcher and allocation rules of your cluster. Avoid CPU oversubscription by numerical libraries or simulations called from `fitfun`.

The initial population exposes one task per prior sample. Later generations expose one task per resampled chain; steps within a chain remain sequential. Each generation ends with synchronization before the next population statistics are computed. The `stealing` parameter controls optional TORC work stealing around these task batches.

All ranks need access to the configuration and any model data loaded by the likelihood. Concurrent evaluations must use distinct temporary filenames or working directories if they launch external programs.

### OpenMP

```bash
make clean
make use_omp=1
OMP_NUM_THREADS=8 ./engine_tmcmc > run.log 2>&1
```

Use a compiler with working OpenMP support, for example `make use_omp=1 CC=gcc-16` when that Homebrew compiler is installed. Treat TORC and OpenMP as separate build modes; the examples do not combine the two switches.

### Explicit compiler and library paths

Command-line assignments to `CFLAGS` or `LDFLAGS` override the Makefile assignments, including its mode-specific additions. Include all necessary flags when overriding them. For example, a TORC build using Homebrew's actual GSL prefix is:

```bash
make clean
make use_torc=1 CC=mpicc \
  CFLAGS="-O3 -std=c99 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_USE_TORC_=1 $(torc_cflags) -I$(brew --prefix gsl)/include -pthread" \
  LDFLAGS="-L$(brew --prefix gsl)/lib -lgsl -lgslcblas -lm $(torc_libs) -pthread"
```

For a serial Linux build requiring explicit pthread flags:

```bash
make clean
make CC=gcc \
  CFLAGS="-O3 -std=c99 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread" \
  LDFLAGS="-lgsl -lgslcblas -lm -pthread"
```

The supplied Makefile uses `_BSD_SOURCE`; recent glibc versions may issue a deprecation warning. `_DEFAULT_SOURCE` is its replacement on those systems.

## Configuration

The program reads `tmcmc.par` from its **current working directory**. It does not use a parameter-file argument in the examples above. Use separate run directories, with a copy of this file, to preserve independent calculations.

The supplied example uses a two-dimensional Rosenbrock target, 10,000 samples, uniform bounds `[-15,15]`, and a proposal covariance multiplier of `0.04`.

A compact configuration is:

```text
Nth 2
MaxStages 30
PopSize 10000
TolCOV 1.0
bbeta 0.04
prior_type 0
Bdef -15 15
MinChainLength 0
MaxChainLength 1000000
burn_in 0
seed 12345
load_from_file 0
use_local_cov 0
icdump 1
ifdump 0
stealing 0
restart 0
```

This example allows more stages than the supplied file; check the terminal exponent after every run.

### Main parameters

| Key | Meaning |
| --- | --- |
| `Nth` | Number of inferred parameters; must match the likelihood. |
| `PopSize` | Nominal population size at each generation. |
| `MaxStages` | Maximum number of stored generations, including the initial prior population. |
| `TolCOV` | Target coefficient of variation of incremental weights; controls tempering step size. |
| `bbeta` | Gaussian proposal covariance multiplier. |
| `Bdef lower upper` | Default bounds for all coordinates. |
| `B0 lower upper`, `B1 ...` | Coordinate-specific bounds; indices start at zero. |
| `prior_type` | Prior selection, described below. |
| `MinChainLength` | Lower limit imposed on selected chain lengths; default zero preserves resampling lengths. |
| `MaxChainLength` | Upper limit; longer chains are split into shorter tasks. Must be positive. |
| `burn_in` | Extra discarded transitions at the start of each chain. |
| `seed` | Random seed; use a fixed nonzero value for repeatability experiments. Parallel scheduling can still change the sample realization. |
| `icdump` | Write population files; keep enabled to obtain `final.txt` and restart populations. |
| `ifdump` | Write the additional evaluation database. |
| `load_from_file` | Initialize coordinates from `init_db.txt` instead of using the generated prior draws. |
| `restart` | Enable checkpoint loading and saving. |
| `stealing` | Enable TORC work stealing during task batches. |
| `use_local_cov` | Keep zero for the active global-covariance proposal implementation. |

Raising `MinChainLength` can increase the number of retained samples beyond the nominal population size. `MaxChainLength` changes task granularity and can help balance expensive likelihoods with variable runtimes. Start with `MinChainLength 0` and the supplied chain behavior before tuning either value.

The numerical exponent-selection controls are `opt.MaxIter`, `opt.Tol`, `opt.Display`, and `opt.Step`. These configure the numerical search and diagnostics; `opt.Step` is not a fixed increment for the entire tempering schedule. Leave them at the supplied settings unless diagnosing exponent selection.

Use one setting per line, place comments on separate lines beginning with `#`, and provide complete values. The parser is a legacy text parser, rather than a general configuration-file library.

### Priors

**Uniform (`prior_type 0`)** is the default and the simplest supported starting point. Specify a default box and optionally override coordinates:

```text
prior_type 0
Bdef -15 15
B0 -6 6
B1 -10 10
```

The prior normalization is included by the engine. The `prior_mu`, `prior_sigma`, and `C...` entries in the supplied file do not select a different prior while `prior_type` remains zero.

**Multivariate Gaussian (`prior_type 1`)** uses a comma-separated mean vector and a covariance matrix with comma-separated entries and semicolon-separated rows:

```text
prior_type 1
prior_mu 0.0,0.0
prior_sigma 10.0,0.0;0.0,10.0
```

Here `prior_sigma` is a **covariance matrix**, not a vector of standard deviations. Provide a symmetric positive-definite matrix of size `Nth` by `Nth`. Proposal-bound checks still apply, whereas initial Gaussian sampling is unbounded. Review this interaction before using finite bounds with a Gaussian prior; it does not automatically implement a consistently normalized truncated Gaussian model.

**Composite (`prior_type 3`)** parses independent coordinate specifications `Ci distribution parameter0 parameter1`. Distribution codes are zero for uniform, one for normal, and two for truncated normal. Bounds for truncated normals come from the `B...` settings.

For composite normal priors (distribution code `1`), the two parameters are the **mean and standard deviation**. The standard deviation must be strictly positive. Initialization uses its square as the variance, and log-density evaluation uses the standardized residual `(theta - mean) / standard_deviation`.

For example:

```text
prior_type 3
Nth 2
Bdef -100 100
C0 1 0.0 1.0
C1 1 2.0 0.5
```

This selects independent normal priors with means 0 and 2 and standard deviations 1 and 0.5, respectively. Finite proposal bounds still apply; consider the Gaussian-bound interaction described above. For truncated normals (distribution code `2`), the parameters are also the mean and standard deviation, with truncation bounds supplied through `B...`.

This standard-deviation convention is specific to composite priors: `prior_sigma` under `prior_type 1` remains a covariance matrix. `prior_type 2` is not implemented.

The supplied `posdef` and `use_proposal_cma` lines are not active configuration options in this engine's parameter reader. Also, setting `use_local_cov` does not switch the active chain loop to the alternative covariance proposal: that call is behind a disabled compilation branch. Keep `use_local_cov 0` for this implementation.

### Initial population from a file

With `load_from_file 1`, provide `init_db.txt` containing `PopSize` rows, each with

```text
theta_0 ... theta_(Nth-1) log_likelihood log_prior
```

The loader reads all these fields, but the normal initialization task reevaluates the likelihood at the loaded coordinates. Supply valid, in-support coordinates representative of the intended prior; arbitrary starting points do not preserve the standard prior-based evidence interpretation.

## Defining a likelihood

The model interface in `fitfun.c` is:

```c
void fitfun_initialize(char *name);
void fitfun_finalize(void);
double fitfun(double *x, int N, void *output, int *info);
```

Return **the natural logarithm of the likelihood**. The engine adds the prior contribution separately. Returning a raw likelihood, a negative log-likelihood, or a log-posterior changes the target incorrectly.

The active example returns the negative Rosenbrock function:

$$
\log L(\theta)=-\sum_{i=0}^{N-2}
\left[100(\theta_{i+1}-\theta_i^2)^2+(\theta_i-1)^2\right].
$$

This defines a curved synthetic target under the chosen prior. It demonstrates sampling; it is not an observation-based likelihood with a specified noise model. The alternative Gaussian and mixture examples in `fitfun.c` are selected with preprocessor definitions. Enable exactly one target and use a compatible dimension.

For observations $y_k=f(t_k;\theta)+\epsilon_k$ with independent Gaussian noise of known standard deviation $\sigma$, a log-likelihood is

$$
\log L(\theta)=-\frac{n}{2}\log(2\pi)-n\log\sigma
-\frac{1}{2\sigma^2}\sum_{k=1}^{n}\left[y_k-f(t_k;\theta)\right]^2.
$$

Keep the normalization terms when estimating evidence. If $\sigma$ is inferred, those terms also affect the posterior. Compute directly in logarithms to avoid underflow.

The historical Pi4U sampling example uses the sinusoidal model

$$f(t;\phi)=\phi_1\sin(\phi_2t+\phi_3),$$

with 100 noisy observations and parameters $(\phi_1,\phi_2,\phi_3,\sigma)$. It is a useful application template, but its older `tmcmc_theta_internal`, setup scripts, and separate prior files are not the interface of this standalone engine. Implement the log-likelihood in `fitfun.c` and express its prior through this directory's configuration.

`fitfun_initialize` and `fitfun_finalize` run on each process. The likelihood can execute concurrently on several workers, so protect mutable shared state and avoid a single shared output filename. The current wrapper passes `NULL` for `output`; do not dereference it. `info` carries task metadata and is not the parameter vector. Handle invalid model results consistently and verify how any nonfinite log-likelihoods affect the weight calculations before production runs.

## Output and analysis

All output is written relative to the working directory. Preserve `tmcmc.par`, the executable/source revision, and `run.log` with each calculation.

| File | Contents |
| --- | --- |
| `curgen_db_000.txt` | Initial population. |
| `curgen_db_001.txt`, ... | Successive sampled populations when `icdump 1`. |
| `final.txt` | Copy of the last population file when `icdump 1`; inspect the exponent to establish whether it is posterior. |
| `fitness.txt` | One scalar: the accumulated **log evidence** reported by the engine. |
| `full_db_000.txt`, ... | Additional evaluation database snapshots when `ifdump 1`. |
| `runinfo.txt` | Restart information when `restart 1`. |

Population files contain no header and have **`Nth + 2` columns**:

```text
theta_0 ... theta_(Nth-1) log_likelihood log_prior
```

Thus, for `Nth 2`, columns 1 and 2 are parameters, column 3 is log-likelihood, and column 4 is log-prior. `full_db_*.txt` instead contains `Nth` coordinates followed by log-likelihood, without the final prior column. These evaluation snapshots are not interchangeable with the retained populations: they can include evaluated proposals that were rejected.

A minimal Python inspection is:

```python
import numpy as np
import matplotlib.pyplot as plt

samples = np.loadtxt("final.txt", ndmin=2)
dim = samples.shape[1] - 2
parameters = samples[:, :dim]
log_likelihood = samples[:, dim]

if dim >= 2:
    plt.scatter(parameters[:, 0], parameters[:, 1], s=5, alpha=0.4)
    plt.xlabel("theta_0")
    plt.ylabel("theta_1")
else:
    plt.hist(parameters[:, 0], bins=50, density=True)
    plt.xlabel("theta_0")
plt.tight_layout()
plt.show()
```

Use parameter columns for marginals, credible intervals, and pair plots. A population combines multiple chains and its row order reflects execution and database updates; do not treat the file as one chronological Markov chain. Samples are dependent, so uncertainty estimates require more care than an independent-sample calculation.

Console output includes the tempering sequence (`runinfo.p`), weight coefficients of variation, unique-point counts, covariance information, and function-call counts. **`runinfo.acceptance` is computed as the fraction of unique population points**, rather than a direct count of accepted Metropolis proposals. Interpret it accordingly.

Evidence is meaningful for a completed transition to the posterior and a correctly normalized probabilistic model. `MaxStages 1` produces only the initial population and skips the normal evidence-writing path. A premature stage limit can also leave partial evidence accumulation; do not use that value as a completed model evidence.

## Restarting a calculation

Set both:

```text
restart 1
icdump 1
```

Checkpoint operation uses `runinfo.txt` together with the corresponding population database. Rerun from the same working directory with compatible dimensions, model, and parameters to resume. Keep these files together. Start a new directory for a different inference problem.

When restart mode is enabled, `EXIT_GEN` requests an exit at a generation boundary, for example:

```bash
EXIT_GEN=2 ./engine_tmcmc
```

An `exit.txt` file is another stop request checked at these boundaries. The intentional checkpoint exit can return a nonzero status. Remove the stop condition before resuming. The restart files do not save the complete random-generator state, so a resumed run is not guaranteed to reproduce an uninterrupted run bit for bit.

## Practical guidance

- Begin with the supplied low-dimensional target and a serial run before introducing MPI or a costly model.
- Verify the terminal exponent is one. Increase `MaxStages` if the run stops earlier.
- Repeat calculations with several seeds and larger populations to assess stability of posterior summaries and evidence.
- Smaller `TolCOV` generally gives more conservative transitions and can require more stages. Larger values can produce more uneven weights.
- Tune `bbeta` to the target's scale; overly large proposals lead to rejection, while overly small proposals move slowly.
- Keep the number of independent chain tasks large enough to occupy workers. Cheap likelihoods may not amortize task overhead.
- Retain repeated states in posterior statistics. Removing duplicates changes their empirical weights.
- The optional evaluation database can consume substantial memory and disk space. Keep `ifdump 0` unless those evaluations are needed.

`make clean` removes build products. **`make clear` deletes generated populations, restart information, and result files**; archive any needed results first.

## References

1. **Ching, J. and Chen, Y.-C. (2007).** Transitional Markov Chain Monte Carlo Method for Bayesian Model Updating, Model Class Selection, and Model Averaging. *Journal of Engineering Mechanics*, **133**(7), 816–832. [DOI: 10.1061/(ASCE)0733-9399(2007)133:7(816)](https://doi.org/10.1061/%28ASCE%290733-9399%282007%29133%3A7%28816%29). Original TMCMC method.

2. **Hadjidoukas, P. E., Angelikopoulos, P., Papadimitriou, C., and Koumoutsakos, P. (2015).** Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models. *Journal of Computational Physics*, **284**, 1–21. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006). Pi4U framework and parallel Bayesian inference.

3. **Hadjidoukas, P. E., Angelikopoulos, P., Kulakova, L., Papadimitriou, C., and Koumoutsakos, P. (2015).** Exploiting Task-Based Parallelism in Bayesian Uncertainty Quantification. In *Euro-Par 2015: Parallel Processing*, Lecture Notes in Computer Science, **9233**, 532–544. [DOI: 10.1007/978-3-662-48096-0_41](https://doi.org/10.1007/978-3-662-48096-0_41). Task-based parallel execution of Bayesian uncertainty quantification algorithms.

Additional resources: [Pi4U documentation](https://phadjido.github.io/pi4u/), [Pi4U repository](https://github.com/CEID-HPCLAB/pi4u), and [TORC-Lite installation and runtime documentation](https://github.com/CEID-HPCLAB/torc_lite).

## License

This implementation is distributed as part of Pi4U. Consult the repository-level license and the notices in individual source files and dependencies for the applicable terms.
