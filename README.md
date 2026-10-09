# Pi4U

**Parallel uncertainty quantification and optimization with TORC.**

Pi4U (Π4U) is a framework for Bayesian inference, uncertainty quantification, and numerical optimization of computationally demanding models. It combines sampling and optimization algorithms with task-based parallel execution, allowing expensive model evaluations to run across multicore systems and distributed-memory clusters.

Pi4U is designed for workflows in which repeated model evaluations dominate the computational cost: calibrating simulation parameters, exploring posterior distributions, searching for optimal designs, and estimating rare-event probabilities. Models can be integrated directly or coupled through external programs.

This repository is the home of the Pi4U continuation at **CEID-HPCLAB, University of Patras**, led by **Panagiotis Hadjidoukas**, the original framework's main developer.

## Why Pi4U?

- **Parallel model evaluations.** Distribute expensive evaluations using the TORC tasking runtime.
- **A common foundation for multiple algorithms.** Use inference, optimization, and reliability methods within the same framework.
- **External-model coupling.** Connect existing simulation software through model-evaluation functions and file-based interfaces.
- **Support for scientific computing workflows.** Work with configurable algorithm engines, sample databases, numerical differentiation, and postprocessing tools.
- **Extensible source code.** Adapt existing engines and coupling examples to new models and research problems.

## Algorithms and capabilities

The current source tree includes the following components. Parallel execution options and dependencies vary by engine; the DRAM implementation is sequential, and asynchronous CMA-ES is experimental.

| Area | Algorithms and tools | Source directory |
| --- | --- | --- |
| Bayesian inference | Transitional Markov chain Monte Carlo (TMCMC) | [`inference/TMCMC`](inference/TMCMC/) |
| Bayesian inference | Delayed rejection adaptive Metropolis (DRAM) | [`inference/DRAM`](inference/DRAM/) |
| Approximate Bayesian computation | ABC with subset simulation (ABC-SubSim) | [`inference/ABC_SubSim`](inference/ABC_SubSim/) |
| Single-objective optimization | CMA-ES, including an experimental asynchronous engine | [`singleopt/cmaes`](singleopt/cmaes/) |
| Single-objective optimization | AMaLGaM | [`singleopt/AMaLGaM`](singleopt/AMaLGaM/) |
| Multiobjective optimization | NSGA-II | [`multiobj/nsga2`](multiobj/nsga2/) |
| Rare-event estimation | Subset simulation with predefined or adaptive levels | [`rare/subset`](rare/subset/) |
| Numerical differentiation | Parallel numerical differentiation library (PNDL), gradient and Hessian tools | [`derivatives`](derivatives/) |
| Hierarchical inference | Hierarchical Bayesian modeling code and examples | [`hierarchical`](hierarchical/) |

Additional directories provide [external-model coupling examples](coupling/), [visualization](display/), and [postprocessing tools](postprocessing_tools/).

## Parallel execution with TORC

The task-based parallel implementations in Pi4U use **TORC**, a runtime library developed by Panagiotis Hadjidoukas. TORC schedules model-evaluation tasks across worker threads and MPI processes, providing a common execution layer for shared- and distributed-memory systems.

The repository includes the [`torc_lite`](torc_lite/) runtime. Some engines also provide serial or OpenMP execution modes. Build options are defined in each engine's Makefile.

This repository contains the original implementation, primarily in C, with supporting Fortran code and scripts.

## Getting started

Clone the repository:

```bash
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u
```

Start with the [build and installation guide](README), which covers prerequisites, TORC installation, engine compilation, and example runs. Depending on the selected component, you will need C and/or Fortran compilers, an MPI implementation, and the GNU Scientific Library (GSL).

After installing TORC and adding its helper scripts to your `PATH`, build the TORC-based TMCMC engine with:

```bash
cd inference/TMCMC
make use_torc=1
```

The bundled example evaluates a two-dimensional Rosenbrock function. Its model-evaluation function is defined in `fitfun.c`, and runtime settings are provided in `tmcmc.par`. Consult the installation guide for running and visualizing the example.

For an example of connecting an external Python model, see [`coupling/simple_model`](coupling/simple_model/).

**Documentation note:** the installation guide and some component documentation were written for earlier releases. Compiler settings, dependency paths, directory names, and historical contact details may need updating for your environment. For example, the current CMA-ES directory is `singleopt/cmaes`.

## Project history and continuation

Pi4U was originally developed at the Computational Science and Engineering Laboratory (CSELAB), ETH Zurich, with Panagiotis Hadjidoukas as its main developer and contributions from the collaborators listed in [`AUTHORS`](AUTHORS). The [original repository](https://github.com/cselab/pi4u) records that development history.

Development is being resumed independently at CEID-HPCLAB, University of Patras. This continuation builds on the original code and preserves its authorship, acknowledgments, and license notices. References to ETH Zurich and CSELAB describe the project's origins; the continuation is maintained by the University of Patras group.

The inherited codebase is the starting point for renewed development. Modernization and future extensions will be documented as they become available.

## Citing Pi4U

If you use Pi4U in research, please cite the original framework paper:

P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos. **Π4U: A high performance computing framework for Bayesian uncertainty quantification of complex models.** *Journal of Computational Physics*, 284, 1–21, 2015. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).

```bibtex
@article{hadjidoukas2015pi4u,
  title   = {{Pi4U}: A high performance computing framework for
             Bayesian uncertainty quantification of complex models},
  author  = {Hadjidoukas, P. E. and Angelikopoulos, P. and
             Papadimitriou, C. and Koumoutsakos, P.},
  journal = {Journal of Computational Physics},
  volume  = {284},
  pages   = {1--21},
  year    = {2015},
  doi     = {10.1016/j.jcp.2014.12.006}
}
```

Please also cite the relevant algorithm publications when using individual methods.

## Contributing and support

Bug reports, documentation improvements, reproducible examples, and algorithm contributions are welcome. Use the [issue tracker](https://github.com/CEID-HPCLAB/pi4u/issues) for questions and bug reports, or submit a pull request with a clear description of the change.

For build or runtime issues, include the engine, compiler and MPI versions, build command, relevant configuration, and error output.

## License

The repository includes the GNU General Public License, version 2, in [`COPYING`](COPYING). Individual components and bundled third-party code may carry their own license terms; consult the corresponding files and source headers.
