# Parallel subset simulation

This directory contains two TORC-based subset simulation drivers for exploring rare-event regions through a sequence of increasingly restrictive response thresholds. Initial function evaluations and Markov chains run in parallel, with nested tasks for coordinate-wise candidate evaluations.

The method is described in the Pi4U paper, particularly Section 2.3 (reliability and rare events), Section 4.3, and Algorithm 2. The source includes a small analytical response function for experimentation. Adapting it to a reliability problem requires specifying both the input probability distribution and the response that defines failure; see [Model integration and interpretation](#model-integration-and-interpretation).

## Why two engines?

The two filenames identify different threshold-selection strategies. They are not serial and parallel versions: both use MPI and TORC.

| Feature | `engine_ss_fixed.cpp` | `engine_ss_adaptive.cpp` |
| --- | --- | --- |
| Threshold selection | Fixed thresholds supplied by the user | Adaptive thresholds selected from the samples |
| Level configuration | `NTHRESHOLDS` and `Thresholds` | `MAXTHRESHOLDS` and `factor` |
| Initial population | Uniform draws inside the configured bounds | Attempts to load shuffled samples from `curgen_db` |
| Seeds | Samples above the next prescribed threshold, randomly reduced to at most `N_seeds` | Highest-valued fraction of samples, randomly reduced to at most `N_seeds` |
| Probability bookkeeping | Product of observed survivor fractions | Nominal values `factor^(level + 1)` |
| Sampling-density hook | Reuses the event-response function | Separate `fmodel()` and `demand()` hooks, currently calling the same `fitfun()` |
| Default chain proposals | `N_steps = 10` | `N_steps = 10` |

Use `engine_ss_fixed` when you want to explore a prescribed sequence of event thresholds. Use `engine_ss_adaptive` when you want the thresholds to follow the observed response distribution, retaining approximately the same fraction at each level. Its initial-sample loading and separate model hook provide the structure needed for a posterior-sample workflow, such as the one discussed in the paper. The shipped demo does not itself provide a distinct posterior density or physical response model.

Both engines contain `main()` and build as separate executables. They share `engine_ss.h`, `ss_db.cpp`, `auxil.cpp`, and `fitfun.c`.

## Method

For a scalar response `demand(x)` and increasing thresholds `b_0, ..., b_L`, define nested events

$$
F_\ell = \{x : \mathrm{demand}(x) > b_\ell\}.
$$

The probability identity underlying subset simulation is

$$
P(F_L) = P(F_0)\prod_{\ell=1}^{L} P(F_\ell\mid F_{\ell-1}).
$$

Instead of waiting for rare failures in a large unconditional Monte Carlo run, conditional Markov chains explore progressively narrower regions. At each level, selected samples become the seeds for the next set of chains.

In the fixed-threshold driver, the initial fraction is the number of samples above the first threshold divided by `N_init`. Subsequent fractions are the number of stored samples above the next threshold divided by the number stored at the current level.

In the adaptive driver, samples are sorted by descending response. The highest `floor(factor * sample_count)` entries are retained, and the first discarded entry's response becomes the next threshold. The driver reports nominal probability values as powers of `factor`. Ties and integer rounding can make actual survivor fractions differ from that nominal value.

The implementation creates one task per chain. Each chain attempts `N_steps` proposals, with `Nth` coordinate-wise evaluations dispatched as nested tasks. Chains are sequential internally, while different chains can run concurrently. Each proposal coordinate is drawn uniformly within `leader[index] +/- 2*sigma`, with redraws until it is strictly inside the configured bounds. Coordinate acceptance is followed by a full response evaluation and the subset constraint check.

## Requirements and build

- MPI with a C++ compiler wrapper (`mpic++`) and launcher (`mpirun` or the platform equivalent).
- An installed [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite), with `torc_cflags` and `torc_libs` in `PATH`.
- GNU Scientific Library (GSL), including the `gsl-config` helper.
- GNU Make and a C++11-or-newer compiler. `engine_ss_fixed` now uses `std::vector` for its chain buffers; `engine_ss_adaptive` still requires the compiler's variable-length array extension.
- For the adaptive engine's initial-sample workflow, the GNU `shuf` utility.

Use one consistent MPI installation to build TORC_LITE and these executables and to launch them. TORC_LITE is an external dependency; this Makefile does not build it.

From the Pi4U repository root:

```sh
cd rare/subset
command -v mpic++ mpirun torc_cflags torc_libs gsl-config
make
```

The Makefile builds both executables. To build only one:

```sh
make engine_ss_fixed
# or
make engine_ss_adaptive
```

The MPI C++ wrapper is selected through `CPP` in this Makefile, for example:

```sh
make CPP=mpicxx
```

After changing included headers, the model implementation, or MPI installations, clean and rebuild:

```sh
make clean
make
```

`make clean` removes executables and object files. `make clear` removes generated `samples*.txt`, `seeds*.txt`, and `Pc.txt`; preserve any results you need before running it.

## Configuration

Both programs read `subset.par` from the current working directory. No `subset.par` is included in this directory on the reviewed `main` branch, so create one before running. If it is absent, the engines silently retain internal defaults; these are not a suitable substitute for a problem-specific configuration.

Use whitespace-separated values, one setting per line, and put `#` in the first column for comments. Setting names are case-sensitive and parsing uses substring matching rather than a strict configuration grammar.

| Setting | Meaning |
| --- | --- |
| `Nth` | Parameter dimension; the supplied response uses two coordinates. |
| `N_init` | Number of initial samples/evaluations. |
| `N_seeds` | Maximum number of seeds used to launch chains at a level. |
| `N_steps` | Number of proposal attempts per chain, in addition to storing its seed. |
| `Bdef` | Common lower and upper coordinate bounds. |
| `B0`, `B1`, ... | Optional per-coordinate bounds, using zero-based indices. |
| `sigma` | Proposal scale; the actual uniform half-width is `2*sigma`. |
| `seed` | GSL random seed; streams are initialized per worker and rank. |
| `logval` | `0`: coordinate acceptance uses value ratios with an added `1e-6`; nonzero: it uses differences interpreted as log-density differences. Set to `1` for the current negative-valued examples. |
| `NTHRESHOLDS` | Fixed engine only: number of prescribed thresholds. |
| `Thresholds` | Fixed engine only: increasing threshold values, all on one line. |
| `factor` | Adaptive engine only: retained fraction, strictly between 0 and 1. |
| `MAXTHRESHOLDS` | Adaptive engine only: maximum number of levels, including the initial level. |

Keep configuration lines shorter than 256 bytes. For the fixed engine, supply exactly `NTHRESHOLDS` values: when a parameter file is read, the threshold array is recreated with zeros before parsing `Thresholds`, so missing values do not retain the built-in sequence. For the adaptive engine, choose population sizes and `factor` so the retained count is always at least one.

Worker count and task scheduling affect random-number consumption. The same `seed` does not guarantee identical samples across different parallel configurations.

Both engines still initialize `logval` to `0`. Override it with `logval 1` in `subset.par` for the current `fitfun.c`; negative scores are not ordinary nonnegative density values. This switch affects Metropolis acceptance only: event thresholds continue to be compared directly with the returned score, without exponentiation.

## Running the fixed-threshold engine

For a small demonstration of the supplied response, create `subset.par` with:

```text
Nth           2
N_init        10000
N_seeds       1000
N_steps       10
Bdef          -6 6
NTHRESHOLDS    10
Thresholds    -64 -32 -16 -8 -4 -2 -1 -0.5 -0.25 -0.125
sigma         0.1
logval        1
seed          280675
```

This configuration explores increasingly small regions around the two maxima of the current test function. The bounds contain both centers. It is an illustrative configuration for exploring the code, not a validated reliability benchmark.

The compiled fixed-engine defaults now also use ten thresholds `-64 / 2^i`, for `i = 0,...,9`, and ten proposal attempts per chain. If `subset.par` exists, include the full `Thresholds` line: reading a parameter file replaces the built-in threshold array with zeros before parsing it.

Run from the directory containing `subset.par`:

```sh
mpirun -np 1 ./engine_ss_fixed > ss_fixed.log 2>&1
```

For parallel execution, select the MPI rank count and TORC worker configuration appropriate to your installation, for example:

```sh
mpirun -np 4 ./engine_ss_fixed > ss_fixed.log 2>&1
```

The log includes level index `L`, threshold `T`, cumulative value `P`, timing, and function-evaluation counts. Level numbering is zero-based. Initial samples are stored at level 0; subsequent sample files represent the conditional populations generated at earlier thresholds. Files are dumped at the beginning of each chain-generation loop, so there is not necessarily a sample file for every probability entry, especially the final one.

## Running the adaptive engine

Replace the fixed-threshold settings with:

```text
factor          0.5
MAXTHRESHOLDS    8
```

Retain the common settings above, including `Nth 2`, `Bdef -6 6`, and `logval 1`. `NTHRESHOLDS` and `Thresholds` are not used by `engine_ss_adaptive`; its thresholds are selected from the samples. For the current negative score, these thresholds should progress toward zero as populations concentrate near the two maxima.

The adaptive engine executes this shell command internally:

```sh
shuf < curgen_db > init_samples.txt
```

Provide `curgen_db` with at least `N_init` rows. Each row must contain exactly `Nth` parameter values followed by one numeric value, which the input reader skips. The response is recomputed; the final column of the input is not used as the new response. For a posterior reliability calculation, the coordinates should be samples from the intended posterior or base input distribution, not merely arbitrary points.

For a format demonstration after a fixed-engine run, its level-0 sample dump can be used as input:

```sh
cp samples_000.txt curgen_db
mpirun -np 1 ./engine_ss_adaptive > ss_adaptive.log 2>&1
```

That example initializes from the fixed engine's uniform sample population; it does not turn it into posterior samples. Save the fixed-engine outputs first, or use a separate working directory, because both engines reuse the same output filenames.

Ensure `shuf` is available under that exact command name. On platforms where GNU Coreutils exposes it as `gshuf`, update the shell command in `engine_ss_adaptive.cpp` or provide an appropriate executable named `shuf` in `PATH`.

Do not rely on the intended uniform-sampling fallback: shell redirection can create an empty `init_samples.txt` even if `curgen_db` is missing or `shuf` fails. The code checks whether the output file opens, but does not check the command status, row count, or `fscanf` results.

`engine_ss_adaptive` advances until the configured level limit or another break condition. It does not accept a final failure threshold or stop automatically when a particular physical event has been reached.

## Outputs and plotting

| File | Contents |
| --- | --- |
| `samples_NNN.txt` | Stored sample coordinates and their response values for a dumped population. |
| `seeds_NNN.txt` | Selected chain seeds and response values for a level. |
| `Pc.txt` | Cumulative values reported by the engine, one numeric value per line. |
| `init_samples.txt` | Adaptive engine's shuffled copy of `curgen_db`. |

Sample and seed files have no header or labels. Columns 1 through `Nth` contain coordinates; column `Nth + 1` contains `demand(x)`. Their filename indices are padded to three digits. `Pc.txt` contains no thresholds: preserve the run log to associate its entries with events. It is deleted at startup and recreated. Sample and seed dumps overwrite files with the same names, while old dumps from longer previous runs can remain.

If the Python `display_gen.py` helper is placed in `../../display/`, plot a two-dimensional population with:

```sh
python3 ../../display/display_gen.py samples 0 2 --auto-limits
python3 ../../display/display_gen.py seeds 0 2 --auto-limits
```

For higher dimensions, specify two one-based coordinate indices:

```sh
python3 ../../display/display_gen.py samples 3 4 1 3 \
    --auto-limits --save samples_003.png --no-show
```

These commands require that helper and its NumPy/Matplotlib dependencies. The engines themselves do not perform plotting.

## Model integration and interpretation

Both drivers include `fitfun.c`, with the interface:

```cpp
double fitfun(double *x, int N, void *output, int *info);
```

The active `_USE_SUBSETFUN_` example is

$$
f(x)=-\min\left(x_0^2+x_1^2,\;(x_0-2)^2+(x_1-3)^2\right).
$$

It uses only two coordinates, so keep `Nth = 2` for this example. The maximum score is zero at `(0,0)` and `(2,3)`. The previous positive-valued analytical response remains in the source as a commented expression.

For a negative threshold `b`, the event `f(x) > b` is the union of two open disks centered at these maxima, each with radius `sqrt(-b)`, intersected with the configured box. Thus the fixed threshold sequence has a direct geometric interpretation:

| Level | Threshold | Disk radius |
| --- | --- | --- |
| 0 | -64 | 8 |
| 1 | -32 | 5.657 |
| 2 | -16 | 4 |
| 3 | -8 | 2.828 |
| 4 | -4 | 2 |
| 5 | -2 | 1.414 |
| 6 | -1 | 1 |
| 7 | -0.5 | 0.707 |
| 8 | -0.25 | 0.5 |
| 9 | -0.125 | 0.354 |

The center separation is `sqrt(13)`. The disks become disjoint when their radius is less than `sqrt(13)/2`, which occurs at threshold `-2` and later in this sequence. Small-step chains then tend to remain in their current component, so retain seeds near both centers when examining the two-peak behavior. A threshold of zero or above gives an empty event because the comparison is strict and the score never exceeds zero.

Alternative examples are selected through source-level macros:

| Macro | Returned score | Maximizers |
| --- | --- | --- |
| `_USE_SUBSETFUN_` | Negative squared distance to the nearest of two centers | `(0,0)` and `(2,3)` |
| `_USE_ROSENBROCK_` | Negative Rosenbrock function | All coordinates equal to 1, for dimension at least 2 |
| `_USE_RASTRIGIN_` | Negative Rastrigin function divided by 10 | All coordinates equal to 0 |

Enable exactly one macro and rebuild. All three scores are nonpositive and use increasing negative thresholds to approach their maxima. The same numerical thresholds define different geometries and survivor fractions for each function; tune the sequence, bounds, and proposal scale to the chosen test. Use `logval 1` with each of these score definitions.

For a physical reliability problem, distinguish:

1. The input density `p(x)`, or log density, which determines Metropolis acceptance.
2. The response `g(x)`, which determines whether `g(x) > threshold`.

`engine_ss_adaptive` provides separate `fmodel()` and `demand()` functions for these roles, but both currently call the same `fitfun()`. Replace their bodies with the corresponding problem functions. The initial population must follow the same base distribution used by the transition kernel. In `engine_ss_fixed`, separate the density and response roles explicitly before using it for a general reliability calculation.

The supplied code is a legacy demonstration and needs numerical review before its reported values are treated as validated failure probabilities:

- **Distribution consistency:** initial samples in `engine_ss_fixed` are uniform, but with `logval 1` its coordinate acceptance corresponds to a density proportional to `exp(fitfun(x))`. Those roles do not generally define the same base distribution. The current nonpositive scores are suitable as log-density scores for this acceptance rule, not as ordinary density values with `logval 0`. For `_USE_SUBSETFUN_`, `exp(-min(r1,r2))` is the maximum of two exponential kernels, not their sum and therefore not a conventional two-component Gaussian mixture.
- **Rejected transitions:** both engines store the seed and subset-accepted proposals, but omit the current state when a full candidate fails the subset constraint. Standard conditional MCMC estimators normally retain the current state on rejection. The resulting accepted-only population can change the distribution used for probability estimates.
- **Boundary proposals:** candidates are repeatedly redrawn inside the box. Near a boundary the proposal becomes truncated, and its reverse/forward density ratio is not included in acceptance. Review this behavior when implementing a kernel intended to preserve a specified density.
- **Adaptive probabilities:** `Pc.txt` from `engine_ss_adaptive` contains nominal powers of `factor`, not separately measured conditional probabilities. There is no final correction for an arbitrary user-defined failure event.
- **Storage and empty subsets:** `ss_db.cpp` reserves space for 1,000,000 samples and seeds in each database without insertion bounds checks. Keep `N_init` and the maximum stored population, up to `N_seeds * (N_steps + 1)`, within capacity. Empty populations and retained counts of zero are not consistently guarded before seed access and threshold updates.

These distinctions matter when comparing this folder with the general probability formulation in the paper. They do not change the central reason for the two drivers: prescribed versus adaptive threshold selection.

## Source files

| File | Purpose |
| --- | --- |
| `engine_ss_fixed.cpp` | Fixed-threshold driver and chain kernel. |
| `engine_ss_adaptive.cpp` | Adaptive-threshold driver, initial-sample loader, and model/response hooks. |
| `engine_ss.h` | Shared parameters and helper declarations. |
| `ss_db.cpp` | Rank-0 sample/seed databases, sorting, selection, and text output. |
| `auxil.cpp` | Per-worker random streams, function-evaluation counters, and numerical utilities. |
| `fitfun.c` | Analytical response examples and model customization point. |
| `gsl_headers.h` | Shared GSL includes. |
| `Makefile` | Builds both engines. |

## References

1. **Hadjidoukas, P. E., Angelikopoulos, P., Papadimitriou, C., and Koumoutsakos, P. (2015).** “Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models.” *Journal of Computational Physics*, **284**, 1–21. [DOI](https://doi.org/10.1016/j.jcp.2014.12.006). See the reliability formulation, Section 4.3, Algorithm 2, and the posterior subset simulation application.
2. **Au, S., and Beck, J. (2001).** “Estimation of small failure probabilities in high dimensions by subset simulation.” *Probabilistic Engineering Mechanics*, **16**, 263–277. This is the foundational subset simulation reference cited as [20] in the Pi4U paper.

See the [Pi4U repository](https://github.com/CEID-HPCLAB/pi4u) for project information and licensing.
