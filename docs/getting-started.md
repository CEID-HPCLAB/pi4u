# Getting started

## Prerequisites

The required dependencies vary by engine. You will generally need a C or C++ compiler and GNU Make. TORC-parallel implementations also require MPI and a separately installed [TORC-Lite runtime](https://github.com/CEID-HPCLAB/torc_lite). Some components additionally require GNU Scientific Library (GSL), Fortran, or Python packages.

**Use the same MPI implementation to build TORC, compile an engine, and launch that engine.**

## Install TORC-Lite for parallel engines

Follow the [TORC-Lite installation guide](https://github.com/CEID-HPCLAB/torc_lite/blob/master/README.md). After installation ensure its helper tools can be found:

```sh
command -v mpicc mpirun torc_cflags torc_libs
torc_cflags
torc_libs
```

## Get Pi4U

```sh
git clone https://github.com/CEID-HPCLAB/pi4u.git
cd pi4u
```

## Build and run an engine

Pi4U engines have individual `Makefile`s and may support different execution modes. Choose a component from **Algorithms** or **Tools** in the sidebar, and follow that component's README for build targets, parameter files, input formats, and run commands.

Some directories support serial and OpenMP builds in addition to TORC; others are serial-only or TORC-only. Do not assume a single top-level `make` builds every module.

## Running parallel work

Run MPI-based engines with the launcher appropriate to your MPI installation and scheduler. The number of MPI processes and the `TORC_WORKERS` setting determine distributed processes and local workers respectively. Consult your chosen engine's instructions for its supported launch command.

## External models

To execute a model implemented in another program or language, see the [coupling guide](coupling.md) and the [coupling examples](generated/external-coupling.md).
