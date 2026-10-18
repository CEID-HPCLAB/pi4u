#!/usr/bin/env python3
"""Pointwise predictive intervals from Pi4U simulation-curve ensembles."""
import argparse
from pathlib import Path
import numpy as np
from scipy.special import ndtr


def predictive_quantiles(means, variances, probabilities):
    """Equal-weight Gaussian mixture, with zero-variance components as atoms.

    All-zero noise uses NumPy's interpolated empirical quantiles. Otherwise
    bisection evaluates the mixture CDF, including any point-mass components.
    """
    probabilities = np.asarray(probabilities)
    if np.all(variances == 0):
        return np.quantile(means, probabilities, axis=0)
    positive = variances > 0
    sd = np.sqrt(variances[positive])[:,None]
    lo = np.min(means - 12*np.sqrt(variances)[:,None], axis=0)
    hi = np.max(means + 12*np.sqrt(variances)[:,None], axis=0)
    result = []
    for q in probabilities:
        left, right = lo.copy(), hi.copy()
        for _ in range(90):
            mid = left + (right-left)*.5
            cdf = ndtr((mid-means[positive])/sd).sum(axis=0)
            if not positive.all():
                cdf += (means[~positive] <= mid).sum(axis=0)
            cdf /= len(means)
            low = cdf < q
            left = np.where(low, mid, left)
            right = np.where(low, right, mid)
        result.append(left + (right-left)*.5)
    return np.asarray(result)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('simfile', type=Path)
    p.add_argument('--data', type=Path, help='Optional observation file: x y error_bar')
    p.add_argument('--dim', type=int, required=True, help='Leading model parameters, excluding noise column')
    p.add_argument('--noise-kind', choices=['variance','std','none'], default='variance')
    p.add_argument('--quantiles', nargs=2, type=float, default=[.05,.95], metavar=('LOW','HIGH'))
    p.add_argument('--curve', choices=['best','mean','median','none'], default='best')
    p.add_argument('--xlabel', default='x')
    p.add_argument('--ylabel', default='Model output')
    p.add_argument('--xlim', nargs=2, type=float)
    p.add_argument('--ylim', nargs=2, type=float)
    p.add_argument('--export', type=Path, help='Write x lower upper mean_model median_model best_model as CSV')
    p.add_argument('--save', type=Path)
    p.add_argument('--no-show', action='store_true', help='Use a noninteractive backend; requires --save')
    a = p.parse_args()
    if a.dim < 0 or not 0 < a.quantiles[0] < a.quantiles[1] < 1:
        p.error('dim must be nonnegative and 0 < LOW < HIGH < 1')
    if a.no_show and not a.save:
        p.error('--no-show requires --save')
    try:
        sim = np.loadtxt(a.simfile, ndmin=2)
        ncurve = sim.shape[1]-a.dim-2
        if len(sim) < 1 or ncurve < 2 or ncurve % 2:
            raise ValueError('Expected parameters, noise, log-likelihood, and paired x/y prediction columns')
        if not np.isfinite(sim).all():
            raise ValueError('Simulation input must contain finite numeric values')
        noise = sim[:,a.dim]
        if a.noise_kind != 'none' and np.any(noise < 0):
            raise ValueError('Noise variances/standard deviations must be nonnegative')
        variance = np.zeros_like(noise) if a.noise_kind == 'none' else noise**2 if a.noise_kind == 'std' else noise
        if not np.isfinite(variance).all():
            raise ValueError('Noise conversion overflowed')
        xx, yy = sim[:,a.dim+2::2], sim[:,a.dim+3::2]
        if not np.allclose(xx, xx[0], rtol=1e-10, atol=1e-12):
            raise ValueError('All simulations must share the same prediction x grid')
        order = np.argsort(xx[0], kind='stable')
        x, yy = xx[0,order], yy[:,order]
        if np.any(np.diff(x) <= 0):
            raise ValueError('Prediction x coordinates must be distinct')
        obs = None
        if a.data:
            obs = np.loadtxt(a.data, ndmin=2)
            if obs.shape[1] != 3 or not np.isfinite(obs).all() or np.any(obs[:,2] < 0):
                raise ValueError('Observations require exactly three finite columns: x y nonnegative_error_bar')
        band = predictive_quantiles(yy, variance, a.quantiles)
        best_id = int(np.argmax(sim[:,a.dim+1]))
    except (ValueError, OSError) as exc:
        p.error(str(exc))
    mean, median, best = yy.mean(axis=0), np.median(yy, axis=0), yy[best_id]
    import matplotlib
    if a.no_show:
        matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(8,5), layout='constrained')
    coverage = 100*(a.quantiles[1]-a.quantiles[0])
    ax.fill_between(x, band[0], band[1], color='#2c9dff', alpha=.25,
                    label=f'{coverage:g}% pointwise interval')
    if a.curve != 'none':
        curve, label = {'best': (best,'Highest-likelihood sampled curve'),
                        'mean': (mean,'Mean model prediction'),
                        'median': (median,'Median model prediction')}[a.curve]
        ax.plot(x, curve, color='#2176b9', lw=2, label=label)
    if obs is not None:
        ax.errorbar(obs[:,0], obs[:,1], yerr=obs[:,2], fmt='o', ms=4, color='#b52b66', label='Observations')
    ax.set(xlabel=a.xlabel, ylabel=a.ylabel)
    if a.xlim:
        ax.set_xlim(a.xlim)
    if a.ylim:
        ax.set_ylim(a.ylim)
    ax.legend()
    ax.ticklabel_format(style='plain', axis='both', useOffset=False)
    if a.export:
        a.export.parent.mkdir(parents=True, exist_ok=True)
        np.savetxt(a.export, np.column_stack([x,band.T,mean,median,best]), delimiter=',',
                   header='x,lower,upper,mean_model,median_model,best_model', comments='')
        print(f'Saved {a.export}')
    if a.save:
        a.save.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(a.save, dpi=200)
        print(f'Saved {a.save}')
    print(f'{len(sim)} equally weighted simulations; {len(x)} prediction points; noise: {a.noise_kind}')
    print(f'Highest-likelihood input row: {best_id+1}')
    if not a.no_show:
        plt.show()
    plt.close(fig)


if __name__ == '__main__':
    main()
