# Pi4U

**Parallel uncertainty quantification and optimization with TORC.**

Pi4U is a research software framework for Bayesian inference, numerical optimization, and related computational methods. Several engines distribute model evaluations as tasks using the [TORC-Lite](https://github.com/CEID-HPCLAB/torc_lite) runtime, allowing execution on multicore machines and MPI clusters.

This is the documentation for the independently maintained [CEID-HPCLAB/pi4u](https://github.com/CEID-HPCLAB/pi4u) continuation at the University of Patras.

## Find what you need

- **[Get started](getting-started.md)**: prerequisites and where to find commands for each engine.
- **[TORC architecture](architecture.md)**: task-based execution and shared concerns.
- **[External-model coupling](coupling.md)**: connect an executable or script to an engine.
- **Algorithms**: TMCMC, DRAM, ABC-SubSim, CMA-ES, AMaLGaM, NSGA-II, MOEA/D, MO-CMA-ES, and subset simulation are documented in the sidebar.
- **Tools**: derivative computation, sensitivity analysis, sample postprocessing and covariance scripts.

## Documentation policy

Algorithm pages on this website are **automatically generated from the matching README files in the source repository** on each deployment. Edit an engine's `README.md` to update its documentation; there is no separately maintained website copy.

This site's introductory pages explain cross-cutting concepts only. The repository's historical [documentation report](https://github.com/CEID-HPCLAB/pi4u/blob/main/doc/documentation_report.md) preserves the earlier website's content; it contains historical commands and paths, which should not be assumed current.

## Citing Pi4U

See [References and project history](references.md) for the original framework paper and the project's development history.
