# External-model coupling

Pi4U can work with model evaluations implemented outside its optimization or inference engine. This is useful when the application is a pre-existing C, C++, Fortran, Python, MATLAB, or other external scientific program.

## Typical evaluation workflow

1. The Pi4U engine selects a parameter vector.
2. The coupling function sets up a task-specific working directory.
3. Parameters and any required input data are written or copied.
4. An executable or script runs the simulation (and, for inference, may calculate a likelihood).
5. The coupling function reads a scalar result or log-likelihood and returns it to the engine.
6. Temporary files are removed or retained according to the example's settings.

The task execution mechanism is independent from the language of the external model. On MPI clusters, verify that executables, scripts, datasets, and working directories are accessible to the nodes executing model evaluations.

## Implementations

- [Coupling examples README](generated/external-coupling.md): an overview of the supplied examples, requirements, and commands.
- [Rosenbrock example](https://github.com/CEID-HPCLAB/pi4u/tree/main/coupling/rosenbrock): TORC driver launching an external C executable.
- [Simple model example](https://github.com/CEID-HPCLAB/pi4u/tree/main/coupling/simple_model): TMCMC running a Python model and likelihood calculation.

The examples use different filenames and interfaces. Follow the corresponding source and README rather than assuming a universal `params.txt` / `fitness` / `loglike.txt` convention.
