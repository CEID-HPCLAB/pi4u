#!/usr/bin/env python3
"""Plot Pi4U BASE_00000.dat populations with Matplotlib.

Usage: python3 display_gen.py BASE [GENERATION] [DIM] [I] [J]
The first DIM columns are parameters; column DIM+1 supplies point colors.
Parameter indices are one-based. No column labels are required.
Dependencies: numpy, matplotlib.
"""

import argparse
from itertools import combinations
from pathlib import Path
import sys

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", help="filename prefix (possibly including a directory)")
    parser.add_argument("generation", nargs="?", type=int, default=0)
    parser.add_argument("dim", nargs="?", type=int, default=2)
    parser.add_argument("i", nargs="?", type=int, default=1)
    parser.add_argument("j", nargs="?", type=int, default=2)
    parser.add_argument("--all-pairs", action="store_true", help="plot all parameter pairs")
    parser.add_argument("--auto-limits", action="store_true", help="fit axes to data")
    parser.add_argument("--xlim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--ylim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--save", metavar="FILE", help="also save a PNG, PDF, or SVG")
    parser.add_argument("--no-show", action="store_true", help="save without opening a window")
    parser.add_argument("--backend", help="optional GUI backend, e.g. MacOSX or TkAgg")
    args = parser.parse_args()
    if args.generation < 0 or args.dim < 2:
        parser.error("generation must be nonnegative and dim must be at least 2")
    if not (1 <= args.i <= args.dim and 1 <= args.j <= args.dim) or args.i == args.j:
        parser.error("i and j must be distinct indices between 1 and dim")
    if args.no_show and not args.save:
        parser.error("--no-show requires --save FILE")
    for limits in (args.xlim, args.ylim):
        if limits is not None and (not np.isfinite(limits).all() or limits[0] >= limits[1]):
            parser.error("axis limits must be finite and MIN < MAX")

    filename = Path(f"{args.base}_{args.generation:05d}.dat")
    try:
        data = np.loadtxt(filename, comments=("#", "%"), ndmin=2)
    except (OSError, ValueError) as exc:
        parser.error(f"cannot read {filename}: {exc}")
    if data.size == 0 or data.shape[1] < args.dim + 1:
        parser.error(f"{filename} must contain at least {args.dim + 1} numeric columns")

    import matplotlib
    if args.no_show:
        matplotlib.use("Agg")
    elif args.backend:
        matplotlib.use(args.backend)
    from matplotlib import pyplot as plt
    from matplotlib.colors import LinearSegmentedColormap

    # Gnuplot palette rgbformulae 22,13,-31: 3t-1, sin(pi*t), 1-2t.
    t = np.linspace(0, 1, 256)
    colors = np.column_stack((np.clip(3*t-1, 0, 1), np.sin(np.pi*t),
                              np.clip(1-2*t, 0, 1)))
    cmap = LinearSegmentedColormap.from_list("pi4u", colors)
    pairs = list(combinations(range(1, args.dim + 1), 2)) if args.all_pairs else [(args.i, args.j)]
    columns = min(3, len(pairs))
    rows = (len(pairs) + columns - 1) // columns
    fig, axes = plt.subplots(rows, columns, figsize=(6*columns, 5*rows),
                             squeeze=False, layout="constrained")
    for ax, (i, j) in zip(axes.flat, pairs):
        valid = np.isfinite(data[:, [i-1, j-1, args.dim]]).all(axis=1)
        if not valid.any():
            parser.error(f"no finite points for parameters {i} and {j}")
        points = ax.scatter(data[valid, i-1], data[valid, j-1],
                            c=data[valid, args.dim], cmap=cmap, s=20, linewidths=0)
        ax.set(xlabel=f"Parameter {i}", ylabel=f"Parameter {j}",
               title=f"Generation {args.generation}: parameters {i}, {j}")
        ax.set_aspect("equal", adjustable="box")
        if args.xlim or not args.auto_limits:
            ax.set_xlim(args.xlim or (-6, 6))
        if args.ylim or not args.auto_limits:
            ax.set_ylim(args.ylim or (-6, 6))
        ax.grid(True, alpha=0.25)
        fig.colorbar(points, ax=ax, label=f"Value (column {args.dim + 1})")
    for ax in list(axes.flat)[len(pairs):]:
        fig.delaxes(ax)
    if args.save:
        fig.savefig(args.save, dpi=180, bbox_inches="tight")
        print(f"Saved {args.save}")
    if not args.no_show:
        if matplotlib.get_backend().lower() in {"agg", "pdf", "svg", "ps", "cairo", "template"}:
            parser.error("the selected backend cannot open a window; try --backend MacOSX "
                         "on macOS, or --save plot.png --no-show")
        plt.show(block=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
