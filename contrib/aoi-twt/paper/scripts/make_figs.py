#!/usr/bin/env python3
"""Generate figures F2 (validation grid), F5 (regime bars), F7 (scaling)
from valgrid.csv, results.csv, scaling.csv.

Usage: python3 make_figs.py <data_dir> <fig_dir>
"""

import csv
import math
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


def ci95(vals):
    n = len(vals)
    m = sum(vals) / n
    if n < 2:
        return m, 0.0
    var = sum((v - m) ** 2 for v in vals) / (n - 1)
    t = {1: 12.71, 2: 4.30, 3: 3.18, 4: 2.78, 9: 2.26}.get(n - 1, 1.96)
    return m, t * math.sqrt(var / n)


def fig_validation(data_dir, fig_dir):
    pts = defaultdict(lambda: ([], []))  # e -> (measured, predicted)
    with open(f"{data_dir}/valgrid.csv") as f:
        for row in csv.DictReader(f):
            if row["measured_ms"] in ("None", ""):
                continue
            pts[float(row["e"])][0].append(float(row["measured_ms"]))
            pts[float(row["e"])][1].append(float(row["predicted_ms"]))

    fig, ax = plt.subplots(figsize=(4.2, 3.0))
    es = sorted(pts)
    meas = [ci95(pts[e][0]) for e in es]
    pred = [sum(pts[e][1]) / len(pts[e][1]) for e in es]
    ax.errorbar(es, [m for m, _ in meas], yerr=[h for _, h in meas],
                fmt="o", color="tab:blue", label="measured (ns-3)",
                markersize=4, capsize=2)
    ax.plot(es, pred, "k--", linewidth=1.2,
            label=r"model: $\delta + T(1/p - 1/2)$")
    ax.set_xlabel("per-SP loss probability $e$ ($p = 1 - e$)")
    ax.set_ylabel("time-average AoI [ms]")
    ax.legend(frameon=False)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(f"{fig_dir}/validation.pdf")
    print("wrote validation.pdf")


def fig_regimes(data_dir, fig_dir):
    data = defaultdict(lambda: defaultdict(list))
    with open(f"{data_dir}/results.csv") as f:
        for row in csv.DictReader(f):
            if row["regime"].startswith("pareto") or row["aoi_ms"] in ("None", ""):
                continue
            data[row["regime"]][row["scheduler"]].append(float(row["aoi_ms"]))

    regimes = ["loose", "tight", "skew", "fading", "fadingskew"]
    labels = ["loose", "tight", "tight\n+skew", "tight\n+fading",
              "tight+fading\n+skew"]
    x = range(len(regimes))
    width = 0.38
    fig, ax = plt.subplots(figsize=(4.2, 3.0))
    for off, (sched, color, label) in enumerate(
            [("EqualInterval", "tab:gray", "Equal-interval"),
             ("HarmonicGreedy", "tab:blue", "Harmonic-Greedy")]):
        ms = [ci95(data[r][sched]) for r in regimes]
        ax.bar([i + (off - 0.5) * width for i in x], [m for m, _ in ms],
               width, yerr=[h for _, h in ms], color=color, label=label,
               capsize=2)
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, fontsize=7)
    ax.set_ylabel("weighted mean AoI [ms]")
    ax.legend(frameon=False, fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(f"{fig_dir}/regimes.pdf")
    print("wrote regimes.pdf")


def fig_scaling(data_dir, fig_dir):
    data = defaultdict(lambda: defaultdict(list))
    with open(f"{data_dir}/scaling.csv") as f:
        for row in csv.DictReader(f):
            if row["aoi_ms"] in ("None", ""):
                continue
            data[row["scheduler"]][int(row["n"])].append(float(row["aoi_ms"]))

    fig, ax = plt.subplots(figsize=(4.2, 3.0))
    for sched, style, color, label in [
            ("EqualInterval", "o--", "tab:gray", "Equal-interval"),
            ("HarmonicGreedy", "s-", "tab:blue", "Harmonic-Greedy")]:
        ns = sorted(data[sched])
        ms = [ci95(data[sched][n]) for n in ns]
        ax.errorbar(ns, [m for m, _ in ms], yerr=[h for _, h in ms],
                    fmt=style, color=color, label=label, markersize=4,
                    capsize=2, linewidth=1.2)
    ax.set_xlabel("number of stations $N$")
    ax.set_ylabel("weighted mean AoI [ms]")
    ax.legend(frameon=False)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(f"{fig_dir}/scaling.pdf")
    print("wrote scaling.pdf")


if __name__ == "__main__":
    data_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    fig_dir = sys.argv[2] if len(sys.argv) > 2 else "figures"
    fig_validation(data_dir, fig_dir)
    fig_regimes(data_dir, fig_dir)
    fig_scaling(data_dir, fig_dir)
