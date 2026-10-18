#!/usr/bin/env python3
"""Prepare dataset.txt and testset.txt from a Pi4U curgen file."""

import argparse
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("curgen", type=Path, help="Input population file")
    parser.add_argument(
        "--dim", type=int, required=True,
        help="Number of leading parameter columns",
    )
    parser.add_argument(
        "--leader-indices", type=Path,
        help="Optional text file containing zero-based population row indices",
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("."),
        help="Output directory (default: current directory)",
    )
    args = parser.parse_args()

    if args.dim < 1:
        parser.error("--dim must be positive")

    try:
        population = np.loadtxt(args.curgen, ndmin=2)

        if population.shape[0] < 2:
            raise ValueError("At least two population rows are required.")
        if population.shape[1] < args.dim:
            raise ValueError("Input contains fewer columns than --dim.")

        # Ignore log-likelihood, log-prior, and any other trailing columns.
        dataset = population[:, :args.dim]

        if not np.isfinite(dataset).all():
            raise ValueError("Parameter columns contain NaN or infinity.")

        if args.leader_indices is None:
            testset = dataset
        else:
            indices = np.loadtxt(args.leader_indices, ndmin=1)
            indices = indices.ravel()

            if indices.size == 0:
                raise ValueError("Leader index file is empty.")
            if (
                not np.isfinite(indices).all()
                or np.any(indices != np.floor(indices))
                or np.any(indices < 0)
                or np.any(indices >= len(dataset))
            ):
                raise ValueError(
                    f"Leader indices must be integers from 0 to "
                    f"{len(dataset) - 1}."
                )

            testset = dataset[indices.astype(np.intp)]

        args.output_dir.mkdir(parents=True, exist_ok=True)
        dataset_path = args.output_dir / "dataset.txt"
        testset_path = args.output_dir / "testset.txt"

        # Avoid accidentally overwriting either input.
        inputs = {args.curgen.resolve()}
        if args.leader_indices is not None:
            inputs.add(args.leader_indices.resolve())

        if dataset_path.resolve() in inputs or testset_path.resolve() in inputs:
            raise ValueError("An output path would overwrite an input file.")

        np.savetxt(dataset_path, dataset, fmt="%.16e")
        np.savetxt(testset_path, testset, fmt="%.16e")

    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    print(f"{dataset_path}: {len(dataset)} rows, {args.dim} parameters")
    print(f"{testset_path}: {len(testset)} rows, {args.dim} parameters")
    if args.leader_indices is None:
        print("Both files contain the same parameter points.")


if __name__ == "__main__":
    main()
