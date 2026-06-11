#!/usr/bin/env python3
"""Summarize UNO-UMB load and UE-count sweep results."""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


POLICY_ORDER = {
    "all-on": 0,
    "twin": 1,
    "adaptive-twin": 2,
}
CONTROL_ORDER = {
    "reference": 0,
    "no forecast": 1,
    "-25% forecast + 0.25 margin": 2,
    "-25% forecast + gated 0.25 margin": 3,
    "-25% forecast + 0.35 margin": 4,
    "-25% forecast + gated 0.35 margin": 5,
    "-25% forecast + selective margin": 6,
    "-25% forecast + gated selective margin": 7,
}


def as_float(row, field, default=0.0):
    value = row.get(field, "")
    return default if value == "" else float(value)


def is_failed(row):
    return row.get("run_status", "ok") not in {"", "ok"}


def is_all_on_feasible(row):
    return not is_failed(row) and as_float(row, "sla_violation") < 0.5


def is_controller_safe(row):
    return (
        not is_failed(row)
        and as_float(row, "sla_violation") < 0.5
        and as_float(row, "unsafe_sleep_actions") < 0.5
    )


def normalize_row(row):
    if not row.get("forecast_lead_time_s", ""):
        row["forecast_lead_time_s"] = "0.0"
    row.setdefault("min_forecast_lead_time_s", "0.0")
    row.setdefault("forecast_min_burst_extra_load_mbps", "0.0")
    row.setdefault("forecast_burst_extra_load_mbps", "0.0")
    row.setdefault("forecast_burst_rate_error", "0.0")
    row.setdefault("forecast_burst_rate_uncertainty", "0.0")
    row.setdefault("selective_forecast_burst_rate_uncertainty", "0.0")
    row.setdefault("forecast_margin_trigger_slack", "0.0")
    row.setdefault("forecast_margin_trigger_max_offload_m", "0.0")
    row.setdefault("forecast_correction_delay_s", "-1.0")
    return row


def scenario_key(row):
    return (
        row["traffic_profile"],
        as_float(row, "burst_rate_multiplier"),
        int(as_float(row, "ues")),
        as_float(row, "ue_rate_mbps"),
        as_float(row, "enb_spacing_m"),
    )


def sample_key(row):
    return scenario_key(row) + (
        int(as_float(row, "seed")),
        int(as_float(row, "run")),
    )


def scenario_label(profile, multiplier):
    prefix = {
        "center-burst": "center",
        "right-edge-burst": "right-edge",
        "edge-burst": "edge",
        "global-burst": "global",
        "steady": "steady",
    }.get(profile, profile)
    return f"{prefix} {multiplier:g}x"


def control_label(row):
    if row["policy"] == "all-on":
        return "reference"

    if row["traffic_profile"] == "center-burst":
        return "no forecast"

    lead = as_float(row, "forecast_lead_time_s")
    min_lead = as_float(row, "min_forecast_lead_time_s")
    min_extra = as_float(row, "forecast_min_burst_extra_load_mbps")
    error = as_float(row, "forecast_burst_rate_error")
    uncertainty = as_float(row, "forecast_burst_rate_uncertainty")
    selective_uncertainty = as_float(row, "selective_forecast_burst_rate_uncertainty")
    trigger_slack = as_float(row, "forecast_margin_trigger_slack")

    if lead == 0.0:
        return "no forecast"
    if lead == 1.0 and min_lead == 1.0 and error == -0.25:
        if uncertainty == 0.25 and selective_uncertainty == 0.35 and trigger_slack > 0.0:
            if min_extra > 0.0:
                return "-25% forecast + gated selective margin"
            return "-25% forecast + selective margin"
        if uncertainty == 0.25 and selective_uncertainty == 0.0:
            if min_extra > 0.0:
                return "-25% forecast + gated 0.25 margin"
            return "-25% forecast + 0.25 margin"
        if uncertainty == 0.35 and selective_uncertainty == 0.0:
            if min_extra > 0.0:
                return "-25% forecast + gated 0.35 margin"
            return "-25% forecast + 0.35 margin"
    return "other"


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stdev(values):
    if len(values) < 2:
        return 0.0
    average = mean(values)
    return math.sqrt(sum((value - average) ** 2 for value in values) / (len(values) - 1))


def format_value(value):
    if isinstance(value, str):
        return value
    if isinstance(value, int):
        return str(value)
    if abs(value) >= 100:
        return f"{value:.2f}"
    if abs(value) >= 1:
        return f"{value:.3f}"
    return f"{value:.4f}"


def load_rows(paths):
    rows = []
    for path in paths:
        with path.open(newline="") as handle:
            rows.extend(normalize_row(row) for row in csv.DictReader(handle))
    return rows


def summarize_cells(rows):
    all_on = {}
    for row in rows:
        if row["policy"] == "all-on":
            all_on[sample_key(row)] = row

    grouped = defaultdict(list)
    for row in rows:
        control = control_label(row)
        if control == "other":
            continue
        profile, multiplier, ues, ue_rate, spacing = scenario_key(row)
        key = (
            scenario_label(profile, multiplier),
            control,
            row["policy"],
            ues,
            ue_rate,
            spacing,
        )
        grouped[key].append(row)

    summary_rows = []
    for key, group_rows in sorted(grouped.items()):
        scenario, control, policy, ues, ue_rate, spacing = key
        row = {
            "scenario": scenario,
            "control": control,
            "policy": policy,
            "ues": ues,
            "ue_rate_mbps": ue_rate,
            "enb_spacing_m": spacing,
            "rows": len(group_rows),
        }
        failures = [1.0 if is_failed(group_row) else 0.0 for group_row in group_rows]

        if policy == "all-on":
            feasible_flags = [1.0 if is_all_on_feasible(group_row) else 0.0 for group_row in group_rows]
            feasible_rows = int(sum(feasible_flags))
            row.update(
                {
                    "all_on_feasible_rows": feasible_rows,
                    "all_on_feasible_rate": mean(feasible_flags),
                    "feasible_controller_rows": 0,
                    "safe_runs": feasible_rows,
                    "safe_run_rate": mean(feasible_flags),
                    "safe_energy_saving_pct_mean": 0.0,
                    "safe_energy_saving_pct_stdev": 0.0,
                    "induced_violations": 0,
                    "induced_violation_rate": 0.0,
                    "failures": int(sum(failures)),
                    "failure_rate": mean(failures),
                }
            )
            summary_rows.append(row)
            continue

        feasible_rows = [
            group_row
            for group_row in group_rows
            if sample_key(group_row) in all_on and is_all_on_feasible(all_on[sample_key(group_row)])
        ]
        safe_flags = [1.0 if is_controller_safe(group_row) else 0.0 for group_row in feasible_rows]
        safe_energy = [
            as_float(group_row, "energy_saving_pct") if is_controller_safe(group_row) else 0.0
            for group_row in feasible_rows
        ]
        induced = [
            1.0 if as_float(group_row, "sla_violation") > 0.5 else 0.0
            for group_row in feasible_rows
        ]
        feasible_failures = [1.0 if is_failed(group_row) else 0.0 for group_row in feasible_rows]
        row.update(
            {
                "all_on_feasible_rows": len(feasible_rows),
                "all_on_feasible_rate": len(feasible_rows) / len(group_rows) if group_rows else 0.0,
                "feasible_controller_rows": len(feasible_rows),
                "safe_runs": int(sum(safe_flags)),
                "safe_run_rate": mean(safe_flags),
                "safe_energy_saving_pct_mean": mean(safe_energy),
                "safe_energy_saving_pct_stdev": stdev(safe_energy),
                "induced_violations": int(sum(induced)),
                "induced_violation_rate": mean(induced),
                "failures": int(sum(feasible_failures)),
                "failure_rate": mean(feasible_failures),
            }
        )
        summary_rows.append(row)

    return sorted(
        summary_rows,
        key=lambda row: (
            row["scenario"],
            row["ues"],
            row["ue_rate_mbps"],
            row["enb_spacing_m"],
            CONTROL_ORDER.get(row["control"], 99),
            POLICY_ORDER.get(row["policy"], 99),
        ),
    )


def summarize_controls(cell_rows, robust_threshold):
    grouped = defaultdict(list)
    for row in cell_rows:
        key = (row["scenario"], row["control"], row["policy"])
        grouped[key].append(row)

    summary_rows = []
    for key, rows in sorted(grouped.items()):
        scenario, control, policy = key
        denominators = [
            row["rows"] if policy == "all-on" else row["feasible_controller_rows"]
            for row in rows
        ]
        safe_runs = [row["safe_runs"] for row in rows]
        safe_energy_values = []
        for row in rows:
            denominator = row["rows"] if policy == "all-on" else row["feasible_controller_rows"]
            if denominator > 0:
                safe_energy_values.extend([row["safe_energy_saving_pct_mean"]] * denominator)

        evaluable_points = [
            row
            for row in rows
            if (row["rows"] if policy == "all-on" else row["feasible_controller_rows"]) > 0
        ]
        robust_points = [
            row
            for row in evaluable_points
            if row["safe_run_rate"] >= robust_threshold and row["failure_rate"] == 0.0
        ]
        total_denominator = sum(denominators)
        total_safe = sum(safe_runs)
        summary_rows.append(
            {
                "scenario": scenario,
                "control": control,
                "policy": policy,
                "load_points": len(rows),
                "evaluable_load_points": len(evaluable_points),
                "robust_load_points": len(robust_points),
                "robust_load_point_rate": len(robust_points) / len(evaluable_points)
                if evaluable_points
                else 0.0,
                "rows": sum(row["rows"] for row in rows),
                "all_on_feasible_rows": sum(row["all_on_feasible_rows"] for row in rows),
                "feasible_controller_rows": sum(row["feasible_controller_rows"] for row in rows),
                "safe_runs": total_safe,
                "safe_run_rate": total_safe / total_denominator if total_denominator else 0.0,
                "safe_energy_saving_pct_mean": mean(safe_energy_values),
                "induced_violations": sum(row["induced_violations"] for row in rows),
                "failures": sum(row["failures"] for row in rows),
            }
        )

    return sorted(
        summary_rows,
        key=lambda row: (
            row["scenario"],
            CONTROL_ORDER.get(row["control"], 99),
            POLICY_ORDER.get(row["policy"], 99),
        ),
    )


def write_csv(rows, fields, path):
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def write_markdown(rows, fields, path):
    with path.open("w") as handle:
        handle.write("| " + " | ".join(fields) + " |\n")
        handle.write("| " + " | ".join(["---"] * len(fields)) + " |\n")
        for row in rows:
            handle.write(
                "| " + " | ".join(format_value(row[field]) for field in fields) + " |\n"
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aggregate_csv", nargs="+", help="Sweep aggregate CSV files")
    parser.add_argument("--output-dir", default="", help="Directory for summary outputs")
    parser.add_argument(
        "--robust-threshold",
        type=float,
        default=0.999,
        help="Safe-run rate required for a load point to count as robust",
    )
    args = parser.parse_args()

    paths = [Path(path) for path in args.aggregate_csv]
    output_dir = Path(args.output_dir) if args.output_dir else paths[0].parent
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = load_rows(paths)
    cell_rows = summarize_cells(rows)
    control_rows = summarize_controls(cell_rows, args.robust_threshold)

    cell_fields = [
        "scenario",
        "control",
        "policy",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
        "rows",
        "all_on_feasible_rows",
        "all_on_feasible_rate",
        "feasible_controller_rows",
        "safe_runs",
        "safe_run_rate",
        "safe_energy_saving_pct_mean",
        "safe_energy_saving_pct_stdev",
        "induced_violations",
        "induced_violation_rate",
        "failures",
        "failure_rate",
    ]
    control_fields = [
        "scenario",
        "control",
        "policy",
        "load_points",
        "evaluable_load_points",
        "robust_load_points",
        "robust_load_point_rate",
        "rows",
        "all_on_feasible_rows",
        "feasible_controller_rows",
        "safe_runs",
        "safe_run_rate",
        "safe_energy_saving_pct_mean",
        "induced_violations",
        "failures",
    ]

    cell_csv = output_dir / "load-cell-summary.csv"
    cell_md = output_dir / "load-cell-summary.md"
    control_csv = output_dir / "load-control-summary.csv"
    control_md = output_dir / "load-control-summary.md"
    write_csv(cell_rows, cell_fields, cell_csv)
    write_markdown(cell_rows, cell_fields, cell_md)
    write_csv(control_rows, control_fields, control_csv)
    write_markdown(control_rows, control_fields, control_md)

    print(f"Wrote {cell_csv}")
    print(f"Wrote {cell_md}")
    print(f"Wrote {control_csv}")
    print(f"Wrote {control_md}")


if __name__ == "__main__":
    main()
