#!/usr/bin/env python3
"""Turn results.csv (from run_sweep.py) into results_macros.tex and a
mean +- 95% CI summary table printed to stdout.

Usage: python3 make_macros.py results.csv [output.tex]
"""

import csv
import math
import sys
from collections import defaultdict

# two-sided 95% t quantiles for n-1 df (n = 2..10)
T95 = {1: 12.71, 2: 4.30, 3: 3.18, 4: 2.78, 5: 2.57,
       6: 2.45, 7: 2.36, 8: 2.31, 9: 2.26}


def ci(values):
    n = len(values)
    mean = sum(values) / n
    if n < 2:
        return mean, 0.0
    var = sum((v - mean) ** 2 for v in values) / (n - 1)
    half = T95.get(n - 1, 1.96) * math.sqrt(var / n)
    return mean, half


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
    out = sys.argv[2] if len(sys.argv) > 2 else "results_macros.tex"

    data = defaultdict(lambda: defaultdict(list))  # regime -> sched -> [(aoi,peak,duty)]
    with open(path) as f:
        for row in csv.DictReader(f):
            if row["aoi_ms"] in ("None", "", None):
                continue
            data[row["regime"]][row["scheduler"]].append(
                (float(row["aoi_ms"]), float(row["peak_ms"]), float(row["duty"])))

    print(f"{'regime':<14}{'scheduler':<16}{'aoi mean+-ci':<22}"
          f"{'peak mean+-ci':<22}{'duty':<10}{'n'}")
    stats = {}
    for reg in sorted(data):
        for sched in sorted(data[reg]):
            rows = data[reg][sched]
            am, ah = ci([r[0] for r in rows])
            pm, ph = ci([r[1] for r in rows])
            dm, _ = ci([r[2] for r in rows])
            stats[(reg, sched)] = (am, ah, pm, ph, dm)
            print(f"{reg:<14}{sched:<16}{am:7.2f} +- {ah:5.2f}      "
                  f"{pm:7.2f} +- {ph:5.2f}      {dm:7.4f}  {len(rows)}")

    def get(reg, sched):
        return stats.get((reg, sched), (float('nan'),) * 5)

    with open(out, "w") as f:
        def macro(name, val, prec=1):
            f.write(f"\\newcommand{{\\{name}}}{{{val:.{prec}f}}}\n")

        for reg, prefix in [("skew", "Skew"), ("fadingskew", "FadingSkew"),
                            ("loose", "Loose"), ("tight", "Tight"),
                            ("fading", "Fading")]:
            e = get(reg, "EqualInterval")
            h = get(reg, "HarmonicGreedy")
            macro(f"Res{prefix}Equal", e[0])
            macro(f"Res{prefix}EqualCI", e[1], 2)
            macro(f"Res{prefix}Harmonic", h[0])
            macro(f"Res{prefix}HarmonicCI", h[1], 2)
            if e[0] > 0:
                macro(f"Res{prefix}Gain", 100 * (e[0] - h[0]) / e[0])
            macro(f"Res{prefix}PeakEqual", e[2])
            macro(f"Res{prefix}PeakHarmonic", h[2])
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
