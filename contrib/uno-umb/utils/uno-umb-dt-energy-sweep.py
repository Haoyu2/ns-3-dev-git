#!/usr/bin/env python3
"""Run batch sweeps for the UNO-UMB digital-twin energy prototype."""

import argparse
import csv
import itertools
import json
import subprocess
import time
from datetime import datetime, timezone
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
    parser.add_argument("--runs", default="1")
    parser.add_argument("--ue-counts", default="16")
    parser.add_argument("--ue-rates", default="1.0")
    parser.add_argument("--spacings", default="500")
    parser.add_argument("--uncertainty-scales", default="1.0")
    parser.add_argument("--traffic-profiles", default="steady,center-burst")
    parser.add_argument("--burst-rate-multipliers", default="3.0")
    parser.add_argument("--shift-start", default="3.0s")
    parser.add_argument("--shift-stop", default="10.0s")
    parser.add_argument("--sim-time", default="12.0s")
    parser.add_argument("--output-dir", default="")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    out_dir = Path(args.output_dir) if args.output_dir else repo / "results" / (
        "uno-umb-dt-energy-" + time.strftime("%Y%m%d-%H%M%S")
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    policies = split_csv(args.policies)
    seeds = split_csv(args.seeds, int)
    runs = split_csv(args.runs, int)
    ue_counts = split_csv(args.ue_counts, int)
    ue_rates = split_csv(args.ue_rates, float)
    spacings = split_csv(args.spacings, float)
    uncertainty_scales = split_csv(args.uncertainty_scales, float)
    traffic_profiles = split_csv(args.traffic_profiles)
    burst_rate_multipliers = split_csv(args.burst_rate_multipliers, float)

    if not args.skip_build:
        run_command(["./ns3", "build", "uno-umb-dt-energy"], repo)

    aggregate_rows = []
    manifest = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "arguments": vars(args),
        "runs": [],
    }
    combinations = list(
        itertools.product(
            policies,
            seeds,
            runs,
            ue_counts,
            ue_rates,
            spacings,
            uncertainty_scales,
            traffic_profiles,
            burst_rate_multipliers,
        )
    )
    seen_scenarios = set()
    for index, (
        policy,
        seed,
        run,
        ues,
        ue_rate,
        spacing,
        uncertainty_scale,
        traffic_profile,
        burst_rate_multiplier,
    ) in enumerate(combinations, start=1):
        effective_burst_rate_multiplier = (
            1.0 if traffic_profile == "steady" else burst_rate_multiplier
        )
        scenario_key = (
            policy,
            seed,
            run,
            ues,
            ue_rate,
            spacing,
            uncertainty_scale,
            traffic_profile,
            effective_burst_rate_multiplier,
        )
        if scenario_key in seen_scenarios:
            continue
        seen_scenarios.add(scenario_key)

        run_id = (
            f"{index:04d}-{policy}-seed{seed}-run{run}-ues{ues}-rate{ue_rate}"
            f"-spacing{spacing}-unc{uncertainty_scale}-{traffic_profile}"
            f"-burst{effective_burst_rate_multiplier}"
        ).replace(".", "p")
        summary_csv = out_dir / f"{run_id}-summary.csv"
        event_csv = out_dir / f"{run_id}-events.csv"
        program = (
            "uno-umb-dt-energy "
            f"--policy={policy} "
            f"--seed={seed} "
            f"--run={run} "
            f"--numberOfUes={ues} "
            f"--ueRateMbps={ue_rate} "
            f"--enbSpacingMeters={spacing} "
            f"--uncertaintyScale={uncertainty_scale} "
            f"--trafficProfile={traffic_profile} "
            f"--burstRateMultiplier={effective_burst_rate_multiplier} "
            f"--shiftStart={args.shift_start} "
            f"--shiftStop={args.shift_stop} "
            f"--simTime={args.sim_time} "
            f"--summaryCsv={summary_csv} "
            f"--eventCsv={event_csv}"
        )
        manifest["runs"].append(
            {
                "run_id": run_id,
                "policy": policy,
                "seed": seed,
                "run": run,
                "number_of_ues": ues,
                "ue_rate_mbps": ue_rate,
                "enb_spacing_m": spacing,
                "uncertainty_scale": uncertainty_scale,
                "traffic_profile": traffic_profile,
                "burst_rate_multiplier": effective_burst_rate_multiplier,
                "command": ["./ns3", "run", program],
                "summary_csv": str(summary_csv),
                "event_csv": str(event_csv),
            }
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
                "traffic_profile": traffic_profile,
                "burst_rate_multiplier": effective_burst_rate_multiplier,
                "shift_start": args.shift_start,
                "shift_stop": args.shift_stop,
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

    manifest["completed_utc"] = datetime.now(timezone.utc).isoformat()
    manifest["aggregate_csv"] = str(out_dir / "aggregate.csv")
    with (out_dir / "manifest.json").open("w") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")

    print(f"Wrote {out_dir / 'aggregate.csv'}")
    print(f"Wrote {out_dir / 'manifest.json'}")


if __name__ == "__main__":
    main()
