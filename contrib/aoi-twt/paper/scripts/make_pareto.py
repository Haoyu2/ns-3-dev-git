#!/usr/bin/env python3
"""Plot the AoI-energy Pareto frontier (figure F6) from results.csv.

Usage: python3 make_pareto.py results.csv figures/pareto.pdf
"""

import csv
import os
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
# embed TrueType (Type 42), not Type 3 -- IEEE PDF checks reject Type 3 fonts
matplotlib.rcParams["pdf.fonttype"] = 42
matplotlib.rcParams["ps.fonttype"] = 42
import matplotlib.pyplot as plt  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_macros import ci  # noqa: E402  (single source of CI math)


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
        stats = [ci([a for a, _ in pts[sched][b]]) for b in budgets]
        aoi = [m for m, _ in stats]
        err = [h for _, h in stats]
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
