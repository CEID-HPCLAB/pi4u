#!/usr/bin/env python3
"""Parameter marginals, pair densities, and optionally score-colored scatter."""
import argparse
from pathlib import Path
import numpy as np
from scipy.ndimage import gaussian_filter


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('file', type=Path)
    p.add_argument('--dim', type=int, required=True, help='Number of leading parameter columns')
    p.add_argument('--score-column', type=int, help='1-based score column (default: dim+1 if present)')
    p.add_argument('--no-color', action='store_true', help='Do not color scatter points by score')
    p.add_argument('--minimize', action='store_true', help='Smaller scores are better')
    p.add_argument('--labels', nargs='+', help='One label per parameter')
    p.add_argument('--score-label', default='Log-likelihood / score')
    p.add_argument('--bins', type=int, default=40)
    p.add_argument('--smooth', type=float, default=1.0, help='Pair-histogram smoothing width in bins')
    p.add_argument('--max-scatter', type=int, default=10000, help='Maximum displayed points per scatter panel; 0 means all')
    p.add_argument('--save', type=Path, help='Output PNG/PDF/SVG filename')
    p.add_argument('--no-show', action='store_true', help='Save without a GUI; requires --save')
    a = p.parse_args()
    if a.no_show and not a.save:
        p.error('--no-show requires --save')
    if a.dim < 1 or a.bins < 2 or a.smooth < 0 or a.max_scatter < 0:
        p.error('dim >= 1, bins >= 2, smooth >= 0, max-scatter >= 0 required')
    if a.labels and len(a.labels) != a.dim:
        p.error('--labels must contain exactly dim labels')
    try:
        data = np.loadtxt(a.file, ndmin=2)
        if data.shape[0] < 1 or data.shape[1] < a.dim:
            raise ValueError('Input has insufficient rows or parameter columns')
        x = data[:, :a.dim]
        if not np.isfinite(x).all():
            raise ValueError('Parameter columns contain nonfinite values')
        col = a.score_column
        if col is None and data.shape[1] > a.dim:
            col = a.dim + 1
        if col is not None and not 1 <= col <= data.shape[1]:
            raise ValueError('score-column is outside the input columns')
        score = None if col is None else data[:, col - 1]
        if score is not None and not np.isfinite(score).all():
            raise ValueError('Score column contains nonfinite values')
    except (ValueError, OSError) as exc:
        p.error(str(exc))
    labels = a.labels or [f'theta_{i}' for i in range(a.dim)]
    print(f'Samples: {len(x)}; dimension: {a.dim}')
    print('Parameter\tMean\tStd (ddof=0)\tq05\tMedian\tq95')
    qs = np.quantile(x, [.05, .5, .95], axis=0)
    for i, name in enumerate(labels):
        print(f'{name}\t{x[:,i].mean():.8g}\t{x[:,i].std():.8g}\t{qs[0,i]:.8g}\t{qs[1,i]:.8g}\t{qs[2,i]:.8g}')
    if score is not None:
        best = int(np.argmin(score) if a.minimize else np.argmax(score))
        print(f'Best score: {score[best]:.8g}; input row {best+1}; parameters: {x[best]}')
    import matplotlib
    if a.no_show:
        matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.colors import Normalize
    fig, axes = plt.subplots(a.dim, a.dim, squeeze=False,
                             figsize=(max(4, 2.6*a.dim), max(3.5, 2.6*a.dim)),
                             layout='constrained')
    indices = np.arange(len(x))
    if a.max_scatter and len(x) > a.max_scatter:
        indices = np.sort(np.random.default_rng(0).choice(len(x), a.max_scatter, replace=False))
    norm = None if score is None else Normalize(float(score.min()), float(score.max()))
    cmap = 'viridis_r' if a.minimize else 'viridis'
    colored = None
    for i in range(a.dim):
        for j in range(a.dim):
            ax = axes[i,j]
            if i == j:
                ax.hist(x[:,j], bins=a.bins, density=True, color='#51ad78', alpha=.85)
            elif i < j:
                kwargs = dict(s=5, alpha=.5, rasterized=True)
                if score is not None and not a.no_color:
                    colored = ax.scatter(x[indices,j], x[indices,i], c=score[indices], cmap=cmap, norm=norm, **kwargs)
                else:
                    ax.scatter(x[indices,j], x[indices,i], color='#326a9e', **kwargs)
            else:
                hist, xe, ye = np.histogram2d(x[:,j], x[:,i], bins=a.bins)
                if a.smooth:
                    hist = gaussian_filter(hist, a.smooth, mode='reflect')
                density = hist / (hist.sum() * np.diff(xe)[:,None] * np.diff(ye)[None,:])
                ax.pcolormesh(xe, ye, density.T, shading='auto', cmap='Blues', rasterized=True)
            if i == a.dim-1:
                ax.set_xlabel(labels[j])
            else:
                ax.tick_params(labelbottom=False)
            if j == 0:
                ax.set_ylabel('Density' if i == j else labels[i])
            else:
                ax.tick_params(labelleft=False)
            ax.ticklabel_format(style='plain', axis='both', useOffset=False)
    if colored is not None:
        fig.colorbar(colored, ax=axes.ravel().tolist(), label=a.score_label, shrink=.65)
    if a.save:
        a.save.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(a.save, dpi=200)
        print(f'Saved {a.save}')
    if not a.no_show:
        plt.show()
    plt.close(fig)


if __name__ == '__main__':
    main()
