#!/usr/bin/env python3
"""Plot the AoI-energy Pareto frontier (figure F6) from results.csv.

Usage: python3 make_pareto.py results.csv figures/pareto.pdf
"""

import csv
import math
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
    out = sys.argv[2] if len(sys.argv) > 2 else "figures/pareto.pdf"

    pts = defaultdict(lambda: defaultdict(list))  # sched -> budget -> [(aoi, duty)]
    with open(path) as f:
        for row in csv.DictReader(f):
            if not row["regime"].startswith("pareto"):
                continue
            if row["aoi_ms"] in ("None", "", None):
                continue
            b = float(row["regime"][len("pareto"):])
            pts[row["scheduler"]][b].append(
                (float(row["aoi_ms"]), float(row["duty"])))

    fig, ax = plt.subplots(figsize=(4.2, 3.0))
    styles = {"EqualInterval": ("o--", "tab:gray", "Equal-interval"),
              "HarmonicGreedy": ("s-", "tab:blue", "Harmonic-Greedy")}
    for sched, (style, color, label) in styles.items():
        if sched not in pts:
            continue
        budgets = sorted(pts[sched])
        duty = [sum(d for _, d in pts[sched][b]) / len(pts[sched][b])
                for b in budgets]
        aoi = [sum(a for a, _ in pts[sched][b]) / len(pts[sched][b])
               for b in budgets]
        err = [1.96 * math.sqrt(
            sum((a - am) ** 2 for a, _ in pts[sched][b]) /
            max(1, len(pts[sched][b]) - 1) / len(pts[sched][b]))
            for b, am in zip(budgets, aoi)]
        ax.errorbar(duty, aoi, yerr=err, fmt=style, color=color, label=label,
                    markersize=4, linewidth=1.2, capsize=2)
    ax.set_xlabel("mean station duty cycle (awake fraction)")
    ax.set_ylabel("weighted mean AoI [ms]")
    ax.legend(frameon=False)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
