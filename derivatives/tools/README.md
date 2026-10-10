# Parallel numerical differentiation tools

This directory contains three standalone Pi4U programs for estimating derivatives of a scalar function. They evaluate the gradient, and in two cases the Hessian, at a point supplied in `grad.par`. Function evaluations are distributed through [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite).

The tools support experiments with finite-difference accuracy, step-size selection, and simultaneous perturbation estimates. They evaluate derivatives at a fixed point; they do not run an optimization algorithm.

## Programs

| Program | Method | Results |
| --- | --- | --- |
| `fd_deriv` | PNDL finite differences with user-specified steps and bounds | Gradient, Hessian, eigenvalues, and optional positive-definite correction |
| `fd_grad` | PNDL gradients over a decreasing sequence of steps, followed by Romberg extrapolation | Selected derivative estimate, estimated error, and step size for each component |
| `sa_deriv` | Averaged simultaneous perturbation estimates using random sign vectors | Estimated gradient and a regularized positive-definite transformation of the estimated Hessian |

Use `fd_deriv` for direct gradient and Hessian evaluation, `fd_grad` to examine step-size sensitivity, and `sa_deriv` to experiment with stochastic derivative estimation. See the implementation notes below before relying on stochastic results.

## Requirements

- An MPI installation with C and Fortran compiler wrappers (`mpicc`, `mpif77`, and `mpif90`) and an MPI launcher.
- An installed TORC_LITE library, with `torc_cflags` and `torc_libs` available in `PATH`.
- GNU Scientific Library (GSL) headers and libraries, including GSL CBLAS.
- A C99 compiler, a Fortran compiler, GNU Make, and POSIX threads.
- The sibling [PNDL library](../pndl/) for `fd_deriv` and `fd_grad`.
- Autoconf and Automake if regenerating the PNDL build files.

Use the same MPI installation to build TORC_LITE, PNDL, and these programs, and to launch them. Install TORC_LITE separately using its repository instructions; it is not built by this directory's Makefile.

Check the helper commands before building:

```sh
command -v mpicc mpif77 mpif90 mpirun torc_cflags torc_libs
torc_cflags
torc_libs
```

## Build

From the Pi4U repository root, build PNDL first. The following commands use GCC/GFortran-compatible options and build the library without requiring the PNDL test programs:

```sh
cd derivatives/pndl
autoreconf -fi
./configure CC=mpicc F77=mpif77 \
    CFLAGS="-O2 -fno-lto" FFLAGS="-O2 -fno-lto"
make -C src

cd ../tools
make
```

The tools expect the archive at `../pndl/src/libpndl.a`; installing PNDL elsewhere does not replace this build-tree dependency. After changing MPI installations, clean and rebuild TORC_LITE, PNDL, and the tools.

To build only the simultaneous perturbation program, PNDL is not required:

```sh
cd derivatives/tools
make sa_deriv
```

The Makefile compiles C sources with `mpicc`, links `sa_deriv` with `mpicc`, and links the PNDL-based programs with `mpif90`. It disables LTO and defines `_XOPEN_SOURCE=700` to expose the `erand48` declaration. Compiler wrappers can be overridden, for example:

```sh
make CC=mpicc MPIF90=mpifort
```

After modifying included implementation files or headers, use a clean rebuild because the Makefile does not track every include dependency:

```sh
make clean
make
```

`make clean` also removes `sHbarbar.txt`.

## Quick start

Run the programs from a directory containing `grad.par`. They read this fixed filename from the current working directory; there is no application option for selecting another parameter file.

For the default bivariate Gaussian objective, a compact configuration is:

```text
# Dimension and evaluation point
Nth         2
theta       0.5 0.5
Bdef        -4 4

# Finite differences
order       2
Hdef        1e-4
diffstep    1e-4 1e-4
posdef      -1

# Gradient step-size scan
initstep    0.1
stepratio   1.5
maxsteps    100

# Simultaneous perturbation estimates
reps        1000000
seed        280675
```

This example sets `posdef -1` to preserve the raw finite-difference Hessian without requesting an additional correction. The distributed `grad.par` instead sets `posdef 0`.

Start with a single MPI rank:

```sh
mpirun -np 1 ./fd_deriv
mpirun -np 1 ./fd_grad
```

For parallel execution, use the MPI launcher and the worker configuration supported by your TORC_LITE installation. For example:

```sh
mpirun -np 4 ./fd_deriv > fd_deriv.log 2>&1
mpirun -np 4 ./fd_grad > fd_grad.log 2>&1
```

After addressing the RNG initialization issue described below, run the stochastic tool similarly:

```sh
mpirun -np 4 ./sa_deriv > sa_deriv.log 2>&1
```

MPI ranks and TORC workers are distinct: `sa_deriv` divides its repetitions by the total TORC worker count. On multiple nodes, make the executable, required libraries, and the same `grad.par` available to all ranks.

## Configuration reference

Use one setting per line, whitespace-separated values, and `#` in the first column for comments. Supply exactly `Nth` entries for vector settings. The shipped example contains extra entries in some vectors; the parser uses only the first `Nth`.

| Setting | Used by | Meaning |
| --- | --- | --- |
| `Nth` | All | Number of variables; must be positive and match the objective. The default objective requires 2. |
| `theta` | All | Point at which derivatives are evaluated. |
| `Bdef` | `fd_deriv`, `fd_grad` | Common lower and upper bounds for PNDL. |
| `lb`, `ub` | `fd_deriv`, `fd_grad` | Optional per-variable bounds overriding `Bdef`. |
| `order` | `fd_deriv`, `fd_grad` | PNDL finite-difference accuracy order. Use 2 for the example and the central-difference extrapolation in `fd_grad`. |
| `Hdef` | `fd_deriv`, `sa_deriv` | Default finite-difference step for `fd_deriv`; common perturbation magnitude for `sa_deriv`. |
| `diffstep` | `fd_deriv` | Per-variable steps overriding `Hdef`. |
| `posdef` | `fd_deriv` | `-1` disables correction; 0–3 select correction routines in `posdef.c` when the Hessian is not positive definite. |
| `initstep` | `fd_grad` | Initial step in the geometric sequence. |
| `stepratio` | `fd_grad` | Step reduction ratio; choose a value greater than 1. |
| `maxsteps` | Parsed by `fd_grad` | Currently unused by the step-generation loop; its compiled limit is `MAXGRADS = 100`. |
| `reps` | `sa_deriv` | Requested number of simultaneous perturbation repetitions. |
| `seed` | `sa_deriv` | Intended random seed; the current initialization needs correction before it provides reliable seeded behavior. |

`sa_deriv` parses bounds, `diffstep`, `order`, and `posdef`, but these do not control its estimator: it uses `Hdef`, does not enforce bounds, and always applies the Hessian transformation described below.

Always provide `grad.par`. If it is missing, the programs silently retain internal defaults, including a dimension of 4, which is incompatible with the default bivariate objective. In `sa_deriv`, missing `reps` also leaves no valid averaging workload.

## Methods and output

### Finite-difference gradient and Hessian: `fd_deriv`

The program calls `c_pndlga` for the gradient and `c_pndlhfa` for the Hessian. PNDL receives the evaluation point, bounds, steps, and requested order. If `order` is 4, this driver changes it to 2 for the Hessian calculation.

Standard output includes timings, the gradient, the raw Hessian, its eigenvalues, and function-evaluation counts. With correction enabled, a non-positive-definite Hessian is additionally transformed and printed as `mat2`, together with its eigenvalues. This additional matrix is not the original Hessian.

### Gradient extrapolation: `fd_grad`

The program schedules gradients at steps `h[k] = initstep * stepratio^(-k)`. It stops before a step below `1e-6`, or after 100 steps. Each component is extrapolated using sliding windows of five estimates and error terms with powers 2, 4, and 6. The result with the smallest estimated error is selected independently for each component.

Each component produces a line of the form:

```text
0: best:{errest,stepsize,derest} = {...,...,...}
```

The leading index is zero-based. `errest` is the extrapolation error estimate, `stepsize` is the initial step of the selected window, and `derest` is the selected derivative estimate. At least five generated steps are required. The even-power extrapolation assumes central-difference behavior, so use an interior point with suitable bounds and `order 2`.

### Simultaneous perturbation estimates: `sa_deriv`

Each repetition draws two random sign vectors, evaluates the objective six times, estimates the gradient and a symmetric Hessian, and accumulates the results. TORC tasks average their contributions across workers and MPI ranks.

If `W` is the total worker count, the actual repetition count is `W * floor(reps / W)`. Choose `reps >= W`, preferably divisible by `W`; otherwise the remainder is discarded, and `reps < W` leads to division by zero during averaging.

Let `Hbar` denote the averaged raw Hessian estimate. The matrix printed under `HESSIAN MATRIX` and written to `sHbarbar.txt` is:

$$
S = \left(\overline{H}\,\overline{H} + 10^{-6} I\right)^{1/2}.
$$

This is a regularized positive-definite curvature matrix, not the signed Hessian. The file contains an `Nth` by `Nth` whitespace-separated matrix without a header and is overwritten on each run. The program also prints the averaged gradient, eigenvalues of `S`, and function-evaluation counts.

## Theory and related MATLAB implementations

The estimator in `sa_deriv` uses simultaneous random perturbations, following the principles of second-order simultaneous perturbation stochastic approximation (SPSA). This program averages derivatives at a fixed point; it does not implement a complete SPSA optimization loop.

Let `theta` be the evaluation point and `c = Hdef`. Draw independent random sign vectors `Delta` and `Delta_tilde`, with each component equally likely to be -1 or +1. The gradient estimate is

$$
\widehat g_i = \frac{f(\theta+c\Delta)-f(\theta-c\Delta)}{2c\Delta_i}.
$$

For the Hessian, form the mixed central difference

$$
q = \frac{f(\theta+c\Delta+c\widetilde\Delta)
-f(\theta+c\Delta-c\widetilde\Delta)
-f(\theta-c\Delta+c\widetilde\Delta)
+f(\theta-c\Delta-c\widetilde\Delta)}{4c^2}.
$$

The symmetric Hessian estimate used by the implementation can be written as

$$
\widehat H = \frac{q}{2}
\left(\Delta\widetilde\Delta^{\mathsf T}
+\widetilde\Delta\Delta^{\mathsf T}\right).
$$

For sufficiently smooth objectives, these estimates have expectation equal to the corresponding derivatives up to truncation bias of order `c^2`. They are unbiased for quadratic objectives in exact arithmetic. Averaging independent repetitions reduces sampling error approximately as the inverse square root of the repetition count. Smaller perturbations reduce truncation bias but amplify cancellation and evaluation noise, especially for the Hessian. The six function evaluations per repetition are independent of dimension; Hessian storage and accumulation still require quadratic work in the dimension.

The main theoretical reference is:

1. **Spall, J. C. (2000).** “Adaptive Stochastic Approximation by the Simultaneous Perturbation Method.” *IEEE Transactions on Automatic Control*, **45**(10), 1839–1853. [DOI](https://doi.org/10.1109/TAC.2000.880982) · [Full text](https://www.jhuapl.edu/spsa/pdf-spsa/spall_tac00.pdf).

A closely related MATLAB implementation appears in:

2. **Miranda Flores, A. K. (2008).** *Stochastic Perturbation Methods for Robust Optimization of Simulation Experiments*. MSc thesis, Pennsylvania State University. Appendix B, “Matlab Code to Simulate the Adaptive SPSA Algorithm,” pp. 73–75. [Full text](https://etda.libraries.psu.edu/files/final_submissions/580).

The MATLAB appendix shares distinctive variable names, averaging and symmetrization steps, comments, and the `sqrtm(Hbar*Hbar + regularization)` transformation with this code. It uses four function evaluations per repetition, whereas `sa_deriv` uses six with central differences for the additional gradient estimates. 

## Objective function

All three programs include [fitfun.c](fitfun.c), which defines:

```c
double fitfun(double *x, int N, void *output, int *info);
```

`x` contains the `N` variables. The current drivers pass `NULL` for `output` and `info`. Replace the function body with your scalar objective, keep it safe for concurrent calls, and rebuild the programs. Return the objective whose derivatives you actually want: the code does not automatically negate it or take its logarithm.

The supplied source selects examples using macros in `fitfun.c`. Enable exactly one:

| Macro | Objective |
| --- | --- |
| `_USE_BVNPDF_` | Default: log density of a two-dimensional standard Gaussian. |
| `_USE_ROSENBROCK_` | Negative Rosenbrock function. |
| `_USE_MIXED_BVNPDF_` | Log of the sum of two bivariate Gaussian densities centered at `(-5,-5)` and `(5,5)`. |
| `_USE_MIXED_MVNPDF_` | Multivariate counterpart with component means at all-minus-five and all-plus-five vectors. |

The mixture examples sum densities without the factor `1/2`; this shifts the log objective by a constant and does not change its derivatives. The optional multivariate example may require adding a visible declaration of `mvnpdf` before its use under modern C compilers.

For the default objective:

$$
f(x) = -\log(2\pi) - \tfrac12(x_0^2+x_1^2),\qquad
\nabla f(x) = -x,\qquad \nabla^2 f(x) = -I_2.
$$

At `theta 0.5 0.5`, the reference gradient is `(-0.5, -0.5)` and the raw Hessian has diagonal entries `-1` and zero off-diagonal entries. These are analytic reference values for checking a build, not recorded test results. The stochastic program's transformed matrix should instead approach `sqrt(1.000001) * I` as its raw estimate approaches `-I`.

## Current implementation notes

- **Stochastic RNG initialization:** in `sa_deriv.c`, `dr48_buffer[0]` and `[1]` are uninitialized for a nonzero seed. For a zero seed, the time-based value assigned to `[2]` is subsequently overwritten. Correct this initialization before using `sa_deriv` for numerical comparisons or reproducibility studies.
- **Bounds in stochastic estimation:** perturbations are not clipped or checked. The objective must be defined at all evaluated points, including combined perturbations that can move a coordinate by `2 * Hdef`.
- **Artificial noise:** all three sources define `NOISE 0`. Changing it to a nonzero value enables additive uniform noise in the wrapper; this is a source-level switch, not a `grad.par` setting.
- **Error handling:** the finite-difference drivers receive PNDL's `IERR` but do not check it. Configuration parsing also performs little validation. Check dimensions, bounds, steps, and numerical output when changing the example.
- **Density underflow:** the Gaussian examples compute a density and then take its logarithm. Far into the tails, this can produce `-inf`; a direct log-density implementation is preferable for such evaluations.

## Files

| File | Purpose |
| --- | --- |
| `fd_deriv.c` | PNDL gradient and Hessian driver. |
| `fd_grad.c` | PNDL step scan and Romberg extrapolation. |
| `sa_deriv.c` | Simultaneous perturbation estimator and matrix square root. |
| `fitfun.c` | Shared objective examples and customization point. |
| `grad.par` | Runtime configuration. |
| `auxil.c` | Function-evaluation counters, output helpers, and probability utilities. |
| `posdef.c` | Eigenvalue diagnostics and positive-definite matrix corrections. |
| `gsl_headers.h` | Shared GSL includes and declarations. |
| `Makefile` | Builds the three executables. |

See the [Pi4U repository](https://github.com/CEID-HPCLAB/pi4u) for project information and licensing, and [PNDL](../pndl/) for the underlying finite-difference library.
