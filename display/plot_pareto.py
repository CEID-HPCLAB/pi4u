#!/usr/bin/env python3
"""Plot two Pi4U outputs and their nondominated front.

Examples (columns are absolute, starting at 1):
  python3 plot_pareto.py curgen 10 2
  python3 plot_pareto.py curgen 10 4 --columns 5 7
  python3 plot_pareto.py --file results.txt --columns 3 4 --directions min max
  python3 plot_pareto.py curgen 10 2 --save pareto.png --no-show

Prefix mode reads BASENAME_GENERATION.txt with a three-digit generation.
Default columns are dim+1 and dim+2. Both objectives are minimized by default.
The front is computed only among the supplied rows and selected two objectives.
No feasibility filtering is applied; provide feasible rows for constrained runs.
Dependencies: python3 -m pip install numpy matplotlib
"""

import argparse
from pathlib import Path
import sys


def nondominated_mask(values, directions):
    """Return the exact two-objective nondominated mask, keeping duplicates."""
    import numpy as np
    signs = np.array([1 if d == "min" else -1 for d in directions])
    transformed = values * signs
    order = np.lexsort((transformed[:, 1], transformed[:, 0]))
    result = np.zeros(len(values), dtype=bool)
    best_y = np.inf
    start = 0
    while start < len(order):
        end = start + 1
        x = transformed[order[start], 0]
        while end < len(order) and transformed[order[end], 0] == x:
            end += 1
        group = order[start:end]
        y = transformed[group[0], 1]
        if y < best_y:
            result[group[transformed[group, 1] == y]] = True
        best_y = min(best_y, y)
        start = end
    return result


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("basename", nargs="?", help="Input file prefix")
    parser.add_argument("generation", nargs="?", type=int, default=0)
    parser.add_argument("dim", nargs="?", type=int, default=2,
                        help="Number of parameter columns (default: 2)")
    parser.add_argument("--file", type=Path, help="Read this exact filename instead")
    parser.add_argument("--columns", nargs=2, type=int, metavar=("X", "Y"),
                        help="Absolute 1-based output columns (default: dim+1 dim+2)")
    parser.add_argument("--directions", nargs=2, choices=("min", "max"),
                        default=("min", "min"), help="Objective directions (default: min min)")
    parser.add_argument("--labels", nargs=2, metavar=("X_LABEL", "Y_LABEL"))
    parser.add_argument("--front-only", action="store_true", help="Hide dominated points")
    parser.add_argument("--connect", action="store_true", help="Connect front points in x order")
    parser.add_argument("--save", type=Path, help="Save a PNG, PDF, or other figure")
    parser.add_argument("--no-show", action="store_true", help="Save without opening a window")
    parser.add_argument("--dpi", type=int, default=180)
    args = parser.parse_args()
    if args.file is None and args.basename is None:
        parser.error("provide a basename or --file")
    if args.file is not None and args.basename is not None:
        parser.error("use either basename or --file")
    if args.generation < 0 or args.dim < 0:
        parser.error("generation and dim must be nonnegative")
    if args.columns is None:
        args.columns = (args.dim + 1, args.dim + 2)
    if min(args.columns) < 1 or args.columns[0] == args.columns[1]:
        parser.error("--columns requires two different positive column numbers")
    if args.no_show and args.save is None:
        parser.error("--no-show requires --save")
    if args.dpi <= 0:
        parser.error("--dpi must be positive")
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
    filename = args.file if args.file is not None else Path(
        f"{args.basename}_{args.generation:03d}.txt")
    try:
        data = np.loadtxt(filename, ndmin=2)
        if data.size == 0:
            raise ValueError("input file contains no samples")
        if data.shape[1] < max(args.columns):
            raise ValueError(f"selected column {max(args.columns)}, but file has {data.shape[1]} columns")
        values = data[:, np.array(args.columns) - 1]
        valid = np.isfinite(values).all(axis=1)
        dropped = int((~valid).sum())
        values = values[valid]
        if not len(values):
            raise ValueError("no rows have finite selected outputs")
        if dropped:
            print(f"Skipped {dropped} rows with nonfinite selected outputs.", file=sys.stderr)
        mask = nondominated_mask(values, args.directions)
        front = values[mask]
        fig, ax = plt.subplots(figsize=(7, 5.5), layout="constrained")
        if not args.front_only and (~mask).any():
            ax.scatter(values[~mask, 0], values[~mask, 1], s=18, c="0.7",
                       alpha=0.6, label="Dominated samples", rasterized=True)
        if args.connect:
            unique = np.unique(front, axis=0)
            ax.plot(unique[:, 0], unique[:, 1], color="tab:red", linewidth=1, alpha=0.7)
        ax.scatter(front[:, 0], front[:, 1], s=28, c="tab:red",
                   label="Nondominated samples", rasterized=True)
        labels = args.labels or [f"Output column {c}" for c in args.columns]
        ax.set_xlabel(f"{labels[0]} ({args.directions[0]})")
        ax.set_ylabel(f"{labels[1]} ({args.directions[1]})")
        ax.set_title(f"{filename.name}\n{len(front)} nondominated / {len(values)} samples")
        ax.grid(alpha=0.2)
        ax.legend()
        print(f"Nondominated samples: {len(front)} of {len(values)}")
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
