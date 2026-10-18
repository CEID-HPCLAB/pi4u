import numpy as np

dim = 2
rows = np.loadtxt("nn_result.txt", ndmin=2)
covariances = rows.reshape(-1, dim, dim)

for i, cov in enumerate(covariances):
    eigenvalues = np.linalg.eigvalsh(cov)
    std = np.sqrt(np.maximum(np.diag(cov), 0))

    denominator = np.outer(std, std)
    correlation = np.divide(
        cov, denominator,
        out=np.full_like(cov, np.nan),
        where=denominator > 0,
    )

    print(f"\nQuery {i}")
    print("Covariance:\n", cov)
    print("Local standard deviations:", std)
    print("Correlation:\n", correlation)
    print("Eigenvalues:", eigenvalues)
