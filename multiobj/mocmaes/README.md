# MO-CMA-ES with TORC

This directory provides **multiobjective covariance matrix adaptation evolution strategies (MO-CMA-ES)** with task-parallel objective evaluation through [TORC_LITE](https://github.com/CEID-HPCLAB/torc_lite), as part of [Pi4U](../../README.md).

The implementation builds on the [Shark machine learning library](https://github.com/Shark-ML/Shark), with local optimizer adaptations for evaluating application objectives through TORC. Shark must be placed in **`./Shark`**, configured, and compiled in **`./Shark/build`** before building these examples. Both Shark and the application must use **C++14**.

## Executables

| Executable | Optimizer | Objective-evaluation concurrency |
| --- | --- | --- |
| `MOCMAtest1` | Generational MO-CMA-ES; one offspring per parent per step. | Evaluates a full offspring batch through TORC. Recommended starting point for parallel experiments. |
| `ssMOCMAtest1` | Steady-state MO-CMA-ES; one offspring per step. | Initialization can evaluate multiple points concurrently; subsequent steps submit one task and wait for it. |

The steady-state executable is not an asynchronous optimizer with many offspring in flight. Increasing its worker count does not parallelize the single offspring evaluated at each search step.

## Method

MO-CMA-ES is a derivative-free method for continuous multiobjective minimization. It maintains a population of candidate solutions, each carrying a Gaussian mutation distribution with an adaptive covariance matrix and step size. A mutation has the general form

$$
x'=x+\sigma\mathcal{N}(0,C).
$$

The covariance adapts to useful directions and correlations in parameter space, while step-size adaptation controls the scale of exploration. Selection combines Pareto ranking with an indicator to decide which solutions survive when a front must be reduced. The executables use Shark's **hypervolume indicator** specialization.

The generational implementation combines parents and offspring, selects `PopSize` survivors, and updates the relevant parent/offspring adaptation state. The steady-state implementation chooses a rank-1 parent, generates one offspring, and applies selection to the parent population plus that offspring.

The result is a set of trade-off solutions. All objective values are minimized; return the negative of an objective that should be maximized. Retained populations can contain lower-ranked solutions, especially before convergence, so a saved population should not automatically be interpreted as a fully nondominated front.

Full covariance adaptation has quadratic storage per individual in the number of variables. Besides objective cost, covariance operations and hypervolume-based selection can become substantial as dimension, population size, or objective count increases.

## Parallel execution

Both drivers register `geval()` before `torc_init(..., MODE_MW)`. The local optimizer headers replace the objective-evaluation loop with TORC tasks:

```text
optimizer initialization / step
    -> TORC task: geval()
    -> application objective: fitfun()
    -> torc_waitall()
    -> selection and adaptation
```

Each task receives five arguments: a parameter array, its dimension, an objective output array, its dimension, and four metadata integers. Parameters use `CALL_BY_VAL`, dimensions and metadata use `CALL_BY_COP`, and objectives use `CALL_BY_RES`. The input/output buffers remain alive until the batch finishes.

Sampling, selection, and covariance adaptation run in the driver. Worker tasks evaluate the objective only. TORC distributes these tasks across MPI processes and local workers; MPI ranks do not represent independent evolutionary populations.

Metadata has the layout `[0, 0, evaluation_stage, individual_index]`. Initialization uses stage 0. Offspring evaluation stages begin at 2; driver iteration 1 therefore corresponds to evaluation stage 2. Candidate indices are zero-based. Use these values to construct independent task-specific filenames when invoking external models.

## Dependencies

- C and C++ compilers, with C++14 support.
- MPI compiler wrappers and launcher.
- TORC_LITE, installed separately.
- Shark source and its compiled library.
- CMake for building Shark.
- Boost headers and libraries, including thread and serialization.
- OpenBLAS for the supplied application link command.
- Optional: Python 3, NumPy, and Matplotlib for postprocessing.

Use a consistent MPI installation for TORC, the application wrapper, and the launcher. Build Shark and this application with compatible C++ compilers and standard libraries. In particular, changing from Apple Clang/libc++ to GCC/libstdc++ can require rebuilding C++ dependencies, including Boost libraries.

### Linux packages

For a Debian/Ubuntu system, assuming MPI is already installed:

```bash
sudo apt install build-essential cmake libboost-all-dev libopenblas-dev
```

### macOS / Homebrew

```bash
brew install cmake boost openblas
```

An MPI installation is also required. Use the same MPICH or Open MPI installation already used for TORC.

The supplied application Makefile includes Apple Silicon Homebrew paths:

```text
-I/opt/homebrew/include
-L/opt/homebrew/opt/openblas/lib
-L/opt/homebrew/lib
```

These are platform-specific dependency paths, not requirements of the algorithm. On Intel Homebrew systems, or with a custom installation, obtain the correct prefixes from `brew --prefix`, `brew --prefix boost`, and `brew --prefix openblas` and adjust the Makefile. On Linux, the `/opt/homebrew` paths can be removed.

## Install TORC_LITE

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

Use `F77=mpif90` if needed. If generated configuration files are missing, run `autoreconf -i` before configuring. Check that `torc_cflags` and `torc_libs` are available in `PATH`. Standards-compliant TORC execution requires `MPI_THREAD_SERIALIZED` or higher; consult the runtime documentation for details.

## Build Shark locally

Start in this directory:

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u/multiobj/mocmaes
git clone https://github.com/Shark-ML/Shark.git Shark
```

For reproducible builds, record and retain the Shark revision used. These local optimizer headers depend on Shark's C++ API; an arbitrary future revision is not guaranteed to be compatible.

### Set C++14 in Shark's CMake configuration

For this work, **Shark's CMake files were adjusted to use C++14**. Keep that change in your local Shark tree. Near the project setup, use:

```cmake
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

Check for subdirectory or target-specific settings that explicitly select another standard. Replace conflicting C++11 settings with C++14 as needed; passing a cache variable alone may not override a setting hardcoded later in a CMake file.

Configure and compile into the directory expected by the application Makefile:

```bash
cmake -S Shark -B Shark/build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DENABLE_OPENMP=OFF
cmake --build Shark/build --parallel 4
```

OpenMP is disabled here to simplify the dependency chain and avoid nested threading with TORC. Shark does not need to be installed system-wide: the application uses headers from `Shark/include` and `Shark/build/include`, and links the library in `Shark/build/lib`.

On Homebrew systems, add dependency hints if automatic discovery fails:

```bash
cmake -S Shark -B Shark/build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DENABLE_OPENMP=OFF \
    -DCMAKE_PREFIX_PATH="$(brew --prefix boost);$(brew --prefix openblas)"
cmake --build Shark/build --parallel 4
```

Shark's selected BLAS backend depends on its revision and platform; some macOS configurations select Accelerate. Check the configuration summary and retain any additional link dependencies required by the built Shark library. The application Makefile explicitly links OpenBLAS.

If selecting GNU compilers, do so on the first configuration of a fresh build directory, for example:

```bash
cmake -S Shark -B Shark/build-gcc \
    -DCMAKE_C_COMPILER="$(command -v gcc-16)" \
    -DCMAKE_CXX_COMPILER="$(command -v g++-16)" \
    -DCMAKE_CXX_STANDARD=14 \
    -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DENABLE_OPENMP=OFF
```

That example uses a separate build tree to avoid changing compilers in an existing CMake cache. The application Makefile expects `Shark/build`: either create the intended compiler build there from the outset or update **both** generated-header and library paths in the Makefile when using another build directory.

## Build the application

```bash
make
```

The Makefile builds both executables. It uses `mpic++ -std=c++14` for the C++ drivers and `gcc` for `fitfun.c`. The objective is declared with C linkage in `USER1.h`, so compile it as C rather than silently compiling the `.c` file as C++.

The application links:

```text
Shark/build/lib/libshark
OpenBLAS
Boost thread and serialization
TORC and its dependencies
```

`SHARK_DIR=./Shark` is the default. There is no `use_torc=1` option; both targets use TORC.

To use another MPI wrapper:

```bash
make clean
make CCPLUS="mpicxx -std=c++14"
```

For a Linux installation with dependencies in standard paths, explicit overrides can omit Homebrew flags:

```bash
make clean
make CFLAGS="-g -I./Shark/include -I./Shark/build/include $(torc_cflags)" \
     LIBS="-L./Shark/build/lib -lshark -lopenblas -lboost_thread -lboost_serialization $(torc_libs)"
```

If Shark was built with a GNU C++ compiler on macOS, select the corresponding MPI wrapper backend before rebuilding:

```bash
# MPICH
export MPICH_CXX="$(command -v g++-16)"

# For Open MPI, use OMPI_CXX instead:
# export OMPI_CXX="$(command -v g++-16)"
make clean
make
```

For shared Shark builds, make `Shark/build/lib` visible to the runtime loader or add a suitable rpath when linking. For example, on Linux:

```bash
export LD_LIBRARY_PATH="$PWD/Shark/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

On macOS, inspect dependencies with `otool -L MOCMAtest1` and use an appropriate rpath or install name for the local Shark library.

## Run

Both programs read **`mocma.par` in the current working directory**. A configuration filename is not a command-line argument. If the file is absent, compiled defaults are used.

```bash
# Full-population offspring evaluation
OPENBLAS_NUM_THREADS=1 TORC_WORKERS=4 mpirun -np 1 ./MOCMAtest1

# Multiple MPI processes, with two local workers each
OPENBLAS_NUM_THREADS=1 TORC_WORKERS=2 mpirun -np 4 ./MOCMAtest1

# Steady-state search: one objective task per optimization step
OPENBLAS_NUM_THREADS=1 TORC_WORKERS=1 mpirun -np 1 ./ssMOCMAtest1
```

Allocate CPUs for worker threads, TORC communication, and any threads in application libraries. The OpenBLAS setting avoids oversubscribing CPUs with nested BLAS threads; adjust deliberately if multithreaded BLAS is needed.

Use separate working directories for the two variants and for independent runs. Both variants write `pareto_mocma.txt` as their final output and can overwrite existing results.

## Configuration

The supplied file is:

```text
Nth             2
MaxStages       20
PopSize         100
#Bdef           0 3
B0              -6 6
B1              -6 6
seed            1
initialSigma    0.1
shrink_factor   1.0
maxtries        1000
```

| Key | Meaning | Compiled default / notes |
| --- | --- | --- |
| `Nth` | Number of real parameters, unrelated to thread count. | 1 |
| `MaxStages` | Number of `step()` calls after initialization. | 30 |
| `PopSize` | Retained population size `mu`. | 64 |
| `Bdef` | Default lower and upper bounds. | `-10 10` |
| `B0`, `B1`, ... | Per-variable bounds, with zero-based indices. | Override `Bdef`. |
| `seed` | Evolutionary random seed; nonzero gives a fixed seed. | 1 |
| `initialSigma` | Initial mutation step size. | 0.1 |
| `shrink_factor` | Reserved initialization setting in the generational driver. | Parsed but its use is disabled under `#if 0`. |
| `maxtries` | Maximum mutation attempts before clipping in the generational implementation. | 10000; use an integer literal. |

The steady-state driver does not parse or use `shrink_factor` or `maxtries`.

Use positive `Nth`, `PopSize`, and `initialSigma`, finite strictly ordered bounds, and a positive integer `maxtries`. The generational optimizer's bound arrays have 1024 entries, so use `Nth <= 1024`. The parser performs little validation and uses substring matching: `B1` can match `B10`. Keep explicit bound entries in increasing index order and check the parser before using complex bound overrides. Start comment lines with `#` in the first column.

There is no objective-count parameter. `USER1::numberOfObjectives()` returns 2; supporting more objectives requires changing that method and the objective implementation consistently.

`seed=0` seeds from the timer value cast to the RNG integer type. It does not use a random-device entropy source. For reproducible experiments, use an explicit nonzero seed. The two executables perform one run each; the trial-count variables in the drivers do not create an active multi-run loop.

### Iterations and evaluation budgets

Initialization evaluates `PopSize` starting points. With `P=PopSize` and `K=MaxStages`, absent replay or time-limit termination:

| Variant | Evaluations after initialization | Total evaluations |
| --- | --- | --- |
| Generational | `P` per step | `P * (K + 1)` |
| Steady-state | 1 per step | `P + K` |

Thus the supplied settings correspond to 2100 evaluations for the generational executable and 120 for the steady-state executable. Use comparable evaluation budgets, rather than equal `MaxStages`, when comparing their optimization quality.

Both drivers include a 24-hour `JOBMAXTIME` check. This is a source-defined time limit, checked after each step using recent step duration. It may stop before all requested steps; it is not a scheduler query.

## Objective interface and example

The C application objective is:

```c
void fitfun(double *x, int nx, void *output, int *winfo, double *result);
```

`geval()` passes `output=NULL`, the four task metadata integers as `winfo`, and storage for two minimized objective values as `result`. Fill both results and return finite values. The output count is not included in this helper signature, although the TORC adapter receives it; extend the interface consistently if dynamic objective counts are needed.

The supplied `fitfun.c` computes

$$
s=\sum_{i=0}^{nx-1}x_i,\qquad
f_1(x)=(s+2)^2-10,\qquad
f_2(x)=(s-2)^2+20.
$$

For the supplied bounds, feasible sums cover `[-2,2]`, and the Pareto set consists of feasible parameter vectors whose sum belongs to that interval. The objective-space front is the image of that interval. Multiple vectors can have identical objective values. Despite the inherited ZDT1 comment in `USER1.h`, this is **not the ZDT1 test problem**.

The sleep in `fitfun.c` is commented out. This objective is cheap and is intended as a functional example; task overhead can dominate its runtime.

For custom simulations, make model data available on all worker processes, keep evaluations thread-safe, and use unique temporary paths. Avoid concurrent use of Shark's shared `random::globalRng` inside objective tasks; use independent application RNG state if stochastic evaluations are required.

## Bounds

Starting points are sampled uniformly within the configured box. The generational optimizer retries mutations until a point is within bounds or `maxtries` attempts are reached, then clips remaining out-of-range components to the nearest bound.

The steady-state optimizer directly mutates one candidate and its task path bypasses Shark's `PenalizingEvaluator`. It does not implement the same retry-and-clip mechanism. Do not assume steady-state offspring remain inside the configured box; add and validate the desired repair or rejection policy before using objectives that are undefined outside their domain.

Neither task path supplies a general nonlinear-constraint result to the optimizer. Such constraints require an explicit application/algorithm extension.

## Files produced

All numeric result rows use:

```text
x[0] ... x[Nth-1]  objective_1 objective_2
```

There are no column labels. With the supplied `Nth=2`, objective values occupy columns 3 and 4 (1-based).

| File | Contents |
| --- | --- |
| `curgen_db_001.txt` | Generational initial evaluated population. |
| `curgen_db_002.txt`, etc. | Generational evaluated offspring batches, before survivor selection. |
| `pareto_mocma_001.txt`, etc. | Generational retained population after each recorded driver step. |
| `ss_curgen_db_002.txt`, etc. | Steady-state evaluated offspring; normally one row per step. |
| `pareto_ssmocma_001.txt`, etc. | Steady-state retained population after each recorded driver step. |
| `pareto_mocma.txt` | Final retained population, written by either executable. |

The retained population is not an external archive of every nondominated candidate encountered. Filter for nondominance when a strict Pareto subset is needed. Files use `%f` and therefore six digits after the decimal point; increase output precision if accurate postprocessing or replay is important.

## Plotting

The separate Pi4U helpers `plot_pareto.py` and `display_gen_multi.py` can be placed in this directory or invoked by their full paths. They are not built by this Makefile.

```bash
python3 -m pip install numpy matplotlib

# Objective space, using the supplied two-variable layout
python3 plot_pareto.py --file pareto_mocma.txt --columns 3 4 \
    --labels "Objective 1" "Objective 2"

# A retained generational population after iteration 10
python3 plot_pareto.py pareto_mocma 10 2

# Parameter space colored by objective 1, or objective 2
python3 display_gen_multi.py pareto_mocma 10 2 --auto-limits
python3 display_gen_multi.py pareto_mocma 10 2 --color-column 4 --auto-limits
```

Plots open by default. Append `--save front.png --no-show` to save without a GUI. Change the dimension and column selections when `Nth` changes. The Pareto helper highlights nondominated points among the supplied rows; the parameter-space helper requires at least two parameters.

The legacy command files in `plot/` use fixed filenames and column selections; check those against your run's dimension and output location before use.

## Evaluation replay and initial populations

The generational executable accepts:

```bash
TORC_WORKERS=4 mpirun -np 1 ./MOCMAtest1 -cr
```

This enables **replay of saved evaluations** from `curgen_db_*.txt`, beginning at initialization, while reconstructing optimizer steps. Once a dump is missing, the code switches to fresh evaluations. It does not deserialize a complete optimizer state or jump directly to the latest generation.

Use the same seed, dimensions, population size, bounds, and optimizer settings for replay. Current replay code has limitations: it writes through a cast into the initialization point array, performs incomplete input validation, and reads rounded text values. Treat it as a development feature requiring validation rather than a production checkpoint guarantee.

The steady-state driver accepts `-cr` syntactically, but its optimizer header does not read evaluation dumps; this flag does not provide replay for that variant. Loading `user_initial_pop.txt` is disabled under `#if 0` in the generational header.

## Build troubleshooting

- **`boost/version.hpp` missing:** install Boost and add its parent `include` directory to the search path. Homebrew flags must match the actual installation prefix.
- **`shark::random::globalRng` undefined:** ensure the application links the compiled `-lshark` from `Shark/build/lib`, not just Shark headers.
- **`fitfun` undefined:** keep matching C linkage and compile `fitfun.c` with the C compiler; clean stale objects after changing linkage.
- **Deprecated `boost/progress.hpp` in Shark tests:** disable tests with `BUILD_TESTING=OFF`. If deliberately building those tests, the installed Boost may require `BOOST_TIMER_ENABLE_DEPRECATED` while the legacy header remains available.
- **Compiler-standard errors:** check both Shark CMake files and the actual verbose compiler command for C++14. Do not mix generated headers from one Shark build with a library from another.
- **OpenMP runtime symbols unresolved:** use the same OpenMP configuration throughout, or rebuild Shark with `ENABLE_OPENMP=OFF` as shown above.

`make clean` removes application executables and objects. `make clear` deletes `curgen_db_*.txt` and `pareto_mocma_*.txt`; it does not remove every steady-state or final output filename. Archive results before cleaning run data.

## Source map

| File | Role |
| --- | --- |
| `MOCMAtest1.cpp` | Generational driver, configuration, seeding, time limit and retained-population output. |
| `MOCMApeh.h` | Generational MO-CMA-ES adaptation, TORC batches, bounds repair and evaluation replay. |
| `ssMOCMAtest1.cpp` | Steady-state driver. |
| `ssMOCMApeh.h` | Steady-state optimizer adaptation and TORC evaluation. |
| `USER1.h` | Shark objective adapter, bounds, starting points and `geval()` task. |
| `fitfun.c` | Replaceable application objective. |
| `mocma.par` | Runtime settings. |
| `Shark/`, `Shark/build/` | External Shark source and its local configured build. |

## References and credits

1. C. Igel, N. Hansen, and S. Roth. **Covariance Matrix Adaptation for Multi-objective Optimization.** *Evolutionary Computation*, 15(1), 1–28, 2007. [DOI: 10.1162/evco.2007.15.1.1](https://doi.org/10.1162/evco.2007.15.1.1).
2. C. Igel, T. Suttorp, and N. Hansen. **Steady-State Selection and Efficient Covariance Matrix Update in the Multi-objective CMA-ES.** *EMO 2007*, 171–185. [DOI: 10.1007/978-3-540-70928-2_16](https://doi.org/10.1007/978-3-540-70928-2_16).
3. [Shark source and documentation](https://github.com/Shark-ML/Shark).
4. [TORC_LITE runtime](https://github.com/CEID-HPCLAB/torc_lite).
5. P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A High Performance Computing Framework for Bayesian Uncertainty Quantification of Complex Models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).
6. S. Verma, P. Hadjidoukas, P. Wirth, and P. Koumoutsakos.
**Multi-objective optimization of artificial swimmers.**
*2017 IEEE Congress on Evolutionary Computation (CEC)*,
pp. 1037–1046, 2017.
[DOI: 10.1109/CEC.2017.7969422](https://doi.org/10.1109/CEC.2017.7969422).

The implementation retains Shark Development Team attribution, including the original example credited to O. Krause. The Pi4U application integration and TORC parallelization are by **Panagiotis E. Hadjidoukas**. Preserve the original notices in the adapted files. Consult their Shark LGPL notices, the Pi4U repository licensing information, and TORC's separate terms when redistributing.
