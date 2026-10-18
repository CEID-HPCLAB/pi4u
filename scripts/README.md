# Local covariance analysis

These Python 3 utilities prepare parameter samples from a Pi4U population file, estimate nearest-neighbor covariance matrices, and inspect their local scales, correlations, and eigenvalues.

They provide an **offline analysis workflow** for studying the geometry of a sampled distribution. They do not run TMCMC, select its chain leaders, modify its proposals, or automatically compare their results with the C engine.

## Scripts

| Script | Purpose |
| --- | --- |
| `prepare_covariance_inputs.py` | Extract leading parameter columns from a population file and prepare the dataset and query points. |
| `compute_chain_covariance.py` | Find neighbors around each query and write a local sample covariance matrix. |
| `analyze_results.py` | Print covariance matrices, local standard deviations, correlations, and eigenvalues. |

## Requirements

Install Python 3, NumPy, and scikit-learn:

```bash
python3 -m pip install numpy scikit-learn
```

The preparation and analysis scripts need only NumPy. The covariance script also uses scikit-learn's `NearestNeighbors`. No MPI, TORC, plotting backend, MATLAB, or R is required.

## Quick start

From this directory, for a two-parameter population:

```bash
python3 prepare_covariance_inputs.py /path/to/curgen_db_007.txt --dim 2
python3 compute_chain_covariance.py
python3 analyze_results.py > covariance_analysis.txt
```

Before the last command, make sure `dim = 2` in `analyze_results.py` matches the dimension used in the first command. The analysis script currently uses a source-code setting rather than a command-line option.

The workflow creates:

| File | Contents |
| --- | --- |
| `dataset.txt` | All population parameter vectors used as the neighbor-search dataset. |
| `testset.txt` | Query points where local covariance is evaluated. By default, the same points as the dataset. |
| `nn_result.txt` | One flattened covariance matrix per query. |
| `covariance_analysis.txt` | Optional redirected analysis output. |

Generated files with these names are overwritten on subsequent runs. Use separate working directories to preserve independent analyses.

## 1. Prepare the inputs

A TMCMC population file typically contains:

```text
theta_0 ... theta_(dim-1) log_likelihood log_prior
```

The preparation script keeps **only the first `dim` columns**, excluding log-likelihood, log-prior, and any other trailing fields. It can also accept other numeric population formats with leading parameter columns.

```bash
python3 prepare_covariance_inputs.py curgen_db_007.txt --dim 2
```

The files have no headers, with one parameter vector per row:

```text
x0 x1
```

At least two population rows are required. The selected parameters must be finite. Population row order and repeated samples are preserved.

### Query selected population points

By default, `testset.txt` equals `dataset.txt`, so each population point gets its own local covariance estimate. To evaluate a subset, provide zero-based row indices:

```text
0
5
12
```

Save those indices as `leaders.txt` and run:

```bash
python3 prepare_covariance_inputs.py curgen_db_007.txt \
    --dim 2 --leader-indices leaders.txt
```

This keeps the entire population in `dataset.txt`, but queries only its first, sixth, and thirteenth rows, in that order. Repeated indices are allowed and produce repeated query rows.

The option is named `--leader-indices`, but the script **does not identify or resample chain leaders**. To analyze actual TMCMC leaders, export their indices from the engine for the corresponding population. A `curgen` file alone does not encode the subsequent leader selection. Any valid population-row subset can also be used for offline investigation.

### Separate working directory

Only the preparation script accepts `--output-dir`. The other two scripts use fixed filenames in their current working directory:

```bash
# From pi4u/scripts:
python3 prepare_covariance_inputs.py /path/to/curgen_db_007.txt \
    --dim 2 --output-dir analysis_gen007

cd analysis_gen007
python3 ../compute_chain_covariance.py
python3 ../analyze_results.py > covariance_analysis.txt
```

## 2. Compute local covariance matrices

```bash
python3 compute_chain_covariance.py
```

The script reads `dataset.txt` and `testset.txt`. Both must have the same number of columns and contain finite numeric parameter values. Query points can also be supplied independently of the preparation script, provided they use the same coordinates, units, and column order.

For a dataset of `N` points in `d` dimensions, the neighbor count is:

```python
k = min(max(5 * (d + 1), 50), N)
```

For each query, the script selects its `k` nearest dataset rows using **raw Euclidean distance**, and computes:

$$
C=\frac{1}{k-1}\sum_{r=1}^{k}
(x_r-\bar x)(x_r-\bar x)^T,
\qquad \bar x=\frac{1}{k}\sum_{r=1}^{k}x_r.
$$

This is an unweighted sample covariance centered at the neighborhood mean, rather than at the query point. Repeated dataset rows contribute repeatedly. When querying the dataset itself, a matching point is eligible as a zero-distance neighbor; this is not a leave-one-out calculation.

The script checks positive definiteness using Cholesky factorization and prints the number of failed checks. **It writes the matrices unchanged**, including singular ones; it does not add jitter, remove duplicates, or regularize eigenvalues.

### Output format

`nn_result.txt` has one row per query and `d*d` columns, flattened in row-major order. For two parameters:

```text
C00 C01 C10 C11
```

Output row `i` corresponds to row `i` of `testset.txt`. Keep that file alongside the results to retain the query coordinates. With selected indices, the output row number refers to query order, not directly to the original population-row number.

## 3. Analyze the results

Set the dimension in `analyze_results.py`:

```python
dim = 2
```

Then run:

```bash
python3 analyze_results.py
```

For each query, starting at index zero, it prints:

- The local covariance matrix.
- Local standard deviations, obtained from the covariance diagonal.
- The normalized correlation matrix.
- Eigenvalues, returned in ascending order by `eigvalsh`.

Correlations involving a zero-variance coordinate are printed as `NaN` because their normalization is undefined. The script clips negative diagonal values to zero when taking square roots, but does not repair the covariance itself.

**Set the dimension correctly:** the current analysis script reshapes the complete array without explicitly validating that each input row has exactly `dim*dim` columns. A wrong dimension can either fail or silently misinterpret the file when the total number of elements happens to fit.

For large populations, redirect the output to a file rather than printing every matrix to the terminal.

## Interpreting a population's local geometry

| Observation | What it can indicate |
| --- | --- |
| Small local standard deviations | A tightly concentrated neighborhood at that location. |
| Different scales between parameters | Different local variability; assess relative to parameter units and prior ranges. |
| Correlation near +1 or -1 | Strong local linear dependence or a parameter trade-off. |
| One eigenvalue much larger than another | An elongated neighborhood, such as a ridge or narrow valley. |
| Covariances change across query locations | Spatially varying scales or dependence that a global covariance may describe poorly. |
| Zero or near-zero eigenvalues | Duplicate points, collinearity, constrained geometry, or insufficient independent variation. |

For a curved target such as Rosenbrock, neighborhoods may be elongated with directions that change along the valley. The analysis script prints eigenvalues but not eigenvectors; inspect eigenvectors or pair plots if you want to study those changing directions.

These are local geometric diagnostics, not convergence tests. Small variances can also result from poor exploration or repeated states. They do not establish identifiability, posterior uncertainty, or multimodality by themselves. Combine them with parameter pair plots, unique-sample counts, and independent inference runs. For an intermediate `curgen` generation, conclusions apply to its intermediate distribution rather than the final posterior.

## Limitations and practical guidance

- **Small datasets:** when `N <= max(5*(d+1), 50)`, every query uses the whole dataset, so all covariance matrices are identical. In two dimensions this occurs for `N <= 50`.
- **Parameter scaling:** raw Euclidean distance favors coordinates with larger numerical ranges. If scaling is needed, apply it consistently to dataset and queries and document whether the reported covariance is in scaled or physical units. These scripts do not scale automatically.
- **Neighborhood sensitivity:** a fixed neighbor count defines different physical radii in dense and sparse regions. Repeat the analysis with other counts by editing the covariance script when assessing robustness. Equal-distance neighbor ties can affect the selected subset.
- **Dependent samples:** the `k-1` denominator is the conventional sample-covariance normalization. It does not correct for correlations between Markov chain states or turn this local covariance into the uncertainty of an estimated mean.
- **Degeneracy:** more neighbors do not guarantee a positive-definite covariance. Cholesky failures are particularly relevant when matrices are intended for Gaussian sampling.
- **Cost:** querying every population point produces one matrix per sample. Use selected query indices to reduce runtime and output size for large populations or high dimensions.

The matrices can support external comparisons with the C implementation, but the scripts perform no such comparison automatically. Match neighborhood selection, scaling, weighting, and normalization before expecting numerical agreement.

They also do not feed `nn_result.txt` back into TMCMC. Using local covariance in a sampler requires deliberate integration and validation of the transition rule. A proposal whose covariance changes with the current state generally requires the corresponding Metropolis–Hastings proposal-density correction.

## Further information

- [Pi4U repository](https://github.com/CEID-HPCLAB/pi4u)
- [TMCMC implementation](../inference/TMCMC)
- [Parameter and prediction plotting tools](../postprocessing_tools)

These utilities are distributed as part of Pi4U. See the repository-level license and source notices for applicable terms.
