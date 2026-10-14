#!/usr/bin/env python3
"""Plot Pi4U multiobjective generation data using NumPy and Matplotlib.

Usage: python3 display_gen_multi.py curgen 10 2 [i j] [--color-column COLUMN]
Data: curgen_010.txt, with parameters in columns 1..dim followed by outputs.
COLUMN is the absolute, 1-based column number in the file and must exceed dim.
It defaults to dim+1, the first output column.
Select any output (objective or other scalar) to color the parameter plots.
Other output columns are ignored, including when filtering nonfinite rows.
The plot opens by default; --save can also save it.

Examples:
  python3 display_gen_multi.py curgen 10 2 --color-column 3
  python3 display_gen_multi.py curgen 10 2 --color-column 4 --color-label "Objective 2"
  python3 display_gen_multi.py samples 5 4 1 3 --color-column 6 --save samples_005.png
  python3 display_gen_multi.py curgen 10 4 --color-column 5 --all-pairs --save pairs.pdf --no-show

Dependencies: python3 -m pip install numpy matplotlib
"""

import argparse
from itertools import combinations
import math
from pathlib import Path
import sys


def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("basename", help="File prefix, optionally including a directory")
    parser.add_argument("generation", nargs="?", type=int, default=0)
    parser.add_argument("dim", nargs="?", type=int, default=2)
    parser.add_argument("i", nargs="?", type=int, default=None)
    parser.add_argument("j", nargs="?", type=int, default=None)
    parser.add_argument(
        "--color-column", type=int, metavar="COLUMN",
        help="Absolute 1-based output column used for color (default: dim+1)"
    )
    parser.add_argument("--all-pairs", action="store_true", help="Plot every parameter pair")
    parser.add_argument("--save", type=Path, help="Save figure, e.g. plot.png or plot.pdf")
    parser.add_argument("--no-show", action="store_true", help="Save without opening a window")
    parser.add_argument("--auto-limits", action="store_true", help="Fit axes to the samples")
    parser.add_argument("--xlim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--ylim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--cmap", default="turbo", help="Matplotlib colormap (default: turbo)")
    parser.add_argument("--color-label", help="Colorbar label (default: output index and file column)")
    parser.add_argument("--point-size", type=float, default=12.0)
    parser.add_argument("--dpi", type=int, default=180)
    args = parser.parse_args()
    if args.generation < 0:
        parser.error("generation must be nonnegative")
    if args.dim < 2:
        parser.error("dim must be at least 2")
    if args.color_column is None:
        args.color_column = args.dim + 1
    if args.color_column <= args.dim:
        parser.error("--color-column must exceed dim; column numbers start at 1")
    if args.color_label is None:
        args.color_label = f"Output {args.color_column - args.dim} (column {args.color_column})"
    if (args.i is None) != (args.j is None):
        parser.error("provide both parameter indices i and j")
    if args.all_pairs and args.i is not None:
        parser.error("use --all-pairs without explicit parameter indices")
    if args.i is None:
        args.i, args.j = 1, 2
    if not (1 <= args.i <= args.dim and 1 <= args.j <= args.dim):
        parser.error("parameter indices must be between 1 and dim")
    if args.i == args.j:
        parser.error("parameter indices must be different")
    if args.no_show and args.save is None:
        parser.error("--no-show requires --save")
    if args.point_size <= 0 or args.dpi <= 0:
        parser.error("point size and dpi must be positive")
    for name in ("xlim", "ylim"):
        limits = getattr(args, name)
        if limits and (not all(math.isfinite(v) for v in limits) or limits[0] >= limits[1]):
            parser.error(f"--{name} requires finite MIN < MAX")
    return args


def main():
    args = parse_args()
    try:
        import numpy as np
        import matplotlib
        if args.no_show:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"Missing dependency: {exc}. Install numpy and matplotlib.", file=sys.stderr)
        return 1

    filename = Path(f"{args.basename}_{args.generation:03d}.txt")
    try:
        data = np.loadtxt(filename, ndmin=2)
        if data.size == 0:
            raise ValueError("input file contains no samples")
        if data.shape[1] < args.color_column:
            raise ValueError(
                f"selected color column {args.color_column}, but found only {data.shape[1]} columns"
            )
        if args.cmap not in plt.colormaps():
            raise ValueError(f"unknown colormap: {args.cmap}")

        pairs = list(combinations(range(1, args.dim + 1), 2)) if args.all_pairs else [(args.i, args.j)]
        used_columns = sorted({v - 1 for pair in pairs for v in pair} | {args.color_column - 1})
        valid = np.isfinite(data[:, used_columns]).all(axis=1)
        dropped = int((~valid).sum())
        data = data[valid]
        if not len(data):
            raise ValueError("no samples have finite coordinates and color values")
        if dropped:
            print(f"Skipped {dropped} rows with nonfinite plotted values.", file=sys.stderr)

        ncols = min(3, len(pairs))
        nrows = math.ceil(len(pairs) / ncols)
        fig, axes = plt.subplots(
            nrows, ncols, squeeze=False,
            figsize=(5 * ncols, 4.5 * nrows), layout="constrained"
        )
        color = data[:, args.color_column - 1]
        norm = matplotlib.colors.Normalize(vmin=float(color.min()), vmax=float(color.max()))
        for ax, (i, j) in zip(axes.flat, pairs):
            points = ax.scatter(
                data[:, i - 1], data[:, j - 1], c=color, norm=norm,
                cmap=args.cmap, s=args.point_size, linewidths=0, rasterized=True
            )
            ax.set_xlabel(f"Parameter {i}")
            ax.set_ylabel(f"Parameter {j}")
            ax.set_title(f"Generation {args.generation}: parameters {i}, {j}")
            ax.set_aspect("equal", adjustable="box")
            if args.xlim:
                ax.set_xlim(args.xlim)
            elif not args.auto_limits:
                ax.set_xlim(-15, 15)
            if args.ylim:
                ax.set_ylim(args.ylim)
            elif not args.auto_limits:
                ax.set_ylim(-15, 15)
            ax.grid(alpha=0.2)
        visible_axes = list(axes.flat)[:len(pairs)]
        for ax in list(axes.flat)[len(pairs):]:
            ax.set_visible(False)
        fig.colorbar(points, ax=visible_axes, label=args.color_label, shrink=0.85)
        fig.suptitle(f"{filename.name} — {len(data)} samples")

        if args.save:
            fig.savefig(args.save, dpi=args.dpi)
            print(f"Saved {args.save}")
        if not args.no_show:
            plt.show()
        plt.close(fig)
        return 0
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
