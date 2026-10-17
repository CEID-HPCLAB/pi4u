# Parallel model evaluation tools

This directory provides two small [Pi4U](https://github.com/CEID-HPCLAB/pi4u) utilities for evaluating a scalar model with [TORC-Lite](https://github.com/CEID-HPCLAB/torc_lite):

- **`propagation_tool`** evaluates a collection of parameter vectors and compares the results against reference values.
- **`sensitivity_tool`** evaluates one-at-a-time parameter perturbations around a reference point.

Both tools create independent TORC tasks for model evaluations. They can distribute expensive simulations across MPI processes and worker threads. The model is supplied in `fitfun.c`.

## Files

| File | Purpose |
| --- | --- |
| `propagation_tool.c` | Parallel evaluation and reference comparison for a list of points. |
| `sensitivity_tool.c` | Parallel one-at-a-time parameter sweep. |
| `fitfun.c` | Shared scalar model implementation, included directly by each tool. |
| `points.txt` | Example parameter vectors and reference values for propagation. |
| `reference_point.txt` | Example reference vector for sensitivity analysis. |
| `Makefile` | Builds both executables using `mpicc` and the TORC helper scripts. |

## Requirements

- A C99 compiler, GNU Make, and POSIX threads.
- An MPI implementation, such as MPICH or Open MPI.
- TORC-Lite, installed separately, with `torc_cflags` and `torc_libs` available in `PATH`.

The supplied model uses only the C mathematics library. GSL is not required by the active code; the GSL linker settings in the Makefile are commented out.

### Install TORC-Lite

Use the same MPI installation to build TORC, compile the tools, and launch the executables.

```bash
git clone https://github.com/CEID-HPCLAB/torc_lite.git
cd torc_lite
# Run autoreconf -i first if the generated configure script is absent.
./configure --prefix="$HOME/.local/torc" CC=mpicc F77=mpifort
make
make install
export PATH="$HOME/.local/torc/bin:$PATH"
```

Add the `PATH` setting to your shell configuration if needed. Check the installation with:

```bash
torc_cflags
torc_libs
```

See the [TORC-Lite documentation](https://github.com/CEID-HPCLAB/torc_lite) for installation options and MPI thread-support requirements.

## Build

From the Pi4U repository root:

```bash
cd tools
make
```

This builds `propagation_tool` and `sensitivity_tool`. Individual targets are also available:

```bash
make propagation_tool
make sensitivity_tool
```

Both source files include `fitfun.c` directly. Do not also link a separately compiled `fitfun.o`, as that would duplicate its definitions. After changing the model, `make` rebuilds the affected tools.

To remove executables and object files:

```bash
make clean
```

## Propagation and reference comparison

```bash
TORC_WORKERS=2 mpirun -np 4 ./propagation_tool points.txt > propagation.log 2>&1
```

The filename is optional; it defaults to `points.txt` in the current working directory. A single-process run is useful for an initial check:

```bash
TORC_WORKERS=1 mpirun -np 1 ./propagation_tool
```

### Input format

`PROBDIM` is currently set to **2** in `propagation_tool.c`. Each row must contain at least `PROBDIM + 1` whitespace-separated numeric fields:

```text
x0 x1 reference_value
```

The first `PROBDIM` fields are coordinates. The **next field is the reference value**; any later fields are ignored. For example:

```text
2.7055789275729234 7.5616974498854006 -8.7431622689022728
```

The supplied `points.txt` has four columns: two coordinates, a reference value, and an additional value that the tool ignores. This also permits two-dimensional TMCMC-style population rows containing coordinates, log-likelihood, and log-prior, provided that the reference log-likelihood matches the propagation model's sign and definition.

The reader accepts at most **4096 valid rows** (`MAXPOINTS`). Rows missing coordinates or the reference field are skipped with a diagnostic. There is no header or comment syntax, and conversion uses `atof`, which does not validate malformed numeric tokens. Supply clean numeric data. The input buffer is 2048 bytes, so keep each row shorter than that limit.

### Evaluation and output

The active task wrapper computes:

```c
result = -fitfun(x, n, NULL, info);
```

After all tasks finish, the tool prints rows of the form:

```text
RESULT index: x0 x1 reference computed absolute_error
```

The error is `fabs(reference - computed)`. Input and task diagnostics are printed too; task messages may arrive in a different order, while the final result loop follows the accepted input-row order. The result index starts at zero.

The current result formatter explicitly prints two coordinates. If you increase `PROBDIM`, update this formatter as well as the input data and model.

The tool does not automatically write a separate numeric result file or compute propagated means, quantiles, or probability distributions. Its evaluations can serve as a starting point for uncertainty propagation, but sampling and statistical postprocessing are external to this utility.

## One-at-a-time sensitivity sweep

```bash
TORC_WORKERS=2 mpirun -np 4 ./sensitivity_tool reference_point.txt > sensitivity.log 2>&1
```

The filename is optional and defaults to `reference_point.txt`. The file must contain `PROBDIM` numeric coordinates; the supplied example is:

```text
1.01 0.99
```

The dimension and number of perturbations are compile-time settings in `sensitivity_tool.c`:

```c
#define PROBDIM 2
#define NRUNS 5
```

For each coordinate, the tool keeps all other coordinates fixed and evaluates:

$$
x_i^{(-j)}=(1-0.01j)x_i^{\mathrm{ref}},\qquad
x_i^{(+j)}=(1+0.01j)x_i^{\mathrm{ref}},\qquad
j=1,\ldots,\texttt{NRUNS}.
$$

With the current settings, this gives perturbation factors from 0.95 to 0.99 and from 1.01 to 1.05. There are **20 perturbed evaluations plus one reference evaluation**, for 21 model calls in total. In general, the count is `1 + 2 * PROBDIM * NRUNS`.

Unlike propagation, the sensitivity wrapper returns **`fitfun` without negation**. Final output has the form:

```text
RESULT ( x0 x1 ) = model_value
```

The reference point is printed, but its computed model value is not included in the final result table. The sweep reports model values, rather than derivatives, normalized sensitivity indices, or Sobol indices. It does not account for parameter interactions.

Percentage perturbations leave a zero-valued reference coordinate unchanged. For negative coordinates, the minus-percentage branch moves toward zero rather than toward a smaller numerical value. Consider additive steps or parameter-specific scales when relative perturbations do not suit the model. No bounds are enforced on perturbed points.

## Model customization

The shared interface is:

```c
double fitfun(double *x, int N, void *output, int *info);
```

The active example is the **positive Rosenbrock function**:

$$
f(x)=\sum_{i=0}^{N-2}\left[100(x_{i+1}-x_i^2)^2+(x_i-1)^2\right].
$$

Consequently, propagation currently returns negative Rosenbrock values, while sensitivity returns positive Rosenbrock values. Match any reference dataset to this convention.

`fitfun.c` also contains selectable Rastrigin and two-dimensional `subsetfun` examples. Enable exactly one of `_USE_ROSENBROCK_`, `_USE_RASTRIGIN_`, or `_USE_SUBSETFUN_`, or replace the implementation with your own scalar model. The subset example accesses the first two coordinates explicitly.

When adapting the tools:

1. Set `PROBDIM` in each tool to match the model and input files, then rebuild.
2. Decide whether propagation should retain the leading minus sign.
3. Keep the model thread-safe, since TORC can invoke it concurrently.
4. Provide required model files on every MPI process and use distinct output paths for concurrent external simulations.

The active wrappers pass `NULL` as `output`, so the model must not dereference that argument. The four-element `info` array contains task metadata; its final entry identifies the input row or perturbation, and the sensitivity reference task uses `-1` there.

`propagation_tool.c` contains a disabled example for writing 12 model observables to `propagation.txt`. This branch is a customization sketch: the supplied model does not populate those observables, and its pthread mutex does not coordinate separate MPI processes writing the same file. Adapt model output and result collection before enabling it.

## Further information

- [Pi4U repository](https://github.com/CEID-HPCLAB/pi4u)
- [Pi4U documentation](https://phadjido.github.io/pi4u/)
- [TORC-Lite](https://github.com/CEID-HPCLAB/torc_lite)

For the Pi4U framework, see P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos, **Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models**, *Journal of Computational Physics* 284 (2015), 1–21. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).

## License

These tools are part of Pi4U. See the repository-level license and the notices in the source files for the applicable terms.
