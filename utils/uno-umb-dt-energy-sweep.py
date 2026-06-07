#!/usr/bin/env python3
"""Run batch sweeps for the UNO-UMB digital-twin energy prototype."""

import argparse
import csv
import itertools
import subprocess
import time
from pathlib import Path


def split_csv(value, cast=str):
    return [cast(item.strip()) for item in value.split(",") if item.strip()]


def run_command(command, cwd):
    print(" ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policies", default="all-on,threshold,aggressive,twin")
    parser.add_argument("--seeds", default="1")
    parser.add_argument("--ue-counts", default="16")
    parser.add_argument("--ue-rates", default="1.0")
    parser.add_argument("--spacings", default="500")
    parser.add_argument("--uncertainty-scales", default="1.0")
    parser.add_argument("--sim-time", default="12.0s")
    parser.add_argument("--output-dir", default="")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    out_dir = Path(args.output_dir) if args.output_dir else repo / "results" / (
        "uno-umb-dt-energy-" + time.strftime("%Y%m%d-%H%M%S")
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    policies = split_csv(args.policies)
    seeds = split_csv(args.seeds, int)
    ue_counts = split_csv(args.ue_counts, int)
    ue_rates = split_csv(args.ue_rates, float)
    spacings = split_csv(args.spacings, float)
    uncertainty_scales = split_csv(args.uncertainty_scales, float)

    if not args.skip_build:
        run_command(["./ns3", "build", "uno-umb-dt-energy"], repo)

    aggregate_rows = []
    combinations = list(
        itertools.product(policies, seeds, ue_counts, ue_rates, spacings, uncertainty_scales)
    )
    for index, (policy, seed, ues, ue_rate, spacing, uncertainty_scale) in enumerate(
        combinations, start=1
    ):
        run_id = (
            f"{index:04d}-{policy}-seed{seed}-ues{ues}-rate{ue_rate}"
            f"-spacing{spacing}-unc{uncertainty_scale}"
        ).replace(".", "p")
        summary_csv = out_dir / f"{run_id}-summary.csv"
        event_csv = out_dir / f"{run_id}-events.csv"
        program = (
            "uno-umb-dt-energy "
            f"--policy={policy} "
            f"--seed={seed} "
            f"--numberOfUes={ues} "
            f"--ueRateMbps={ue_rate} "
            f"--enbSpacingMeters={spacing} "
            f"--uncertaintyScale={uncertainty_scale} "
            f"--simTime={args.sim_time} "
            f"--summaryCsv={summary_csv} "
            f"--eventCsv={event_csv}"
        )
        run_command(["./ns3", "run", program], repo)

        with summary_csv.open(newline="") as handle:
            row = next(csv.DictReader(handle))
        row.update(
            {
                "run_id": run_id,
                "ue_rate_mbps": ue_rate,
                "enb_spacing_m": spacing,
                "uncertainty_scale": uncertainty_scale,
                "summary_csv": str(summary_csv),
                "event_csv": str(event_csv),
            }
        )
        aggregate_rows.append(row)

        aggregate_csv = out_dir / "aggregate.csv"
        with aggregate_csv.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(aggregate_rows[0].keys()))
            writer.writeheader()
            writer.writerows(aggregate_rows)

    print(f"Wrote {out_dir / 'aggregate.csv'}")


if __name__ == "__main__":
    main()
