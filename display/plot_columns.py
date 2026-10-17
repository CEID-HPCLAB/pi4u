#!/usr/bin/env python3
"""Plot any two numeric columns, using 1-based column numbers.

Examples:
  python3 plot_columns.py results.txt --columns 1 2
  python3 plot_columns.py pareto_mocma.txt --columns 3 4 --labels Cost Error
  python3 plot_columns.py curgen_db 10 --columns 1 3
  python3 plot_columns.py results.txt --connect --save plot.png --no-show

With GENERATION, reads BASENAME_GENERATION.txt (three-digit padding).
Otherwise reads the exact filename. Defaults to columns 1 and 2.
All finite selected pairs are plotted; no Pareto filtering or color column.
Dependencies: python3 -m pip install numpy matplotlib
"""

import argparse
import math
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", help="Exact filename, or prefix when generation is supplied")
    parser.add_argument("generation", nargs="?", type=int)
    parser.add_argument("--columns", nargs=2, type=int, default=(1, 2), metavar=("X", "Y"))
    parser.add_argument("--labels", nargs=2, metavar=("X_LABEL", "Y_LABEL"))
    parser.add_argument("--title")
    parser.add_argument("--connect", action="store_true", help="Connect points in file order")
    parser.add_argument("--sort-x", action="store_true", help="Sort points by x before plotting")
    parser.add_argument("--delimiter", help="Default: whitespace; use ',' for CSV without labels")
    parser.add_argument("--skiprows", type=int, default=0, help="Skip initial header lines")
    parser.add_argument("--xlim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--ylim", nargs=2, type=float, metavar=("MIN", "MAX"))
    parser.add_argument("--equal", action="store_true", help="Use equal x/y unit scales")
    parser.add_argument("--point-size", type=float, default=18)
    parser.add_argument("--save", type=Path, help="Save PNG, PDF, or another supported format")
    parser.add_argument("--no-show", action="store_true", help="Save without opening a window")
    parser.add_argument("--dpi", type=int, default=180)
    args = parser.parse_args()
    if min(args.columns) < 1:
        parser.error("column numbers start at 1")
    if args.generation is not None and args.generation < 0:
        parser.error("generation must be nonnegative")
    if args.skiprows < 0:
        parser.error("--skiprows must be nonnegative")
    if args.delimiter is not None and len(args.delimiter) != 1:
        parser.error("--delimiter must be one character")
    if not math.isfinite(args.point_size) or args.point_size <= 0 or args.dpi <= 0:
        parser.error("point size and dpi must be positive and finite")
    if args.no_show and args.save is None:
        parser.error("--no-show requires --save")
    for name in ("xlim", "ylim"):
        limits = getattr(args, name)
        if limits and (not all(map(math.isfinite, limits)) or limits[0] >= limits[1]):
            parser.error(f"--{name} requires finite MIN < MAX")

    try:
        import numpy as np
        import matplotlib
        if args.no_show:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"Missing dependency: {exc}. Install numpy and matplotlib.", file=sys.stderr)
        return 1

    filename = Path(args.input if args.generation is None else
                    f"{args.input}_{args.generation:03d}.txt")
    try:
        # Read only selected columns, so unrelated outputs need not be finite.
        data = np.loadtxt(filename, usecols=tuple(c - 1 for c in args.columns),
                          ndmin=2, delimiter=args.delimiter, skiprows=args.skiprows)
        if data.size == 0:
            raise ValueError("input contains no samples")
        valid = np.isfinite(data).all(axis=1)
        dropped = int((~valid).sum())
        data = data[valid]
        if not len(data):
            raise ValueError("no rows have finite selected values")
        if dropped:
            print(f"Skipped {dropped} rows with nonfinite selected values.", file=sys.stderr)
        if args.sort_x:
            data = data[np.argsort(data[:, 0], kind="stable")]
        fig, ax = plt.subplots(figsize=(7, 5.5), layout="constrained")
        if args.connect:
            ax.plot(data[:, 0], data[:, 1], color="tab:blue", linewidth=1, alpha=0.65)
        ax.scatter(data[:, 0], data[:, 1], s=args.point_size, color="tab:blue",
                   linewidths=0, rasterized=True)
        labels = args.labels or [f"Column {c}" for c in args.columns]
        ax.set_xlabel(labels[0])
        ax.set_ylabel(labels[1])
        ax.set_title(args.title if args.title is not None else filename.name)
        ax.ticklabel_format(style="plain", useOffset=False, axis="both")
        if args.xlim:
            ax.set_xlim(args.xlim)
        if args.ylim:
            ax.set_ylim(args.ylim)
        if args.equal:
            ax.set_aspect("equal", adjustable="box")
        ax.grid(alpha=0.25)
        if args.save:
            fig.savefig(args.save, dpi=args.dpi)
            print(f"Saved {args.save}")
        if not args.no_show:
            plt.show()
        plt.close(fig)
        return 0
    except (OSError, ValueError, IndexError) as exc:
        print(f"Error reading or plotting {filename}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
