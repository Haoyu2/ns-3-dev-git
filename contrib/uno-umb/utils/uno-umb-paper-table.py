#!/usr/bin/env python3
"""Build compact paper tables from UNO-UMB aggregate CSV files."""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


POLICIES = {"all-on", "twin", "adaptive-twin"}
CONTROL_ORDER = {
    "reference": 0,
    "no forecast": 1,
    "perfect 1s lead": 2,
    "-25% forecast + 0.25 margin": 3,
    "-25% forecast + gated 0.25 margin": 4,
    "-25% forecast + 0.35 margin": 5,
    "-25% forecast + gated 0.35 margin": 6,
    "-25% forecast + selective 0.25/0.35 margin": 7,
    "-25% forecast + gated selective 0.25/0.35 margin": 8,
    "-25% forecast + selective overlap margin": 9,
    "-25% forecast + gated selective overlap margin": 10,
}
POLICY_ORDER = {
    "all-on": 0,
    "twin": 1,
    "adaptive-twin": 2,
}
Z_95 = 1.959963984540054


def as_float(row, field, default=0.0):
    value = row.get(field, "")
    return default if value == "" else float(value)


def is_failed(row):
    return row.get("run_status", "ok") not in {"", "ok"}


def is_safe(row):
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


def control_label(row):
    if row["policy"] == "all-on":
        return "reference"

    profile = row["traffic_profile"]
    lead = as_float(row, "forecast_lead_time_s")
    min_lead = as_float(row, "min_forecast_lead_time_s")
    min_extra = as_float(row, "forecast_min_burst_extra_load_mbps")
    error = as_float(row, "forecast_burst_rate_error")
    uncertainty = as_float(row, "forecast_burst_rate_uncertainty")
    selective_uncertainty = as_float(row, "selective_forecast_burst_rate_uncertainty")
    trigger_slack = as_float(row, "forecast_margin_trigger_slack")
    trigger_max_offload = as_float(row, "forecast_margin_trigger_max_offload_m")

    if profile == "center-burst" or lead == 0.0:
        return "no forecast"
    if lead == 1.0 and min_lead == 0.0 and error == 0.0 and uncertainty == 0.0:
        return "perfect 1s lead"
    if lead == 1.0 and min_lead == 1.0 and error == -0.25:
        if uncertainty == 0.25 and selective_uncertainty == 0.35 and trigger_slack > 0.0:
            if trigger_max_offload > 0.0:
                if min_extra > 0.0:
                    return "-25% forecast + gated selective overlap margin"
                return "-25% forecast + selective overlap margin"
            if min_extra > 0.0:
                return "-25% forecast + gated selective 0.25/0.35 margin"
            return "-25% forecast + selective 0.25/0.35 margin"
        if uncertainty == 0.25 and selective_uncertainty == 0.0:
            if min_extra > 0.0:
                return "-25% forecast + gated 0.25 margin"
            return "-25% forecast + 0.25 margin"
        if uncertainty == 0.35 and selective_uncertainty == 0.0:
            if min_extra > 0.0:
                return "-25% forecast + gated 0.35 margin"
            return "-25% forecast + 0.35 margin"
    return "other"


def scenario_label(profile, multiplier):
    prefix = {
        "center-burst": "center",
        "right-edge-burst": "right-edge",
        "edge-burst": "edge",
        "global-burst": "global",
        "steady": "steady",
    }.get(profile, profile)
    return f"{prefix} {multiplier:g}x"


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stdev(values):
    if len(values) < 2:
        return 0.0
    average = mean(values)
    return math.sqrt(sum((value - average) ** 2 for value in values) / (len(values) - 1))


def proportion_ci95(successes, total):
    if total <= 0:
        return 0.0, 0.0
    proportion = successes / total
    z2 = Z_95 * Z_95
    denominator = 1.0 + z2 / total
    center = (proportion + z2 / (2.0 * total)) / denominator
    half_width = (
        Z_95
        * math.sqrt((proportion * (1.0 - proportion) + z2 / (4.0 * total)) / total)
        / denominator
    )
    return max(0.0, center - half_width), min(1.0, center + half_width)


def mean_ci95(values):
    if not values:
        return 0.0, 0.0
    average = mean(values)
    if len(values) < 2:
        return average, average
    half_width = Z_95 * stdev(values) / math.sqrt(len(values))
    return average - half_width, average + half_width


def format_number(value):
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


def wanted_row(row):
    if row["policy"] not in POLICIES:
        return False
    if control_label(row) == "other":
        return False
    return row["traffic_profile"] in {"center-burst", "right-edge-burst"}


def dedupe_rows(rows):
    deduped = {}
    for index, row in enumerate(rows):
        if not wanted_row(row):
            continue
        key = sample_key(row) + (control_label(row), row["policy"])
        deduped[key] = (index, row)
    return [row for _, row in sorted(deduped.values())]


def summarize(rows):
    all_on = {}
    for row in rows:
        if row["policy"] == "all-on":
            all_on[sample_key(row)] = row

    grouped = defaultdict(list)
    for row in rows:
        profile, multiplier, ues, ue_rate, spacing = scenario_key(row)
        key = (
            profile,
            multiplier,
            ues,
            ue_rate,
            control_label(row),
            row["policy"],
        )
        grouped[key].append(row)

    summary_rows = []
    for key, group_rows in sorted(grouped.items()):
        profile, multiplier, ues, ue_rate, control, policy = key
        spacings = sorted({as_float(row, "enb_spacing_m") for row in group_rows})
        base = {
            "scenario": scenario_label(profile, multiplier),
            "control": control,
            "policy": policy,
            "ues": ues,
            "ue_rate_mbps": ue_rate,
            "spacings_m": ",".join(f"{spacing:g}" for spacing in spacings),
            "rows": len(group_rows),
        }

        if policy == "all-on":
            feasible = [1.0 if as_float(row, "sla_violation") < 0.5 else 0.0 for row in group_rows]
            safe_runs = int(sum(feasible))
            failures = [1.0 if is_failed(row) else 0.0 for row in group_rows]
            safe_low, safe_high = proportion_ci95(safe_runs, len(feasible))
            base.update(
                {
                    "all_on_feasible_rows": safe_runs,
                    "all_on_feasible_rate": mean(feasible),
                    "feasible_controller_rows": 0,
                    "safe_runs": safe_runs,
                    "safe_run_rate": mean(feasible),
                    "safe_run_ci95_low": safe_low,
                    "safe_run_ci95_high": safe_high,
                    "safe_energy_saving_pct_mean": 0.0,
                    "safe_energy_saving_pct_stdev": 0.0,
                    "safe_energy_saving_pct_ci95_low": 0.0,
                    "safe_energy_saving_pct_ci95_high": 0.0,
                    "induced_violations": 0,
                    "induced_violation_rate": 0.0,
                    "failures": int(sum(failures)),
                    "failure_rate": mean(failures),
                }
            )
            summary_rows.append(base)
            continue

        feasible_rows = []
        for row in group_rows:
            reference = all_on.get(sample_key(row))
            if reference and as_float(reference, "sla_violation") < 0.5:
                feasible_rows.append(row)

        safe_flags = [1.0 if is_safe(row) else 0.0 for row in feasible_rows]
        safe_energy = [
            as_float(row, "energy_saving_pct") if is_safe(row) else 0.0
            for row in feasible_rows
        ]
        induced = [
            1.0 if as_float(row, "sla_violation") > 0.5 else 0.0
            for row in feasible_rows
        ]
        failures = [1.0 if is_failed(row) else 0.0 for row in feasible_rows]
        reference_feasible = [
            1.0
            for row in group_rows
            if all_on.get(sample_key(row))
            and as_float(all_on[sample_key(row)], "sla_violation") < 0.5
        ]
        safe_runs = int(sum(safe_flags))
        safe_low, safe_high = proportion_ci95(safe_runs, len(safe_flags))
        saving_low, saving_high = mean_ci95(safe_energy)

        base.update(
            {
                "all_on_feasible_rows": len(reference_feasible),
                "all_on_feasible_rate": len(reference_feasible) / len(group_rows)
                if group_rows
                else 0.0,
                "feasible_controller_rows": len(feasible_rows),
                "safe_runs": safe_runs,
                "safe_run_rate": mean(safe_flags),
                "safe_run_ci95_low": safe_low,
                "safe_run_ci95_high": safe_high,
                "safe_energy_saving_pct_mean": mean(safe_energy),
                "safe_energy_saving_pct_stdev": stdev(safe_energy),
                "safe_energy_saving_pct_ci95_low": saving_low,
                "safe_energy_saving_pct_ci95_high": saving_high,
                "induced_violations": int(sum(induced)),
                "induced_violation_rate": mean(induced),
                "failures": int(sum(failures)),
                "failure_rate": mean(failures),
            }
        )
        summary_rows.append(base)
    return sorted(
        summary_rows,
        key=lambda row: (
            row["scenario"],
            CONTROL_ORDER.get(row["control"], 99),
            POLICY_ORDER.get(row["policy"], 99),
        ),
    )


def write_csv(rows, output_csv):
    fields = [
        "scenario",
        "control",
        "policy",
        "ues",
        "ue_rate_mbps",
        "spacings_m",
        "rows",
        "all_on_feasible_rows",
        "all_on_feasible_rate",
        "feasible_controller_rows",
        "safe_runs",
        "safe_run_rate",
        "safe_run_ci95_low",
        "safe_run_ci95_high",
        "safe_energy_saving_pct_mean",
        "safe_energy_saving_pct_stdev",
        "safe_energy_saving_pct_ci95_low",
        "safe_energy_saving_pct_ci95_high",
        "induced_violations",
        "induced_violation_rate",
        "failures",
        "failure_rate",
    ]
    with output_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def write_markdown(rows, output_md):
    fields = [
        "scenario",
        "control",
        "policy",
        "rows",
        "all_on_feasible_rows",
        "all_on_feasible_rate",
        "feasible_controller_rows",
        "safe_runs",
        "safe_run_rate",
        "safe_run_ci95_low",
        "safe_run_ci95_high",
        "safe_energy_saving_pct_mean",
        "safe_energy_saving_pct_stdev",
        "safe_energy_saving_pct_ci95_low",
        "safe_energy_saving_pct_ci95_high",
        "induced_violations",
        "induced_violation_rate",
        "failures",
        "failure_rate",
        "spacings_m",
    ]
    with output_md.open("w") as handle:
        handle.write("| " + " | ".join(fields) + " |\n")
        handle.write("| " + " | ".join(["---"] * len(fields)) + " |\n")
        for row in rows:
            cells = []
            for field in fields:
                value = row[field]
                if isinstance(value, float):
                    cells.append(format_number(value))
                else:
                    cells.append(str(value))
            handle.write("| " + " | ".join(cells) + " |\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aggregate_csv", nargs="+", type=Path)
    parser.add_argument("--output-dir", default="", help="Directory for generated tables")
    args = parser.parse_args()

    output_dir = Path(args.output_dir) if args.output_dir else args.aggregate_csv[0].parent
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = dedupe_rows(load_rows(args.aggregate_csv))
    summary_rows = summarize(rows)

    csv_path = output_dir / "paper-policy-table.csv"
    md_path = output_dir / "paper-policy-table.md"
    write_csv(summary_rows, csv_path)
    write_markdown(summary_rows, md_path)
    print(f"Wrote {csv_path}")
    print(f"Wrote {md_path}")


if __name__ == "__main__":
    main()
