#!/usr/bin/env python3
"""Compute local sample covariances at query points."""

import time

import numpy as np
from sklearn.neighbors import NearestNeighbors


def main():
    start = time.perf_counter()

    # Files must contain parameter coordinates only.
    samples = np.loadtxt("dataset.txt", ndmin=2)
    testset = np.loadtxt("testset.txt", ndmin=2)

    n, dim = samples.shape
    ntest, test_dim = testset.shape

    if n < 2 or dim < 1:
        raise ValueError(
            "dataset.txt must contain at least two rows and one column."
        )
    if ntest < 1:
        raise ValueError("testset.txt must contain at least one row.")
    if test_dim != dim:
        raise ValueError("Dataset and test-set dimensions differ.")
    if not np.isfinite(samples).all():
        raise ValueError("dataset.txt contains NaN or infinity.")
    if not np.isfinite(testset).all():
        raise ValueError("testset.txt contains NaN or infinity.")

    # Preserve the original neighborhood-size rule.
    nneighbors = min(max(5 * (dim + 1), 50), n)

    neighbors = NearestNeighbors(
        n_neighbors=nneighbors,
        algorithm="auto",
        metric="euclidean",
    ).fit(samples)

    indices = neighbors.kneighbors(testset, return_distance=False)

    result = np.empty((ntest, dim * dim))
    non_positive_definite = 0

    for k, neighbor_indices in enumerate(indices):
        local_samples = samples[neighbor_indices]

        # Sample covariance, normalized by nneighbors - 1.
        covariance = np.atleast_2d(
            np.cov(local_samples, rowvar=False, ddof=1)
        )

        # Check without changing or regularizing the matrix.
        try:
            np.linalg.cholesky(covariance)
        except np.linalg.LinAlgError:
            non_positive_definite += 1

        # One flattened matrix per query, in row-major order.
        result[k] = covariance.ravel(order="C")

    np.savetxt("nn_result.txt", result, fmt="%.16e")

    print(f"Dataset: {n} points, {dim} dimensions")
    print(f"Queries: {ntest}; neighbors per query: {nneighbors}")
    print("Saved covariance matrices to nn_result.txt")

    if non_positive_definite:
        print(
            f"Warning: {non_positive_definite} covariance matrices failed "
            "the positive-definiteness check. No regularization was applied."
        )

    print(f"Elapsed time: {time.perf_counter() - start:.6f} secs")


if __name__ == "__main__":
    main()
