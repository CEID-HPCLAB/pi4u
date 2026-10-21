# External Model Coupling Examples

This directory demonstrates how to use **Pi4U** with external model-evaluation programs. The simulation or model does not need to use TORC or MPI: Pi4U can invoke an existing executable or script, supply its input parameters, and read back the computed result.

The examples use a **file-based coupling interface**. Each model evaluation is performed in a separate working directory, so independent evaluations can run concurrently without overwriting one another's files.

## Examples

| Directory | Description | External model | Output consumed by Pi4U |
| --- | --- | --- | --- |
| [`rosenbrock/`](rosenbrock/) | A small TORC driver dispatches six independent evaluations of the Rosenbrock function. | C executable (`simcode`) invoked through `run.sh` | `fitness` (objective-function value) |
| [`simple_model/`](simple_model/) | A TMCMC run estimates the parameters of a sinusoidal model using an external model and likelihood calculation. | Python scripts (`my_model.py` and `log_like.py`) invoked through `doall.sh` | `loglike.txt` (log-likelihood value) |

## How coupling works

For each evaluation, the Pi4U-side coupling function generally:

1. Creates a working directory for the evaluation.
2. Copies the external model's executable, scripts, and any required input data into that directory.
3. Writes the candidate parameter values to a file.
4. Starts the external program using `fork()` and `execvp()`.
5. Waits for the program to complete and reads a scalar result from an output file.
6. Returns that result to the Pi4U algorithm or TORC task.

This separation makes it possible to couple models written in C, C++, Fortran, Python, or other languages, provided they can be launched as external programs and follow the chosen input/output convention. The examples illustrate two conventions, not a single mandatory Pi4U file format.

## Requirements

- A POSIX-like environment (Linux or macOS) with `make` and C development tools.
- An MPI implementation and a working **TORC** installation for the TORC-based builds. The helper commands `torc_cflags` and `torc_libs` must be available in `PATH`.
- **GNU Scientific Library (GSL)** to build the TMCMC example.
- **Python**, **NumPy**, and **SciPy** for the supplied `simple_model` scripts. The shell script currently invokes the interpreter as `python`.

The TMCMC Makefile contains `/opt/homebrew/include` and `/opt/homebrew/lib` paths. On systems where GSL is installed elsewhere, adjust the relevant `CFLAGS` and `LDFLAGS` (or the Makefile) accordingly.

## 1. Rosenbrock: TORC driver and external C executable

From the repository root:

```sh
cd coupling/rosenbrock
make
mpirun -np 2 ./driver_rosen.exe
```

Adapt the MPI launcher and number of processes to your local TORC/MPI installation. The `Makefile` builds both `driver_rosen.exe` and `bin/simcode`.

The relevant files are:

- `driver_rosen.c`: initializes TORC, creates six model-evaluation tasks, waits for their completion, and prints the results.
- `fitfun.c`: creates each task's temporary directory, writes `params.dat`, launches the external program, and reads `fitness`.
- `spawner.c`: contains process-launching, file-copying, and directory-cleanup helpers.
- `bin/simcode.c`: evaluates the two-dimensional Rosenbrock function.
- `bin/run.sh`: launches `./simcode` inside each task's working directory.

The external program reads one parameter per line from `params.dat` and writes a single numerical result to `fitness`. The TORC driver reports the objective values as `res[0]`, `res[1]`, and so on. The example normally removes the temporary directories after each evaluation.

## 2. Simple model: TMCMC coupled to Python

Build the example's TMCMC engine:

```sh
cd coupling/simple_model/TMCMC
make
```

Then **run the engine from the example directory**, so that it can find `tmcmc.par` and `model/`:

```sh
cd ../example
../TMCMC/engine_tmcmc
```

The Makefile enables TORC by default (`use_torc=1`). Consult your local TORC/MPI setup if you want to launch the engine with multiple MPI processes.

The configuration in `example/tmcmc.par` specifies four parameters: the amplitude, angular frequency, and phase of a sinusoidal model, plus the observation-noise standard deviation. The scripts implement

\[
y(t) = A\sin(\omega t + \phi),
\]

and compute a Gaussian log-likelihood by comparing the predicted values with the observations in `model/data.dat`.

For each candidate parameter vector:

1. `TMCMC/fitfun.c` creates a directory named `tmpdir.*`, copies the files from `example/model/`, and writes `params.txt`.
2. `doall.sh` calls `my_model.py` to read the parameters and data and produce `result.txt`.
3. The same script calls `log_like.py`, which computes the likelihood and writes `loglike.txt`.
4. `TMCMC/fitfun.c` reads `loglike.txt` and returns its value to the TMCMC engine. A missing or invalid result is assigned a large negative log-likelihood.

**Note:** The supplied `example/tmcmc.par` uses `PopSize 40000` and `MaxStages 40`; reduce these values for a quick initial test. With the current `#define REMOVEDIRS 0` in `TMCMC/fitfun.c`, per-evaluation directories are retained, potentially consuming substantial disk space. Change the definition to `1` and rebuild if you want successful evaluations' directories to be removed.

## Adapting the examples to another simulation

The main integration points are the external launcher script and the coupling function (`fitfun.c`):

- Provide the simulation executable or scripts, together with any required data files.
- Adapt `run.sh` or `doall.sh` to invoke the simulation and any necessary postprocessing.
- Read the candidate parameters from the input file (`params.dat` or `params.txt` in these examples).
- Write the requested scalar output to the file that `fitfun.c` expects (`fitness` or `loglike.txt`).
- Set the correct parameter dimension and algorithm configuration (`PROBDIM`, `Nth`, parameter bounds, etc.).
- Ensure concurrent evaluations do not modify shared files or directories. For non-POSIX platforms, the process-launching code must be adapted.

The external executable can be sequential or internally parallel. TORC parallelizes the **independent model evaluations**; the coupling mechanism does not require modification of the external model to use TORC.

## Further information

See the [Pi4U repository README](../README.md) and the [simple-model README](simple_model/README.md) for additional context. These examples originate from the earlier Pi4U codebase; compiler flags and paths may need updating for your environment.
