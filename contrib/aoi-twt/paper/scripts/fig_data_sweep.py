#!/usr/bin/env python3
"""Collect data for figures F2 (validation grid) and F7 (scaling).

Run from the ns-3 root after building. Produces valgrid.csv and scaling.csv.
"""

import concurrent.futures
import os
import re
import subprocess

ENV = dict(os.environ, LD_LIBRARY_PATH=os.path.abspath("build/lib"))
VAL_BIN = "./build/scratch/ns3.48-twt-aoi-validation-default"
MULTI_BIN = "./build/scratch/ns3.48-twt-aoi-multista-default"


def run_validation(e, rng):
    cmd = [VAL_BIN, "--simTime=300", f"--errorRate={e}", f"--RngRun={rng}"]
    out = subprocess.run(cmd, capture_output=True, text=True, env=ENV,
                         timeout=1800).stdout
    meas = re.search(r"Measured mean AoI:\s+(\d+) ms", out)
    if e == 0:
        pred = re.search(r"Predicted mean AoI:\s+(\d+) ms", out)
    else:
        pred = re.search(r"Predicted \(p=1-e\):\s+(\d+) ms", out)
    if not (meas and pred):
        return (e, rng, None, None)
    return (e, rng, int(meas.group(1)), int(pred.group(1)))


def run_scaling(n, sched, rng):
    cmd = [MULTI_BIN, "--simTime=200", f"--nStations={n}",
           f"--scheduler=ns3::{sched}TwtScheduler",
           "--weightSkew=8", "--budgetScale=2", f"--RngRun={rng}"]
    out = subprocess.run(cmd, capture_output=True, text=True, env=ENV,
                         timeout=3600).stdout
    m = re.search(r"SUMMARY aoi_ms=([\d.]+) peak_ms=([\d.]+) duty=([\d.]+)", out)
    if not m:
        return (n, sched, rng, None, None, None)
    return (n, sched, rng, *map(float, m.groups()))


def main():
    val_jobs = [(e, r) for e in [0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
                for r in range(1, 4)]
    scale_jobs = [(n, s, r) for n in [10, 20, 30, 40, 50]
                  for s in ["EqualInterval", "HarmonicGreedy"]
                  for r in range(1, 4)]

    with concurrent.futures.ProcessPoolExecutor(max_workers=28) as ex:
        vfuts = [ex.submit(run_validation, *j) for j in val_jobs]
        sfuts = [ex.submit(run_scaling, *j) for j in scale_jobs]
        with open("valgrid.csv", "w") as f:
            f.write("e,run,measured_ms,predicted_ms\n")
            for fut in concurrent.futures.as_completed(vfuts):
                e, r, m, p = fut.result()
                f.write(f"{e},{r},{m},{p}\n")
                f.flush()
        with open("scaling.csv", "w") as f:
            f.write("n,scheduler,run,aoi_ms,peak_ms,duty\n")
            for fut in concurrent.futures.as_completed(sfuts):
                n, s, r, a, p, d = fut.result()
                f.write(f"{n},{s},{r},{a},{p},{d}\n")
                f.flush()
    print("fig data sweep complete")


if __name__ == "__main__":
    main()
