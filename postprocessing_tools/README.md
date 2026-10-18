# Python postprocessing for Pi4U

Python equivalents of Pi4U's R/MATLAB sample and propagation plotting tools. Figures **display by default**. Use `--save FILE --no-show` to generate images on servers without Qt, OpenGL, X11 forwarding, or a display.

## Install

```bash
python3 -m pip install -r requirements.txt
```

The scripts use NumPy, SciPy, and Matplotlib. They do not require MPI, TORC, MATLAB, Octave, or R.

## Sample distributions: `plot_samples.py`

Read a numeric text file with one sample per row. Select the number of leading parameter columns with `--dim`:

```bash
python3 plot_samples.py curgen_db_007.txt --dim 2
```

The script accepts TMCMC's complete format:

```text
theta_0 ... theta_(dim-1) log_likelihood log_prior
```

It plots only the first `dim` columns. The default score/color column is **`dim + 1`**, using 1-based column numbers, if present. Extra columns, including TMCMC's final log-prior, are ignored. Parameter-only files are supported without coloring.

```bash
python3 plot_samples.py final.txt --dim 2 \
    --labels 'theta 1' 'theta 2' --save samples.png

python3 plot_samples.py samples.txt --dim 3 --score-column 5 \
    --score-label 'Objective' --minimize

python3 plot_samples.py final.txt --dim 2 --no-color
```

Use `--minimize` when a smaller score is better. It controls the best-sample report and reverses the score colormap; it does not negate or exponentiate the data. With a log-likelihood, the default maximization convention is appropriate. The best likelihood sample is not necessarily a MAP estimate.

Panels:

- Diagonal: normalized marginal histograms.
- Upper triangle: scatter plots, colored by score when available.
- Lower triangle: normalized two-dimensional histograms, optionally smoothed with a Gaussian filter.

These lower-panel estimates follow the MATLAB histogram approach; they are not the R script's exact kernel density estimator. Adjust `--bins 40` and `--smooth 1.0` as needed; `--smooth 0` disables smoothing. Constant parameters are accepted.

The console reports parameter means, population standard deviations (`ddof=0`), 5th/50th/95th percentiles, and the best-score input row and parameter vector. All samples, including repeats, contribute to statistics and density panels. Scatter panels display a reproducible subset of at most 10,000 points by default; use `--max-scatter 0` to display all points. For high dimensions, the matrix becomes large; reduce the input parameter set if needed.

## Prediction uncertainty: `plot_propagation.py`

The simulation file uses the MATLAB propagation format, with one row per equally weighted simulation:

```text
parameter_1 ... parameter_dim noise_value log_likelihood x1 y1 x2 y2 ... xM yM
```

`--dim` counts the leading model parameters, **excluding the separate noise column**. All rows must contain the same prediction x grid. The script sorts that grid for plotting and rejects duplicate x coordinates. The optional observation file has exactly three columns:

```text
x_observed y_observed nonnegative_error_bar
```

Run:

```bash
python3 plot_propagation.py propagation_sim.txt \
    --data propagation_dat.txt --dim 1
```

By default, `noise_value` is a **variance**, matching what the existing MATLAB implementation actually computes. If your files store standard deviations, select this explicitly:

```bash
python3 plot_propagation.py propagation_sim.txt --dim 1 \
    --noise-kind std --xlabel 'Time' --ylabel 'Response' \
    --save propagation.pdf --export prediction_intervals.csv
```

Options:

- `--noise-kind variance`: variance column (default).
- `--noise-kind std`: square the supplied standard deviation before calculation.
- `--noise-kind none`: ignore the noise column and summarize model curves alone. The placeholder noise column must still be present and finite.
- `--quantiles 0.025 0.975`: use a pointwise 95% interval rather than the default 90% interval.
- `--curve best|mean|median|none`: central curve selection; `best` is the highest-log-likelihood sampled curve (default). `mean` and `median` summarize noise-free model predictions, not the predictive mixture's median.
- `--xlim MIN MAX`, `--ylim MIN MAX`: optional axis limits.
- `--export FILE`: CSV columns `x,lower,upper,mean_model,median_model,best_model`.

For positive noise, intervals are quantiles of an equally weighted Gaussian mixture at each prediction point. The implementation evaluates its CDF directly with SciPy and solves for quantiles by bisection, rather than integrating a finite density grid. Zero-variance components are treated as point masses, so mixed zero/positive variances are supported. If all variances are zero, NumPy's default interpolated empirical quantiles summarize the model predictions. The bisection range extends twelve component standard deviations; use ordinary probabilities such as the defaults, rather than extreme Gaussian tail probabilities. Negative noise values are rejected unless the noise column is explicitly ignored.

The shaded region is a **pointwise interval**, not a simultaneous confidence region for entire curves. For a posterior predictive interpretation, the simulations must represent posterior samples, and the added Gaussian noise must match the intended observation/prediction model. The stored log-likelihood selects the best curve; it does not weight the ensemble. Retain duplicate samples or resample weighted ensembles correctly before plotting.

Ordinary `curgen` files contain no prediction curves. First evaluate the model at the parameter samples and write the simulation format above. The scalar console output of `tools/propagation_tool` is not sufficient for this script.

## Remote Linux from a Mac

Generate images without a GUI:

```bash
python3 plot_samples.py final.txt --dim 2 --save samples.png --no-show

python3 plot_propagation.py propagation_sim.txt --dim 1 \
    --data propagation_dat.txt --save propagation.png --no-show
```

Both scripts select Matplotlib's noninteractive Agg backend before importing pyplot when `--no-show` is set. This avoids the Qt cocoa/GLX/FLTK issues encountered with Octave. Download and view the image from a terminal on your Mac:

```bash
scp user@host:~/path/to/samples.png .
open samples.png
```

Interactive display requires a working local Matplotlib GUI backend; a remote machine without one should use `--save ... --no-show`. Without `--no-show`, a saved figure also opens interactively.

## Included examples

From this directory:

```bash
python3 plot_samples.py examples/samples.txt --dim 2
python3 plot_propagation.py examples/propagation_sim.txt \
    --data examples/propagation_dat.txt --dim 1
```

Run either script with `--help` for all options. Input files must be numeric and rectangular; NumPy accepts comment lines beginning with `#`. Nonfinite values in selected parameters, scores, and propagation inputs are rejected. Statistics and plotting do not by themselves establish inference convergence or sample independence.
