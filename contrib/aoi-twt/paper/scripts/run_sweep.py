#!/usr/bin/env python3
"""Paper-grade experiment sweep for the AoI-TWT paper.

Run from the ns-3 root (after building scratch/twt-aoi-multista):
    python3 contrib/aoi-twt/paper/scripts/run_sweep.py
Produces results.csv with one row per (regime, scheduler, seed).
"""

import concurrent.futures
import os
import re
import subprocess
import sys

BIN = "./build/scratch/ns3.48-twt-aoi-multista-default"
ENV = dict(os.environ, LD_LIBRARY_PATH=os.path.abspath("build/lib"))
SIM_TIME = "300"


def run_one(tag, args, rng):
    cmd = [BIN, f"--simTime={SIM_TIME}", f"--RngRun={rng}"] + args
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, env=ENV,
                             timeout=1800).stdout
    except subprocess.TimeoutExpired:
        return (tag, rng, None, None, None)
    m = re.search(r"SUMMARY aoi_ms=([\d.]+) peak_ms=([\d.]+) duty=([\d.]+)", out)
    if not m:
        return (tag, rng, None, None, None)
    aoi, peak, duty = map(float, m.groups())
    return (tag, rng, aoi, peak, duty)


def sched_args(s):
    return [f"--scheduler=ns3::{s}TwtScheduler"]


def main():
    regimes = {
        "loose": [],
        "tight": ["--budgetScale=2"],
        "skew": ["--weightSkew=8", "--budgetScale=2"],
        "fading": ["--fading=1", "--budgetScale=2"],
        "fadingskew": ["--fading=1", "--weightSkew=8", "--budgetScale=2"],
    }
    jobs = []
    # main regimes, 10 seeds
    for reg, extra in regimes.items():
        for s in ["EqualInterval", "HarmonicGreedy"]:
            for r in range(1, 11):
                jobs.append((f"{reg},{s}", sched_args(s) + extra, r))
    # analyzed (dyadic-reservation) variant on the two skew regimes
    for reg in ["skew", "fadingskew"]:
        for r in range(1, 11):
            jobs.append((f"{reg},HarmonicDyadic",
                         sched_args("HarmonicGreedy") + ["--dyadic=1"] + regimes[reg], r))
    # Pareto frontier: budget scale sweep under weight skew, 5 seeds
    for b in ["0.5", "0.75", "1", "1.5", "2", "3", "4", "6"]:
        for s in ["EqualInterval", "HarmonicGreedy"]:
            for r in range(1, 6):
                jobs.append((f"pareto{b},{s}",
                             sched_args(s) + ["--weightSkew=8", f"--budgetScale={b}"], r))

    workers = int(sys.argv[1]) if len(sys.argv) > 1 else 28
    done = 0
    with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as ex, \
            open("results.csv", "w") as f:
        f.write("regime,scheduler,run,aoi_ms,peak_ms,duty\n")
        futs = [ex.submit(run_one, *j) for j in jobs]
        for fut in concurrent.futures.as_completed(futs):
            tag, rng, aoi, peak, duty = fut.result()
            f.write(f"{tag},{rng},{aoi},{peak},{duty}\n")
            f.flush()
            done += 1
            print(f"[{done}/{len(jobs)}] {tag} run={rng} aoi={aoi}", flush=True)
    print("sweep complete:", len(jobs), "runs")


if __name__ == "__main__":
    main()
