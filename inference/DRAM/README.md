# DRAM: Delayed Rejection Adaptive Metropolis in C

This directory contains a **C adaptation of DRAM-related MCMC routines**, based on Marko Laine's [MCMCstat MATLAB toolbox](https://github.com/mjlaine/mcmcstat) and its Gaussian and banana-shaped target examples. It is part of [Pi4U](../../README.md).

The code demonstrates Metropolis–Hastings sampling, covariance adaptation, and a two-stage delayed-rejection step. It provides standalone test programs and a replaceable `fitfun()` interface for application targets.

**This directory builds serial executables.** It does not require MATLAB, MPI, or TORC. It is a focused C adaptation, not a complete port of MCMCstat's inference, diagnostics, plotting, or prediction facilities. Numerical translation details that need validation are described below.

## Method

DRAM combines two techniques for random-walk Markov chain Monte Carlo:

- **Adaptive Metropolis (AM):** update the Gaussian proposal covariance using the history of the chain, so that proposal directions and scales better match the target distribution.
- **Delayed rejection (DR):** after a rejected proposal, try another proposal, typically with smaller scale, using a corrected acceptance probability that accounts for the rejected first attempt.

With symmetric first-stage proposals and target density $\pi$, the ordinary Metropolis acceptance probability is

$$
\alpha_1(x,y)=\min\left(1,\frac{\pi(y)}{\pi(x)}\right).
$$

For the second-stage proposal used here, the delayed-rejection correction has the form

$$
\alpha_2(x,y,z)=\min\left(1,
\frac{\pi(z)q_1(y\mid z)[1-\alpha_1(z,y)]}
     {\pi(x)q_1(y\mid x)[1-\alpha_1(x,y)]}
\right).
$$

The second Gaussian proposal is centered at the current state, with its Cholesky factor divided by `drscale`. Its covariance is therefore reduced by `drscale²`. This is not simply a second ordinary Metropolis test: the first-stage proposal-density and rejection-probability terms matter.

The intended adaptive covariance has the form

$$
Q_t=a^2\left(\widehat{\operatorname{Cov}}(X_{0:t})+\epsilon I\right),
$$

where `a = options.adascale`, normally `2.4 / sqrt(npar)`, and `epsilon = options.qcovadj`. Rejected states remain in the chain and contribute to adaptation. For the theoretical conditions and limitations of adaptive MCMC, consult the papers in the references; adaptation alone does not establish convergence for an arbitrary application.

## Executables and supplied settings

| Program | Target | Current settings |
| --- | --- | --- |
| `normaltest` | Two-dimensional Gaussian quadratic target, constructed with requested covariance condition number 10. | 10,000 stored states; adaptation every 500 states; delayed-rejection scale 20. |
| `bananatest` | Two-dimensional nonlinear transformation of a correlated Gaussian. | 100,000 stored states; **AM selected by default**, adaptation interval 10, delayed rejection disabled. |
| `generaltest` | Application target through `fitfun.c`; Rosenbrock-based energy enabled by default. | 10,000 stored states; adaptation interval 500; delayed-rejection scale 20. |

`bananatest.c` contains a `method_type` switch for MH, DR, AM, and DRAM. Its active line is `method_type method = AM`. Change that line and rebuild to compare the methods.

Each program calls `gsl_rand_init(1)`, supplying a fixed seed to the GSL generator. The dimension, initial point, bounds, run length, proposal covariance, and other settings are configured **in the C sources**, not through command-line arguments or a parameter file.

## Dependencies

- A C99 compiler, such as GCC or Clang.
- GNU Make.
- GNU Scientific Library (GSL), including its CBLAS library.
- The C math library and POSIX threads.
- Optional: Python 3, NumPy, and Matplotlib for plotting.

The implementation uses C99 variable-length arrays; compile these sources as C, not C++.

Install dependencies on Debian/Ubuntu:

```bash
sudo apt install build-essential libgsl-dev
```

On macOS with Homebrew:

```bash
brew install gsl
```

## Build and run

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/inference/DRAM
make
```

The Makefile builds `normaltest`, `bananatest`, and `generaltest`. Its `/opt/homebrew/include` and `/opt/homebrew/lib` flags support Apple Silicon Homebrew installations. Adjust these paths for another prefix.

For a Linux installation with GSL in the standard locations, an explicit build is:

```bash
make clean
make CFLAGS="-O3 -Wall -std=c99 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread" \
     LDFLAGS="-lgsl -lgslcblas -lm -pthread"
```

For a Homebrew installation at either an Intel or Apple Silicon prefix:

```bash
make clean
make CFLAGS="-O3 -Wall -std=c99 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread -I$(brew --prefix gsl)/include" \
     LDFLAGS="-L$(brew --prefix gsl)/lib -lgsl -lgslcblas -lm -pthread"
```

`-pthread` is included explicitly because the GSL RNG helpers use POSIX mutexes, even though these programs run a single sampler thread. Some systems link successfully without that extra flag; explicit compilation and linking is more portable.

Run an example directly:

```bash
./normaltest
./bananatest
./generaltest
```

No MPI launcher is needed. Running multiple copies in one directory overwrites the same output filenames; use separate working directories for separate chains.

### Source inclusion and rebuilds

Each test source includes `dramrun.c`, which itself includes `auxil.c`. The Makefile therefore compiles the test as one translation unit; do not also pass `dramrun.c` or `auxil.c` to the linker as separate objects without restructuring these inclusions.

The `generaltest` target separately compiles and links `fitfun.c`. Some included helper/header dependencies are absent from the Makefile target lists, so use `make clean` followed by `make` after changing `auxil.c`, `fitfun.h`, or shared headers.

## Target-density convention

`dramrun()` does not accept a density directly. The target is represented by two scalar functions:

```c
double ssfun(double *x, int n);
double priorfun(double *x, int n);
```

The acceptance code corresponds to the unnormalized target

$$
\pi(x)\propto
\exp\left[-\frac{\operatorname{ssfun}(x)}{2\sigma^2}
          -\frac{\operatorname{priorfun}(x)}{2}\right]
\mathbf{1}_{[\ell,u]}(x).
$$

For a Gaussian observation model, `ssfun` can be the residual sum of squares and `sigma2` the observation-error variance. A nonconstant prior can be encoded by `priorfun(x) = -2 * log_prior(x)` up to an additive constant. The examples return zero from `priorfun`, so only the target energy and box bounds affect acceptance.

Bounds are inclusive: proposals below a lower bound or above an upper bound are rejected. Provide a feasible initial point and a positive-definite initial proposal covariance.

### Application interface in generaltest

`generaltest.c` defines:

```c
double ssfun(double *x, int n)
{
    return -fitfun(x, n, NULL, NULL);
}
```

The application interface is:

```c
double fitfun(double *x, int N, void *output, int *info);
void fitfun_initialize(char *name);
void fitfun_finalize(void);
```

The initialization/finalization hooks are called by `generaltest`; the supplied hooks are empty. Both optional arguments passed to `fitfun` are `NULL`, so custom code must not dereference them.

**Check the factor of two when supplying a log target.** With `sigma2=1` and the current wrapper, returning `fitfun(x) = log_pi(x)` produces a target proportional to `exp(log_pi(x)/2)`. To sample `exp(log_pi(x))`, change the wrapper to:

```c
double ssfun(double *x, int n)
{
    return -2.0 * fitfun(x, n, NULL, NULL);
}
```

Use a zero `priorfun` if `fitfun` already includes the entire log posterior, to avoid applying the prior twice. If `fitfun` represents only a log likelihood, implement the prior separately. Keep `sigma2=1` for this direct log-density convention unless an additional tempering/scaling is intentional.

The default `fitfun.c` returns the negative Rosenbrock energy:

$$
\operatorname{fitfun}(x)=-\sum_{i=0}^{N-2}
\left[100(x_{i+1}-x_i^2)^2+(x_i-1)^2\right].
$$

With the current wrapper, `sigma2=1`, and zero prior, the intended target is proportional to `exp(-Rosenbrock/2)` within the configured bounds. This is a sampling example, not an optimization run. The other compile-time examples in `fitfun.c` return log Gaussian or log mixture densities, so their scaling must be checked with the same convention.

## Sampler settings

Edit the test driver's setup before `dramrun(npar)`:

| Setting | Role |
| --- | --- |
| `npar` | Number of parameters. The supplied examples and output writer are two-dimensional. |
| `params.par0` | Initial chain state; provide `npar` values. |
| `params.lbounds`, `params.ubounds` | Per-parameter support bounds. |
| `params.sigma2` | Fixed positive target scaling / Gaussian error variance. |
| `params.filename` | Output chain filename. |
| `options.nsimu` | Number of stored states, including the initial state. |
| `options.qcov` | Initial proposal covariance, in row-major `npar * npar` storage. |
| `options.adaptint` | Positive adaptation interval; set to 0 to disable adaptation. |
| `options.adascale` | Multiplier applied to an adapted Cholesky factor; covariance scaling is its square. |
| `options.qcovadj` | Diagonal covariance regularization; the example initializer uses `1e-5`. |
| `options.drscale` | Second-proposal shrink factor; set to 0 to disable delayed rejection. Values greater than 1 give a smaller second proposal. |
| `options.printint` | Positive progress-print interval. |
| `options.verbosity` | Controls adaptation messages. |

Enable methods by choosing:

| Method | `adaptint` | `drscale` |
| --- | --- | --- |
| Metropolis–Hastings | 0 | 0 |
| Delayed rejection | 0 | Positive, normally greater than 1 |
| Adaptive Metropolis | Positive | 0 |
| DRAM | Positive | Positive, normally greater than 1 |

The `params.n0` and `params.n` fields remain from the MATLAB variance-update interface. **The variance-sampling statements are commented out in this C implementation:** do not expect Gibbs updates of `sigma2`, an output variance chain, or the full MATLAB results structure.

## Output

| Program | Chain file |
| --- | --- |
| `normaltest` | `normaltest_chain.txt` |
| `bananatest` | `bananatest_chain.txt` |
| `generaltest` | `generaltest_chain.txt` |

Each file has `nsimu` rows and **two parameter columns**, with no header. The first row is the initial point; subsequent rows are the states after accepting or rejecting proposals. Repeated rows after rejection are an essential part of the Markov chain and must not be removed.

The current writer always outputs coordinates 0 and 1. Increasing `npar` alone does not export all coordinates, and `npar=1` would access a nonexistent second coordinate. Update the writer and all example-specific arrays/functions before changing dimensionality. The banana transform is explicitly two-dimensional.

The program prints progress, an integer acceptance percentage, and empirical means/covariances. Those summaries include the stored chain history without an automatic burn-in discard. They are not convergence diagnostics or uncertainty estimates. Output uses `%f`, so increase text precision if needed for subsequent analysis.

`make clean` removes the three executables. `make clear` deletes files matching `*test_chain.txt` in the working directory; archive chain results before using it.

## Plot and inspect a chain

The separate Pi4U `plot_columns.py` helper can plot the two coordinates directly:

```bash
python3 plot_columns.py generaltest_chain.txt --columns 1 2 \
    --labels "Parameter 1" "Parameter 2"
```

It opens a scatter plot by default. The following self-contained Python example additionally plots coordinate traces and discards an illustrative burn-in fraction:

```python
import numpy as np
import matplotlib.pyplot as plt

chain = np.loadtxt("generaltest_chain.txt", ndmin=2)
burn = len(chain) // 5  # Illustrative choice; assess burn-in for your problem.
samples = chain[burn:]

fig, axes = plt.subplots(1, 3, figsize=(13, 4), layout="constrained")
for j in range(2):
    axes[j].plot(chain[:, j], linewidth=0.5)
    axes[j].axvline(burn, color="red", linestyle="--")
    axes[j].set(xlabel="Stored state", ylabel=f"Parameter {j + 1}")
axes[2].scatter(samples[:, 0], samples[:, 1], s=3, alpha=0.2)
axes[2].set(xlabel="Parameter 1", ylabel="Parameter 2")
plt.show()
```

For inference, examine multiple dispersed chains, trace plots, effective sample sizes, Monte Carlo standard errors, and convergence diagnostics. A visually plausible scatter plot or a high acceptance rate does not establish correct stationary sampling.

## Numerical implementation notes

This is a legacy C translation. The following source details should be reviewed and validated against the MATLAB reference before relying on posterior estimates:

1. **Correlated proposal construction:** the proposal loops draw a fresh Gaussian value for each matrix element while accumulating each coordinate. A multivariate Gaussian proposal should draw one shared standard-normal vector and multiply it by the Cholesky factor with consistent row/column conventions. The current loops do not reproduce the intended general correlated covariance.
2. **Delayed-rejection proposal ratio:** the `term1` and `term2` loops overwrite an element inside the inner loop instead of accumulating a matrix-vector product. Their contractions also need to be checked against the MATLAB row-vector expression. This affects the second-stage correction when DR is enabled.
3. **Covariance-construction example:** `compare()` in the example drivers converts a floating-point difference to `int`, which can report equality for unequal values. It is not a reliable general comparator for sorting eigenvalues in `covcond()`.
4. **Generalization and robustness:** the chain writer is fixed to two coordinates; several arrays are stack-allocated with runtime sizes; allocation/file-operation checks and some numerical failure handling are limited. Large dimensions or long chains require additional care. The full chain is stored in memory using approximately `8 * nsimu * npar` bytes, excluding work arrays.

These points describe this C implementation, not defects attributed to the original MCMCstat toolbox or DRAM theory. They also mean that the target formulas described above express the intended acceptance model; the current implementation should not be treated as a validated reference sampler. Correcting the proposal and DR ratio should be accompanied by checks against known Gaussian moments and the upstream examples.

## Parallelism

The active sampler advances one chain sequentially. The helper file contains optional TORC-related code and a serial `torc_node_id()` stub, but the Makefile does not enable a task-parallel DRAM implementation.

Independent chains can be launched as separate processes after configuring distinct seeds and output paths. Launching identical binaries with their unchanged fixed seed repeats the same random stream. Such a multi-chain launcher, seed interface, and output management are not supplied here. Parallelizing a single chain requires preserving its transition kernel; simply evaluating dependent first- and second-stage decisions concurrently is not an equivalent replacement.

## Relationship to MCMCstat and source map

The upstream [MCMCstat README](https://github.com/mjlaine/mcmcstat/blob/master/README.org) describes a MATLAB toolbox for Bayesian model analysis using Metropolis–Hastings chains and adaptive Gaussian proposals. This directory translates selected routines and examples into C, using GSL for random numbers and linear algebra, and adds the Pi4U `fitfun` connection.

| File | Responsibility |
| --- | --- |
| `dramrun.c` | MCMC loop, delayed-rejection acceptance, adaptation, covariance helpers and chain output. |
| `normaltest.c` | Gaussian setup and quadratic target. |
| `bananatest.c` | Banana transform, target and MH/DR/AM/DRAM method selection. |
| `generaltest.c` | Generic application driver and adapter from `fitfun` to `ssfun`. |
| `fitfun.c`, `fitfun.h` | Application target and initialization/finalization interface. |
| `auxil.c` | GSL RNG helpers, statistical and matrix utilities. |
| `gsl_headers.h` | GSL includes. |

The MATLAB toolbox is useful both as the conceptual source and as a comparison implementation. Its full set of options and outputs should not be assumed to exist in this C adaptation.

## References and credits

1. H. Haario, M. Laine, A. Mira, and E. Saksman. **DRAM: Efficient adaptive MCMC.** *Statistics and Computing*, 16, 339–354, 2006. [DOI: 10.1007/s11222-006-9438-0](https://doi.org/10.1007/s11222-006-9438-0).
2. H. Haario, E. Saksman, and J. Tamminen. **An adaptive Metropolis algorithm.** *Bernoulli*, 7(2), 223–242, 2001. [DOI: 10.2307/3318737](https://doi.org/10.2307/3318737).
3. Marko Laine. **MCMCstat: MCMC toolbox for MATLAB.** [Source repository](https://github.com/mjlaine/mcmcstat) · [Documentation](https://mjlaine.github.io/mcmcstat/).
4. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).

Credit **Marko Laine and the MCMCstat contributors** for the upstream toolbox and translated routines, and **Panagiotis E. Hadjidoukas** for the Pi4U C adaptation and integration. Preserve the original source notices. Upstream MCMCstat provides an [MIT license](https://github.com/mjlaine/mcmcstat/blob/master/LICENSE.txt); consult it together with the Pi4U notices and GSL's separate licensing terms when distributing the combined software.
