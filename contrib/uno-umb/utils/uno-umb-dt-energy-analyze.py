#!/usr/bin/env python3
"""Summarize UNO-UMB digital-twin energy sweep results."""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


METRICS = [
    "throughput_mbps",
    "loss_ratio",
    "mean_delay_ms",
    "energy_saving_pct",
    "unsafe_sleep_actions",
    "handover_requests",
    "sla_violation",
]


def as_float(row, field):
    return float(row[field])


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stdev(values):
    if len(values) < 2:
        return 0.0
    avg = mean(values)
    return math.sqrt(sum((value - avg) ** 2 for value in values) / (len(values) - 1))


def format_number(value):
    if abs(value) >= 100:
        return f"{value:.2f}"
    if abs(value) >= 1:
        return f"{value:.3f}"
    return f"{value:.4f}"


def write_policy_summary(rows, output_dir):
    by_policy = defaultdict(list)
    for row in rows:
        by_policy[row["policy"]].append(row)

    fields = ["policy", "runs"]
    for metric in METRICS:
        fields.extend([f"{metric}_mean", f"{metric}_stdev"])

    summary_rows = []
    for policy in sorted(by_policy):
        policy_rows = by_policy[policy]
        summary = {"policy": policy, "runs": len(policy_rows)}
        for metric in METRICS:
            values = [as_float(row, metric) for row in policy_rows]
            summary[f"{metric}_mean"] = mean(values)
            summary[f"{metric}_stdev"] = stdev(values)
        summary_rows.append(summary)

    summary_csv = output_dir / "policy-summary.csv"
    with summary_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)

    summary_md = output_dir / "policy-summary.md"
    md_fields = [
        "policy",
        "runs",
        "throughput_mbps_mean",
        "loss_ratio_mean",
        "mean_delay_ms_mean",
        "energy_saving_pct_mean",
        "unsafe_sleep_actions_mean",
        "sla_violation_mean",
    ]
    with summary_md.open("w") as handle:
        handle.write("| " + " | ".join(md_fields) + " |\n")
        handle.write("| " + " | ".join(["---"] * len(md_fields)) + " |\n")
        for row in summary_rows:
            cells = []
            for field in md_fields:
                value = row[field]
                cells.append(str(value) if isinstance(value, str) else format_number(float(value)))
            handle.write("| " + " | ".join(cells) + " |\n")

    return summary_csv, summary_md


def write_pairwise_comparison(rows, output_dir):
    scenario_fields = [
        "seed",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
        "uncertainty_scale",
        "traffic_profile",
        "burst_rate_multiplier",
        "shift_start_s",
        "shift_stop_s",
    ]
    by_scenario = defaultdict(dict)
    for row in rows:
        key = tuple(row.get(field, "") for field in scenario_fields)
        by_scenario[key][row["policy"]] = row

    comparison_fields = scenario_fields + [
        "energy_saving_twin_minus_threshold",
        "loss_twin_minus_threshold",
        "unsafe_twin_minus_threshold",
        "energy_saving_twin_minus_aggressive",
        "loss_twin_minus_aggressive",
        "unsafe_twin_minus_aggressive",
    ]
    comparison_rows = []
    for key, policies in sorted(by_scenario.items()):
        if "twin" not in policies:
            continue

        row = dict(zip(scenario_fields, key))
        twin = policies["twin"]
        for baseline in ["threshold", "aggressive"]:
            if baseline not in policies:
                continue
            row[f"energy_saving_twin_minus_{baseline}"] = (
                as_float(twin, "energy_saving_pct") -
                as_float(policies[baseline], "energy_saving_pct")
            )
            row[f"loss_twin_minus_{baseline}"] = (
                as_float(twin, "loss_ratio") - as_float(policies[baseline], "loss_ratio")
            )
            row[f"unsafe_twin_minus_{baseline}"] = (
                as_float(twin, "unsafe_sleep_actions") -
                as_float(policies[baseline], "unsafe_sleep_actions")
            )
        comparison_rows.append(row)

    comparison_csv = output_dir / "pairwise-comparison.csv"
    with comparison_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=comparison_fields)
        writer.writeheader()
        writer.writerows(comparison_rows)

    return comparison_csv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aggregate_csv", nargs="+", help="Path to aggregate.csv from the sweep runner")
    parser.add_argument("--output-dir", default="", help="Directory for analysis outputs")
    args = parser.parse_args()

    aggregate_paths = [Path(path) for path in args.aggregate_csv]
    output_dir = Path(args.output_dir) if args.output_dir else aggregate_paths[0].parent
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    for aggregate_csv in aggregate_paths:
        with aggregate_csv.open(newline="") as handle:
            rows.extend(csv.DictReader(handle))

    if not rows:
        raise SystemExit("No rows found in aggregate CSV")

    summary_csv, summary_md = write_policy_summary(rows, output_dir)
    comparison_csv = write_pairwise_comparison(rows, output_dir)

    print(f"Wrote {summary_csv}")
    print(f"Wrote {summary_md}")
    print(f"Wrote {comparison_csv}")


if __name__ == "__main__":
    main()
