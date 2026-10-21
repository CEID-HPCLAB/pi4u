# Pi4U documentation report

**Source:** [Pi4U documentation website](https://phadjido.github.io/pi4u/)  
**Extracted:** 9 October 2026  
**Source branch:** `phadjido/pi4u`, `gh-pages`  
**Source commit:** `66c0b4fe17365f38f585f38d5db97bc778fdaa5b`  
**Editorial update:** 22 October 2026 (current-repository guide and maintenance notes; original website extraction retained)

## Scope and reading notes

This report consolidates the content of all 14 documentation pages in the supplied website's navigation. Page text, code examples, mathematical expressions, publication references, and figure links are retained. Website navigation, edit buttons, footer controls, and other interface elements are omitted. The website's source HTML was retrieved from its `gh-pages` branch.

The extracted chapters below record the published documentation, with the TORC prerequisites and installation instructions updated on 9 October 2026 for the separate CEID-HPCLAB Torc-Lite repository. Other historical paths, commands, dates, and contact addresses have been preserved for traceability and should be checked against the current source tree before use. Links are made absolute so they remain usable outside the website. Figures are linked to their original website locations and require network access to display.

Several pages contain only placeholders. They are included without inventing missing documentation. An editorial appendix at the end distinguishes migration notes from the original content.

## Current Pi4U documentation guide (editorial update, October 2026)

> **Important distinction.** The 14 chapters below are a historical capture of the Pi4U website, not an up-to-date manual for every engine. The project is now maintained at [CEID-HPCLAB/pi4u](https://github.com/CEID-HPCLAB/pi4u). For current build commands, supported execution modes, application interfaces, parameter files, and troubleshooting, **use the README in the corresponding repository directory**. The website's older `build/`, `source/`, and `examples/` commands should not be treated as instructions for the current tree without verification.

### Division of responsibility

| Resource | Intended purpose |
| --- | --- |
| [Repository README](https://github.com/CEID-HPCLAB/pi4u/blob/main/README.md) | Project scope, algorithm overview, repository map, history, license and citations. |
| **This report / project-wide documentation** | TORC architecture, shared parallel-execution concepts, external-model interface patterns, portability, and directions to component documentation. |
| **Case-specific README files** | Algorithm description, exact prerequisites, compiler options, build targets, parameters, input/output files, run commands, and limitations. |
| [Torc-Lite documentation](https://github.com/CEID-HPCLAB/torc_lite) | Installation and configuration of the external tasking runtime. |
| [Original documentation site](https://phadjido.github.io/pi4u/) | Historical examples and mathematical background, pending maintenance of the website. |

### Repository documentation index

The following paths were checked against the `main` branch on 22 October 2026. Each link points to the relevant case-specific documentation rather than reproducing its contents here.

| Area | Documentation |
| --- | --- |
| Bayesian inference | [TMCMC](https://github.com/CEID-HPCLAB/pi4u/blob/main/inference/TMCMC/README.md), [DRAM](https://github.com/CEID-HPCLAB/pi4u/blob/main/inference/DRAM/README.md), [ABC-SubSim](https://github.com/CEID-HPCLAB/pi4u/blob/main/inference/ABC_SubSim/README.md) |
| Single-objective optimization | [CMA-ES](https://github.com/CEID-HPCLAB/pi4u/blob/main/singleopt/cmaes/README.md), [AMaLGaM](https://github.com/CEID-HPCLAB/pi4u/blob/main/singleopt/AMaLGaM/README.md) |
| Multiobjective optimization | [NSGA-II](https://github.com/CEID-HPCLAB/pi4u/blob/main/multiobj/nsga2/README.md), [MOEA/D](https://github.com/CEID-HPCLAB/pi4u/blob/main/multiobj/moead/README.md), [MO-CMA-ES](https://github.com/CEID-HPCLAB/pi4u/blob/main/multiobj/mocmaes/README.md) |
| Rare-event analysis | [Subset simulation](https://github.com/CEID-HPCLAB/pi4u/blob/main/rare/subset/README.md) |
| External-code coupling | [Python/TMCMC example](https://github.com/CEID-HPCLAB/pi4u/blob/main/coupling/simple_model/README.md); a directory-level `coupling/README.md` has been drafted separately but was **not on the checked `main` branch**. The `coupling/rosenbrock/` case also has no standalone README on that branch. |
| Numerical derivatives | [PNDL README](https://github.com/CEID-HPCLAB/pi4u/blob/main/derivatives/pndl/README) (plain `README` file), [derivative tools](https://github.com/CEID-HPCLAB/pi4u/blob/main/derivatives/tools/README.md) |
| Utilities and data analysis | [Parallel model-evaluation tools](https://github.com/CEID-HPCLAB/pi4u/blob/main/tools/README.md), [postprocessing](https://github.com/CEID-HPCLAB/pi4u/blob/main/postprocessing_tools/README.md), [local covariance scripts](https://github.com/CEID-HPCLAB/pi4u/blob/main/scripts/README.md) |

### Shared architecture and concepts

Pi4U collects multiple inference, optimization, numerical-differentiation, and reliability algorithms under a common approach to executing expensive model evaluations. Many engines use **Torc-Lite**, a task-based runtime that combines worker threads with MPI processes. An algorithm can generate independent function-evaluation tasks, let TORC distribute them, and collect results for the next algorithmic step. This separates the choice of algorithm from the runtime used to evaluate candidates.

A conceptual flow is:

```text
Inference / optimization / analysis engine
         |
         v
Application-specific evaluation function (e.g., fitfun)
         |
         +--> In-process evaluation
         |
         +--> External executable or script
         |       parameters -> run model -> read scalar result
         v
TORC task execution (where supported)
         |
         +--> MPI processes
         +--> Local worker threads
```

The drawing represents a *conceptual* structure, not one universal executable or mandatory API: some components are serial, use OpenMP, or provide other execution modes. In a TORC-driven external-model example, TORC distributes the evaluation tasks while the external simulation itself need not be MPI-aware or linked against TORC.

#### Common operational guidance

- **Runtime and MPI compatibility:** TORC-based components use an external Torc-Lite installation; build and execute TORC and dependent engines with a consistent MPI implementation. Use the appropriate MPI thread support for the runtime.
- **Thread safety and concurrent evaluations:** model evaluations may occur concurrently on threads and processes. Keep shared in-process state safe and use separate per-task files/directories for external models. Do not assume a global working directory or a single shared output file.
- **Model interface and result semantics:** each engine defines its own `fitfun` signature, configuration, and interpretation of the returned scalar or vector. An optimization objective, a TMCMC log-likelihood, and a reliability response are *not* interchangeable merely because each can be evaluated in parallel.
- **Parallel execution modes:** serial, OpenMP, and TORC variants are not available for every engine. Consult that engine's README and Makefile rather than applying a global `make` command.
- **Configuration and outputs:** settings and generated data files vary by case; the corresponding README is the authoritative description of supported parameters, build flags, expected outputs, and restart behavior.

#### Current scope versus future work

The current repository demonstrates TORC-based parallel evaluations and contains examples of external-process coupling. Work on exporting algorithms as reusable, thread-safe library calls, nested invocation from host applications, language-level clients (MATLAB, R, Python), and generalized prediction-assisted scheduling should be documented as **development goals or independently verified implementations**, not as uniform features already available in every engine.

### How to use the historical chapters

| Historical chapter | Current reading path |
| --- | --- |
| Installation (Chapter 2) | Torc-Lite installation plus the selected case README; do not use the old central `build/` commands uncritically. |
| Sampling (Chapter 3) | TMCMC or DRAM README and the active example files in the source tree. |
| External coupling (Chapter 4) | `coupling/simple_model/README.md`, `coupling/rosenbrock/`, and the proposed coupling-directory README after it is committed. |
| Optimization (Chapter 5, placeholder) | CMA-ES, AMaLGaM, NSGA-II, MOEA/D, and MO-CMA-ES READMEs. |
| Bayesian UQ plots (Chapter 6, placeholder) | `postprocessing_tools/README.md` and the relevant engine README. |
| Structure, conventions, parameter files, likelihoods (Chapters 7–10) | Consult the source-tree index above and specific engine README; these are good candidates for **short shared-convention pages** once the underlying conventions are agreed upon. |
| Performance (Chapter 13, placeholder) | A future project-wide section should compare TORC scheduling, MPI/thread scaling, workload imbalance, and evaluation-cost distributions across engines, with reproducible benchmark configurations. No such benchmark claims are inferred from the repository index here. |

---

## Contents and coverage

| Chapter | Page | Published content |
| --- | --- | --- |
| 1 | [Welcome to Π4U](#chapter-1) | Documentation |
| 2 | [Installation](#chapter-2) | Updated TORC installation; other sections historical |
| 3 | [Sampling a posterior distribution](#chapter-3) | Documentation |
| 4 | [Coupling with external code](#chapter-4) | Documentation |
| 5 | [Optimization](#chapter-5) | Placeholder |
| 6 | [Bayesian UQ plots](#chapter-6) | Placeholder |
| 7 | [Structure](#chapter-7) | Placeholder |
| 8 | [Conventions](#chapter-8) | Placeholder |
| 9 | [Par files](#chapter-9) | Headings and external link only |
| 10 | [Likelihoods](#chapter-10) | Placeholder |
| 11 | [Upcoming](#chapter-11) | Historical roadmap |
| 12 | [Beta](#chapter-12) | Placeholder |
| 13 | [Performance](#chapter-13) | Placeholder |
| 14 | [Related publications](#chapter-14) | Documentation |


---

<a id="chapter-1"></a>

## Welcome to Π4U

Π4U is a high performance framework for optimization, sampling and Bayesian uncertainty quantification of large scale computational models.

The framework is based on the [TORC](https://phadjido.github.io/pi4u/references/#anchor_torc) task-parallel library for clusters, which is designed to provide unified programming and runtime support for computing platforms that range from single-core systems to hybrid multicore-GPU clusters and heterogenous Grid based supercomputers.

## What Π4U can do for you

1. **Optimize**: given a cost function $F(\vartheta)$ find
      $$
      \vartheta^\star = \mathop{\arg\min}\limits_{\vartheta} F(\vartheta) \,.
      $$
2. **Sample**: given the density of a probability distribution $p_{\vartheta}$ draw samples,
      $$
          \vartheta^{(k)} \sim p_\vartheta, \quad k=1,\ldots,N_s \, .
      $$
3. **Uncertainty Quantification**: given a set of data $d$, the output of the model $f(x;\vartheta)$ a likelihood function $p(d|\vartheta)$ and a prior probablity density $p(\vartheta)$ sample the posterior distribution,
      $$
      p(\vartheta | d) = \frac{p(d | \vartheta) p(\vartheta)}{p(d)}\, .
      $$
  The model output $f$ depends on a set of input parameters $x$.

After [installing](https://phadjido.github.io/pi4u/installation/) the software have a look at the [examples](https://phadjido.github.io/pi4u/examples/sampling/) and learn how you can run your cases.

Warning

The software and the documentation page are under continuous development. New pages and new feature will be constantly added.

*Source page: [https://phadjido.github.io/pi4u/](https://phadjido.github.io/pi4u/)*


---

<a id="chapter-2"></a>

## Installation

### Prerequisites

- Git, GNU Make, and C/C++ compilers must be available. For TORC Fortran support and PNDL, also provide a Fortran compiler and MPI Fortran wrapper.
- Use an MPI implementation such as MPICH or Open MPI, with the corresponding compiler wrappers available on `PATH`. The examples below use `mpicc` and `mpif90`. Torc-Lite requires `MPI_THREAD_SERIALIZED` or `MPI_THREAD_MULTIPLE` for standards-compliant execution.
- Autoconf and Automake are needed if the generated build files must be regenerated.
- [GSL-2.4](http://www.gnu.org/software/gsl/) or later must be installed on your system.

### Installation steps

#### 1. Torc-Lite library — updated installation

Pi4U uses Torc-Lite as an external dependency. Obtain it from [CEID-HPCLAB/torc_lite](https://github.com/CEID-HPCLAB/torc_lite) and install it before building the TORC-based Pi4U engines.

The following commands assume a Bash-compatible shell and an MPI installation with C and Fortran compiler wrappers. On a cluster, load the appropriate compiler and MPI modules first. Use the same MPI implementation when building Torc-Lite and Pi4U.

From a directory where you keep source checkouts:

```bash
git clone https://github.com/CEID-HPCLAB/torc_lite.git
cd torc_lite

export TORC_PREFIX="$HOME/usr/torc"
./configure CC=mpicc F77=mpif90 --prefix="$TORC_PREFIX"
make
make install

export PATH="$TORC_PREFIX/bin:$PATH"
```

This installs Torc-Lite under your home directory without requiring administrator privileges. Choose a different `TORC_PREFIX` if desired. If `configure` or generated build files are missing, run `autoreconf -fi` in the Torc-Lite source directory before configuring. Substitute the MPI wrapper names provided by your installation where necessary.

Check the installed helper scripts:

```bash
command -v torc_cflags
command -v torc_libs
torc_cflags
torc_libs
```

`torc_cflags` prints the include/compiler flags and `torc_libs` prints the linker flags required by TORC applications. Pi4U Makefiles use these scripts to locate the installed runtime. Keep the installation's `bin` directory on `PATH` in new terminal sessions and batch jobs that build Pi4U. For the example prefix, add the following line to your shell startup file or job script:

```bash
export PATH="$HOME/usr/torc/bin:$PATH"
```

**Verify the runtime.** From the Torc-Lite source root, build and run a demo:

```bash
make -C demo
export TORC_WORKERS=2
mpirun -np 2 ./demo/masterslave
```

This uses two MPI processes with two TORC workers per process. TORC also uses communication threads. Allocate sufficient CPU resources and use your site's launcher and binding configuration when running on a cluster. `TORC_WORKERS` controls TORC worker threads per process; the MPI launcher controls the number of processes.

After checking the demo, move to your Pi4U checkout before following Pi4U build instructions. The remaining installation sections below reproduce the historical website and use its older repository layout; see the editorial appendix for current directory mappings.

*Current TORC installation reference: [Torc-Lite README](https://github.com/CEID-HPCLAB/torc_lite/blob/master/README.md). The commands above replace the original bundled-runtime instructions.*

#### 2. GSL library

The GNU Scientific Library (GSL) is a numerical library for C and C++ programmers.

- Download the latest version of GSL. In a terminal write

```text
    wget http://mirror.switch.ch/ftp/mirror/gnu/gsl/gsl-latest.tar.gz
    tar -xvzf gsl-latest.tar.gz
```

- Configure. Set the install folder to be `/$HOME/usr`. If you want to install in the default diretcory, `/usr/local`, delete the  `--prefix=/$HOME/usr`.

```text
    ./configure   --prefix=/$HOME/usr
```

- Compile and install

```text
    make -j2
    make install
```

This step will take some time. If you have more available cores, change the 2 in the -j2 to a bigger number.

#### 3. Sampling and Optimization Algorithms

Enter the `build` directory:

```text
    cd build  
```

Before compiling, the following need to be checked:

- `gsl-config` must be in your `PATH` (installed by GSL).
- Name for the MPI compiler in the `Makefile`, since this can be named differently on different platforms (e.g. CC=cc on Piz Daint).

**Compilation options:**

build the default option (uses use_torc=0):

```text
    make
```

build the OpenMP version:

```text
    make use_omp=1
```

build the TORC-based version:

```text
    make use_torc=1
```

build the serial version:

```text
    make  use_omp=0  use_torc=0
```

### Test

You can test your installation by doing the following (from root folder):

```text
    cd source/CMAES
    make test
```

### Notes

Please send your questions to:

- phadjido AT upatras.gr

*Source page: [https://phadjido.github.io/pi4u/installation/](https://phadjido.github.io/pi4u/installation/)*


---

<a id="chapter-3"></a>

## Sampling a posterior distribution

In this example we will show how to sample the posterior distribution in an uncertainty quantification problem. First, we will create synthetic data using the model,

$$
f(t;\varphi) = \varphi_1 \sin(\varphi_2  x + \varphi_3 )
$$

We fix $\varphi^* = (2,3,1)$ and create $100$ data point using the equation,

$$
d_i = f(x_i,\varphi^{\star}) + \sigma \epsilon, \quad \epsilon \sim \mathcal{N}(0,1) \, ,
$$

where $x_i = 0.02 i,\; i=1,\ldots,100$ and $\sigma=0.3$. We will sample the posterior distribution of $\vartheta=(\varphi,\sigma)$ conditioned on the data $d$. The prior distribution is uniform for each parameter,

$$
\begin{align}
    p(\vartheta_1) &= \mathcal{U}( \vartheta_1 | 0,5) \\
    p(\vartheta_2) &= \mathcal{U}( \vartheta_2 | 0,10) \\
    p(\vartheta_3) &= \mathcal{U}( \vartheta_3 | -3.14,3.14) \\
    p(\vartheta_4) &= \mathcal{U}( \vartheta_4 | 0,5)  \, ,
\end{align}
$$

and the likelihood function in given by,

$$
\begin{align}
    p(d | \vartheta) & = \prod_{i=1}^4 p(d_i | \vartheta) \\
                     &=  \prod_{i=1}^4 \mathcal{N}( d_i | f(x;\varphi),\sigma ) \, .
\end{align}
$$

### Sampling with TMCMC

#### Compile and run

From the base folder run

```text
cd build
make tmcmc_theta_internal
```

Make sure that `use_torc=0` and `use_omp=0` in the Makefile since we don't want to run parallel in this example. Go back to the the base folder and run

```text
cd ../examples/sampling/internal/tmcmc/
./setup_tmcmc.sh
cd runs/run_001/
```

Run the TMCMC sampling algorithm:

```text
./tmcmc_theta_internal
```

Finally, visualize the samples:

```text
cp ../../../../../../source/tools/display/plotmatrix_hist.py .
python plotmatrix_hist.py final.txt
```

![Documentation figure](https://phadjido.github.io/pi4u/examples/fig_tmcmc.png)

#### Behind the scripts

The script `./setup_tmcmc.sh` makes a new running folder and copies inside the executable and the needed files:

1. the data file data.txt,
2. the file that contains the prior information, [priors.par](https://phadjido.github.io/pi4u/developing/par_files/#priors.par),
3. the parameter file for tmcmc, [tmcmc.par](https://phadjido.github.io/pi4u/developing/par_files/#tmcmc.par).

For this example, TMCMC has been linked to the likelihood function `loglike_theta_fast.c`. Inside this file the model $f$ as well as the likelihood function $p(d | \vartheta)$ has beem implemented. More information on the the likelihood implementation and how to write your own likelihood you can find [here](https://phadjido.github.io/pi4u/developing/likelihoods/).

### Sampling with DRAM

#### Compile and run

From the base folder run

```text
cd build
make dram_theta_internal
```

Make sure that `use_torc=0` and `use_omp=0` in the Makefile since we don't want to run parallel in this example. Go back to the the base folder and run

```text
cd ../examples/sampling/internal/dram/
./setup_dram.sh
cd runs/run_001/
```

Run the TMCMC sampling algorithm:

```text
./tmcmc_dram_internal
```

Finally, visualize the samples:

```text
cp ../../../../../../source/tools/display/plotmatrix_hist.py .
tail -n +1000 chain.txt > tmp
./plotmatrix_hist.py tmp
```

With this command `tail -n +1000 chain.txt` we discard the first $1000$ samples which we consider as the burn-in period.

![Documentation figure](https://phadjido.github.io/pi4u/examples/fig_dram.png)

#### Behind the scripts

Same as in the TMCMC example. The only difference is that instead of the tmcmc.par is substituted by [dram.par](https://phadjido.github.io/pi4u/developing/par_files/#dram.par).

*Source page: [https://phadjido.github.io/pi4u/examples/sampling/](https://phadjido.github.io/pi4u/examples/sampling/)*


---

<a id="chapter-4"></a>

## Coupling with external code

```text
  cd build
  make tmcmc_theta_external use_torc=1
```

This version creates intermediate folders of the form `tmpdir.*.*.*.*` where the external code is running on a set of parameters saved in `params.txt`. After the execution the directory is deleted. If you want to keep the directories set `REMOVEDIRS  0` in `source/likelihoods/loglike_theta.c`.

**To setup the example:**

```text
    cd examples/sampling/external/tmcmc
    ./setup_tmcmc.sh
    cd runs/run_001
```

**Contents of `runs/run_001`:**

- The Π4U executable: `tmcmc_theta_external`
- The files `priors.par` and `tmcmc.par`. These files contain information about the choice of the prior distributions and the TMCMC parameters respectively.
- A directory named `model`. Inside this directory, Π4U expects to find:

  - a file with the experimental data named `data.txt`
  - a user-provided script named `doall.sh`, which (i) runs the external simulation, (ii) compares the output with the experimental data, and (iii) saves the log-likelihood inside a file called `loglike.txt`. The value inside `loglike.txt` is then read from Π4U.

**To run the example:**

```text
    mpirun -np 4 ./tmcmc_theta_external
```

or

```text
    export TORC_WORKERS=4
    mpirun -np 1 ./tmcmc_theta_external
```

*Source page: [https://phadjido.github.io/pi4u/examples/external/](https://phadjido.github.io/pi4u/examples/external/)*


---

<a id="chapter-5"></a>

## Optimization

*Source page: [https://phadjido.github.io/pi4u/examples/optimizing/](https://phadjido.github.io/pi4u/examples/optimizing/)*


---

<a id="chapter-6"></a>

## Bayesian UQ plots

- maltab
- python

## Optimization plots

- maltab
- python

*Source page: [https://phadjido.github.io/pi4u/examples/post/](https://phadjido.github.io/pi4u/examples/post/)*


---

<a id="chapter-7"></a>

## Structure

coming soon

*Source page: [https://phadjido.github.io/pi4u/developing/structure/](https://phadjido.github.io/pi4u/developing/structure/)*


---

<a id="chapter-8"></a>

## Conventions

coming soon

*Source page: [https://phadjido.github.io/pi4u/developing/conventions/](https://phadjido.github.io/pi4u/developing/conventions/)*


---

<a id="chapter-9"></a>

## Par files

### priors.par

### tmcmc.par

### dram.par

[dram parameter file generator](http://www.cse-lab.ethz.ch/research/projects/pi4u/dram-parameter-file/)

*Source page: [https://phadjido.github.io/pi4u/developing/par_files/](https://phadjido.github.io/pi4u/developing/par_files/)*


---

<a id="chapter-10"></a>

## Likelihoods

coming soon

*Source page: [https://phadjido.github.io/pi4u/developing/likelihoods/](https://phadjido.github.io/pi4u/developing/likelihoods/)*


---

<a id="chapter-11"></a>

## Upcoming

- ABC-SubSim (xx/xx/2018)
- AMaLGaM (xx/xx/2018)
- Multi-objective optimization (xx/xx/2018)
- Manifold TMCMC (xx/xx/2018)

*Source page: [https://phadjido.github.io/pi4u/developing/upcoming/](https://phadjido.github.io/pi4u/developing/upcoming/)*


---

<a id="chapter-12"></a>

## Beta

coming soon

*Source page: [https://phadjido.github.io/pi4u/developing/beta/](https://phadjido.github.io/pi4u/developing/beta/)*


---

<a id="chapter-13"></a>

## Performance

TODO

*Source page: [https://phadjido.github.io/pi4u/hpc/](https://phadjido.github.io/pi4u/hpc/)*


---

<a id="chapter-14"></a>

## Related publications

### Pi4U framework

- Hadjidoukas P.E., Angelikopoulos P., Papadimitriou C., Koumoutsakos P., Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models. J. Comput. Phys., 284:1-21, 2015
  ([doi](http://www.sciencedirect.com/science/article/pii/S0021999114008134),[pdf](http://www.cse-lab.ethz.ch/images/stories/Publications/2015/Pi4U.JCP2015.pdf))
- Hadjidoukas P.E., Angelikopoulos P., Kulakova L., Papadimitriou C., Koumoutsakos P., Exploiting Task-Based Parallelism in Bayesian Uncertainty Quantification. EuroPar 2015, LLCS 2015, 9233, 532
  ([doi](http://link.springer.com/chapter/10.1007%2F978-3-662-48096-0_41),[pdf](http://www.cse-lab.ethz.ch/images/stories/Publications/2015/Pi4U.Europar2015.pdf))

### Applications

- Kulakova L., Angelikopoulos P., Hadjidoukas P. E., Papadimitriou C., Koumoutsakos P., Approximate Bayesian Computation for Granular and Molecular Dynamics Simulations. Proceedings of the Platform for Advanced Scientific Computing Conference PASC'16, 2016
  ([doi](http://doi.acm.org/10.1145/2929908.2929918), [pdf](http://www.cse-lab.ethz.ch/images/stories/Publications/2016/a4-kulakova.pdf))
- Hadjidoukas P.E, Angelikopoulos P., Rossinelli D., Alexeev D., Papadimitriou C., Koumoutsakos P., Bayesian uncertainty quantification and propagation for discrete element simulations of granular materials. Comput. Methods Appl. Mech. Engrg., 282:218-238, 2014
  ([doi](http://dx.doi.org/10.1016/j.cma.2014.07.017),[pdf](http://www.cse-lab.ethz.ch/images/stories/Publications/2014/PH_DEM_14.pdf))

### TORC: Task-Based Runtime Library

- Hadjidoukas P.E., Lappas E., Dimakopoulos V.V: A Runtime Library for Platform-Independent Task Parallelism. PDP 2012: 229-236
  ([doi](http://dx.doi.org/10.1109/PDP.2012.89))

*Source page: [https://phadjido.github.io/pi4u/references/](https://phadjido.github.io/pi4u/references/)*


---

## Editorial appendix: interpretation and migration notes

These notes were added for this report and are not part of the extracted website documentation.

### Documentation gaps

The optimization, postprocessing, structure, coding conventions, likelihood implementation, beta, and performance pages are placeholders. The parameter-files page contains headings and a link to a DRAM parameter-file generator, but no substantive explanatory text. The upcoming page is a historical list with unspecified 2018 dates; it does not establish current development plans or current feature availability.

### Repository layout and TORC dependency

The original installation page assumed a bundled runtime at `lib/torc_lite`. Section 1 of the installation chapter has been updated to install Torc-Lite from its separate repository. The subsequent engine-build instructions still refer to a central `build` directory and an example at `source/CMAES`; those paths describe the documentation's historical layout.

For the revived framework, use [CEID-HPCLAB/pi4u](https://github.com/CEID-HPCLAB/pi4u). Install TORC separately from [CEID-HPCLAB/torc_lite](https://github.com/CEID-HPCLAB/torc_lite), and expose the installed `torc_cflags` and `torc_libs` scripts through `PATH`. Consult the updated installation guide prepared for the continuation before running historical build commands.

The source tree reviewed for the continuation places TMCMC in `inference/TMCMC`, CMA-ES in `singleopt/cmaes`, ABC-SubSim in `inference/ABC_SubSim`, and subset simulation in `rare/subset`. Build modes vary by engine. These locations should be rechecked if the repository is reorganized.

### Sampling-example checks

The sampling page describes 100 observations, but its displayed likelihood product runs from 1 to 4. This apparent inconsistency is preserved in the extracted text; verify the intended product against the model implementation before reusing the equation.

The same page labels its DRAM run as TMCMC and builds `dram_theta_internal` but subsequently invokes `tmcmc_dram_internal`. Verify the executable name against the corresponding build target. Its model equation also mixes the input-variable names `t` and `x`; check the notation when reusing the example.

The DRAM example's `tail -n +1000` command starts output at line 1000. For a file consisting entirely of samples, it skips 999 lines. To discard exactly 1000 sample lines, use `tail -n +1001`; account for any header lines separately.

### Historical references and contact information

References to ETH Zurich, CSELAB, and ETH email addresses describe the original development period. Original contributor credit should be retained. Use the [continuation's issue tracker](https://github.com/CEID-HPCLAB/pi4u/issues) for current support.

### Extraction limits

This report contains documentation published in the site's navigation, including placeholders, with the TORC installation update identified above. It does not infer undocumented APIs or algorithms or reproduce the repository's complete source code. The build and run commands have not been validated by execution in this environment. External linked papers and repositories are referenced rather than reproduced.


### October 2026 documentation-maintenance priorities

The following items concern the **current repository and documentation organization**; they do not alter the historical source text above.

1. **Keep the case READMEs as the single source of truth.** The central report/website should link to them and explain project-wide architecture; it should not maintain duplicate build recipes, parameter tables, or walkthroughs that can drift out of date.
2. **Complete the external-coupling index.** Commit the drafted `coupling/README.md` when reviewed; consider adding a short `coupling/rosenbrock/README.md` if the goal is literally one README per case. The directory-level index and case-specific documentation serve different purposes.
3. **Correct the root README navigation.** As checked on `main`, the entries for MOEA/D and MO-CMA-ES display NSGA-II paths in the link text even though their hyperlink destinations target the correct directories. The root README also points to a top-level `README` installation guide, while the checked repository contains `README.md` rather than a separate `README` file. Review and fix these references.
4. **Replace historical placeholders with small navigation pages**, rather than rewriting method documentation that already exists in algorithm READMEs. In particular, make the coupling, optimization, and postprocessing pages link to their maintained counterparts.
5. **Document genuinely shared technical contracts only when confirmed:** how TORC is initialized and scheduled, thread safety, working-directory isolation for subprocesses, and the relationship between in-process library calls and external-model coupling. Distinguish implemented capabilities from ongoing work on reusable APIs and bindings.
6. **Maintain a single platform-support note** (Linux/macOS compilers, MPI compatibility and thread support, GSL and Python dependencies where needed). Do not imply that all components require GSL, MPI, or TORC; for example, the DRAM component is serial and the offline covariance scripts have Python-only requirements.
7. **Treat the website migration as a separate task.** Editing this Markdown report does **not** publish changes to the original `gh-pages` documentation website. A future website update should use the maintained documentation sources and preserve historical attribution and citations.

