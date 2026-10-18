# Postprocessing tools

This directory contains R, MATLAB, and GNU Octave tools for visualizing samples and model predictions produced with [Pi4U](https://github.com/CEID-HPCLAB/pi4u).

Two complementary tasks are supported:

- **Parameter analysis:** marginal distributions, pairwise dependence, and scatter plots of sampled parameters.
- **Prediction analysis:** propagate parameter samples through a model and visualize prediction uncertainty alongside observations.

These scripts operate on saved text files; they do not run inference or automatically evaluate a model. MPI and TORC are not required for plotting.

## Files and requirements

| File | Purpose | Requirements |
| --- | --- | --- |
| `plot_samples.R` | Parameter summaries and a matrix of histograms, density estimates, and colored scatter plots; writes PNG. | R, `MASS`, `sp`, and Cairo-capable PNG output. |
| `helpers/kdepairs.R` | Generic plotting interface used by the R script. | R. |
| `helpers/kdepairs.default.R` | Implements the R plotting panels. | R packages above. |
| `plot_curgen.m` | Loads a sample file and calls the custom MATLAB plotter. | MATLAB. |
| `plotmatrix_hist.m` | Marginal histograms, scatter plots, and smoothed pairwise densities. | MATLAB graphics; smoothing helpers are included in the file. |
| `plot_curgen_octave.m` | Basic sample plot matrix saved without an interactive window. | GNU Octave, gnuplot, and PNG rendering support. |
| `plot_propagation.m` | Predictive uncertainty band, highest-likelihood sampled curve, and observations. | MATLAB; `normpdf` requires Statistics and Machine Learning Toolbox for the positive-noise branch. |

The paths under `data/` are defaults in the scripts, not an assurance that the input files are supplied. Create that directory and prepare the relevant files, or edit the script paths. Run commands below from `postprocessing_tools` unless stated otherwise.

## Sample files: select the correct columns

Use headerless, whitespace-separated numeric text, with one sample per row. Set the parameter dimension explicitly in the MATLAB and Octave wrappers.

For TMCMC, a population file typically contains:

```text
theta_0 ... theta_(dim-1) log_likelihood log_prior
```

Only the first `dim` columns are parameter samples. The next column is the log-likelihood; the final column is the log-prior. Other Pi4U algorithms may use different output layouts, so check the producer's format before plotting.

The **R script expects a different layout**:

```text
theta_0 ... theta_(dim-1) score
```

It treats every column except the last as a parameter, and the last column as the score used for scatter coloring. Passing an unmodified TMCMC population file would therefore plot log-likelihood as an extra parameter and use log-prior for coloring.

For a two-parameter TMCMC population, prepare the R input with:

```bash
mkdir -p data
awk '{print $1, $2, $3}' /path/to/curgen_db_007.txt > data/samples.txt
```

Alternatively, keep the original file and select columns immediately after loading it in R:

```r
data <- read.table(fname)
dim <- 2
data <- as.matrix(data[, seq_len(dim + 1), drop = FALSE])
```

The MATLAB and Octave wrappers already select the first `dim` columns and can read the full population file. A final population represents the posterior only if the inference run reached its posterior target; plotting an earlier generation visualizes an intermediate population.

## R: sample summaries and density plots

### Install packages

Inside R:

```r
install.packages(c("MASS", "sp"), repos = "https://cloud.r-project.org")
```

Or from a terminal:

```bash
Rscript -e 'install.packages(c("MASS", "sp"), repos="https://cloud.r-project.org")'
```

`MASS` is often included with an R installation. The `tools` package used by the script is part of R. Use package versions compatible with your R release.

### Configure and run

Edit the user settings near the top of `plot_samples.R`:

```r
fname <- "data/samples.txt"
l <- c(expression(theta[1]), expression(theta[2]))
discrepancy <- 0
truelik <- 0
```

Supply one label per parameter. The filename and labels are script settings, not command-line arguments. Run:

```bash
Rscript plot_samples.R
```

The script sources `helpers/...` relative to the working directory. Running from this directory ensures the helpers are found.

The image is saved **beside the input file with the same basename and a `.png` extension**. For example, `data/samples.txt` produces `data/samples.png`; a selected `curgen_db_007.txt` produces `curgen_db_007.png`. The image is 2500 by 2500 pixels at 300 dpi. It is written directly to disk without requiring an interactive plot window.

### Interpretation

The plot contains:

- Diagonal panels: normalized marginal histograms with boxplots.
- Lower panels: two-dimensional kernel density estimates.
- Upper panels: parameter scatter plots colored by the last input column.

Console output includes means, standard deviations computed using divisor `N`, 5th and 95th percentiles, the parameter vector at the largest score, and a formatted LaTeX table row. The printed `MPVs` are the vector at the highest stored score, not the separately estimated modes of the parameter marginals. A log-likelihood score does not generally identify the MAP sample for a nonuniform prior.

`discrepancy = 1` negates the final column for coloring. `truelik = 1` exponentiates that column, intended for converting log-likelihood to likelihood. Keeping log-likelihood is usually more numerically stable.

**The current script selects its reported best sample before applying those transformations.** With a discrepancy where smaller is better, change the best-sample selection to `which.min(data[, nd])`; setting `discrepancy = 1` alone only changes the plot colors. Constant parameters or a constant score may also require special handling in the histogram, density, or color-bin code.

## MATLAB: sample plot matrix

Edit `plot_curgen.m`:

```matlab
filename = 'data/samples.txt';
dim = 2;
color_by_likelihood = false;
```

Then run in MATLAB:

```matlab
plot_curgen
```

Or from the terminal, keeping MATLAB open to inspect the figure:

```bash
matlab -r "plot_curgen"
```

To save a PNG and exit:

```bash
matlab -batch "plot_curgen; exportgraphics(gcf,'samples_matlab.png','Resolution',300);"
```

On macOS, if MATLAB is not on `PATH`, use its installed executable, for example `/Applications/MATLAB_R2026a.app/bin/matlab`, adjusting the release name.

The wrapper loads the numeric data, selects `data(:,1:dim)`, and calls `plotmatrix_hist`. Its diagonal panels show normalized histograms, upper panels show scatter plots, and lower panels show smoothed two-dimensional histogram densities. With `color_by_likelihood = true`, the upper scatter panels use column `dim + 1` for coloring. That setting assumes this column really is log-likelihood.

`plotmatrix_hist` accepts a matrix rather than a filename, so it can also be called directly:

```matlab
data = load('/absolute/path/to/curgen_db_007.txt');
dim = 2;
figure;
plotmatrix_hist(data(:, 1:dim));
```

Use absolute data paths when invoking scripts from another directory. MATLAB's `run('/path/to/script.m')` can temporarily change the working directory to the script's directory, affecting relative paths.

## Octave: basic plots and graphics troubleshooting

The custom `plotmatrix_hist.m` relies on MATLAB graphics interfaces and should not be assumed to work unchanged in Octave. Use the separate **`plot_curgen_octave.m`**, which calls Octave's built-in `plotmatrix` instead. It produces scatter plots and diagonal histograms, without the custom smoothed density panels or likelihood coloring.

Its current defaults read `data/samples.txt`, select two parameter columns, and write `data/samples_octave.png`. Edit those settings for your data.

```bash
octave-cli plot_curgen_octave.m
```

### Remote Linux from a macOS laptop

Interactive Qt/OpenGL plotting over an SSH/X11 connection can fail with messages such as:

```text
glx: failed to create drisw screen
fatal: caught signal Aborted
```

On macOS, Qt setup problems can instead report:

```text
Could not find the Qt platform plugin "cocoa"
```

A practical route is to generate the PNG on the compute host with **gnuplot and no display**, then view it locally. The provided Octave wrapper uses:

```matlab
graphics_toolkit('gnuplot');
setenv('GNUTERM', 'pngcairo');
set(0, 'defaultfigurevisible', 'off');
```

On Ubuntu/Debian, install the noninteractive gnuplot package if needed:

```bash
sudo apt install gnuplot-nox
```

Run on Linux:

```bash
DISPLAY= octave-cli --no-init-file plot_curgen_octave.m
```

From a local terminal on your Mac:

```bash
scp user@host:~/pi4u/postprocessing_tools/data/samples_octave.png .
open samples_octave.png
```

Replace the host, username, and path with your actual values. No X11 forwarding is needed for this workflow.

### Warnings and failures

The warning that the gnuplot toolkit is discouraged describes its maintenance and interaction limitations; it does not itself indicate a failed PNG export.

An error mentioning **FLTK requires a visible figure** indicates that the figure is using a different graphics toolkit. Select gnuplot before creating any figures, restart with `octave-cli --no-init-file`, and check:

```matlab
available_graphics_toolkits()
graphics_toolkit()
```

For `unable to create figure handle`, isolate figure creation from the sample script:

```bash
DISPLAY= octave-cli --no-init-file --eval "graphics_toolkit('gnuplot'); figure('visible','off'); plot(1:10); print('test.png','-dpng');"
```

If this minimal example fails, the graphics installation needs attention before the sample plotter can work. Check that `gnuplot` is available and inspect the complete error for missing fonts or rendering dependencies. An invisible figure still needs a functioning rendering backend.

These instructions concern the basic Octave wrapper. They do not establish Octave compatibility for the full propagation script, whose statistics functions, histogram APIs, and transparency behavior may require adaptation.

## MATLAB: prediction uncertainty with `plot_propagation.m`

This script plots an ensemble of model predictions against observations. A typical workflow is:

1. Obtain parameter samples from a completed inference calculation.
2. Evaluate the model for each sample on a common prediction grid.
3. Save the parameter values, noise variance, log-likelihood, and predicted curve in the format below.
4. Run `plot_propagation.m` to construct a prediction band and comparison plot.

An ordinary `curgen` file does not contain the simulated curves required here. The current `tools/propagation_tool` console output is also not this file format; adapt the model evaluation/output stage to save the curves explicitly.

### Simulation file

Default settings:

```matlab
simfile = 'data/propagation_sim.txt';
datfile = 'data/propagation_dat.txt';
dim = 1;
```

Each simulation row has `dim + 2 + 2*M` columns:

```text
parameter_1 ... parameter_dim noise_variance log_likelihood x1 y1 ... xM yM
```

`dim` counts the model parameters preceding the noise column; it excludes that separately stored noise quantity. All rows must have the same number of predictions at the same ordered x coordinates. The band uses the x coordinates from the first simulation row.

For one model parameter and three prediction locations, an illustrative dataset is:

```text
1.8 0.09 -13.2 0.0 0.0 1.0 1.8 2.0 3.6
2.0 0.09 -12.5 0.0 0.0 1.0 2.0 2.0 4.0
2.2 0.16 -13.0 0.0 0.0 1.0 2.2 2.0 4.4
```

The script comments call the noise column `sigma`, but the implementation passes `sqrt(sigma)` to `normpdf`. **The current implementation therefore expects a variance**, such as `0.09` for a standard deviation of `0.3`. If your inference samples store a standard deviation, square it when constructing this file or consistently change the script's convention.

### Observation file

Each row contains:

```text
x_observed y_observed error_bar
```

For example:

```text
0.0 0.1 0.2
1.0 2.1 0.2
2.0 3.9 0.3
```

The third column is passed directly to `errorbar`, producing `y ± error_bar`. Whether it represents a standard deviation, standard error, or another interval must be defined by the data provider. Unlike the simulation noise column, this quantity is not square-rooted.

### Calculation and meaning of the figure

For positive simulation variances, the script estimates a density at each prediction location using an equally weighted mixture:

$$
p(y\mid x,D)\approx\frac{1}{S}\sum_{s=1}^{S}
\mathcal N\!\left(y;f(x;\theta_s),v_s\right).
$$

Parameter samples contribute variation in model predictions; the Gaussian components add noise with variances `v_s`. The log-likelihood column is used to select a curve, not to weight this mixture. The rows must therefore already represent the intended sample distribution, or be resampled appropriately before use.

The script evaluates densities on a grid (`nmesh_y = 1000`), normalizes them, approximates their CDFs, and extracts:

```matlab
myqq = [0.05 0.95];
```

The resulting plot contains:

- A shaded **pointwise 90% predictive interval**, between the 5th and 95th percentiles.
- The curve from the simulation row with the highest stored log-likelihood.
- Observations with their supplied error bars.

The solid curve is not a posterior mean and is not necessarily a MAP curve. The band is pointwise, rather than a simultaneous 90% region for entire curves. In the histogram branch, no Gaussian noise is added, so the band instead summarizes variation in the supplied model predictions alone.

### Configure and run

Edit the filenames, `dim`, labels, and limits at the top of the script:

```matlab
name_x = 'x';
name_y = 'model output';
lim_x = [0 2];
lim_y = [-1 6];
```

The supplied shear-rate/TTF labels and axis limits belong to an earlier application and should be changed. Then run in MATLAB:

```matlab
plot_propagation
```

Or from a terminal:

```bash
matlab -r "plot_propagation"
```

For export and exit:

```bash
matlab -batch "plot_propagation; exportgraphics(gcf,'propagation.png','Resolution',300);"
```

`isplot = 0` skips figure creation while retaining the calculated arrays in the MATLAB workspace when run interactively. The script does not automatically save those arrays or export the figure.

### Numerical details to review before scientific use

The following are **suggested improvements**, rather than behavior already implemented in the script.

**Noise validation.** The current test `prod(sim_s) > 0` can underflow when many samples are present, accepts some invalid inputs, and routes mixed zero/positive variances into the histogram branch. Replace it with explicit cases for finite positive variances and all-zero variances, and reject or deliberately handle mixed cases:

```matlab
if all(isfinite(sim_s) & sim_s > 0)
    % Positive-variance Gaussian mixture branch
elseif all(sim_s == 0)
    % Noise-free histogram branch
else
    error('Handle invalid or mixed zero/positive variances explicitly.');
end
```

**Prediction grid.** The current grid depends on the range of simulated means and can truncate Gaussian tails. For positive variances, a more suitable starting range is:

```matlab
sd = sqrt(sim_s);
min_val_y = min(bsxfun(@minus, sim_y, 5*sd), [], 1);
max_val_y = max(bsxfun(@plus,  sim_y, 5*sd), [], 1);
```

Apply this before computing `mesh_step_y`. Check grid resolution and range sensitivity. Constant predictions can produce a zero-width grid, particularly in noise-free calculations, and need explicit handling. The existing CDF is a rectangular cumulative sum; trapezoidal integration with consistent normalization is another useful refinement for accurate quantiles.

## Interpretation and reproducibility

Keep the input data, inference settings, sample generation number, script settings, and software versions with exported figures. Preserve repeated parameter samples: deleting duplicates changes their empirical weights. Pair plots and marginal histograms are useful diagnostics, but they do not establish convergence or sample independence.

## Background and license

For the Pi4U framework, see P. E. Hadjidoukas, P. Angelikopoulos, C. Papadimitriou, and P. Koumoutsakos, *Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models*, Journal of Computational Physics 284 (2015), 1–21. [DOI: 10.1016/j.jcp.2014.12.006](https://doi.org/10.1016/j.jcp.2014.12.006).

Further information is available in the [Pi4U documentation](https://phadjido.github.io/pi4u/). This README consolidates the plotting instructions from the historical `README.txt` and documents the current scripts. Consult the repository-level license and individual source notices; retain third-party notices, including those for the smoothing routines embedded in `plotmatrix_hist.m`.
