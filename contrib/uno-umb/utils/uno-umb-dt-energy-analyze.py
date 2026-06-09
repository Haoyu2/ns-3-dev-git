#!/usr/bin/env python3
"""Summarize UNO-UMB digital-twin energy sweep results."""

import argparse
import csv
import html
import math
from collections import defaultdict
from pathlib import Path


BASE_METRICS = [
    "throughput_mbps",
    "loss_ratio",
    "mean_delay_ms",
    "energy_saving_pct",
    "unsafe_sleep_actions",
    "handover_requests",
    "sla_violation",
    "simulation_failure",
]
DERIVED_METRICS = [
    "safe_run",
    "safe_energy_saving_pct",
]
METRICS = BASE_METRICS + DERIVED_METRICS
FEASIBILITY_METRICS = [
    "throughput_mbps",
    "loss_ratio",
    "energy_saving_pct",
    "safe_run",
    "safe_energy_saving_pct",
    "excess_loss_vs_all_on",
    "throughput_delta_vs_all_on",
    "controller_induced_sla_violation",
    "simulation_failure",
]
ENVELOPE_METRICS = [
    "throughput_mbps",
    "loss_ratio",
    "energy_saving_pct",
    "safe_run",
    "safe_energy_saving_pct",
    "excess_loss_vs_all_on",
    "throughput_delta_vs_all_on",
    "controller_induced_sla_violation",
    "simulation_failure",
]

POLICY_COLORS = {
    "all-on": "#4b5563",
    "threshold": "#2563eb",
    "aggressive": "#dc2626",
    "twin": "#059669",
    "adaptive-twin": "#7c3aed",
}


def as_float(row, field):
    return float(row[field])


def is_failed_run(row):
    return row.get("run_status", "ok") not in {"", "ok"}


def normalize_row(row):
    if not row.get("forecast_lead_time_s", ""):
        row["forecast_lead_time_s"] = "0.0"
    row.setdefault("min_forecast_lead_time_s", "0.0")
    row.setdefault(
        "forecast_lead_applied",
        "1" if float(row["forecast_lead_time_s"]) > 0.0 else "0",
    )
    row.setdefault("forecast_burst_rate_multiplier", row.get("burst_rate_multiplier", "1.0"))
    row.setdefault("forecast_burst_rate_error", "0.0")
    row.setdefault("forecast_burst_rate_uncertainty", "0.0")
    row.setdefault("forecast_utilization_margin", "0.0")
    row.setdefault("controller_shift_start_s", row.get("shift_start_s", ""))
    return row


def metric_value(row, field):
    if field == "simulation_failure":
        return 1.0 if is_failed_run(row) else 0.0
    if field == "safe_run":
        safe = (
            as_float(row, "sla_violation") < 0.5
            and as_float(row, "unsafe_sleep_actions") < 0.5
            and metric_value(row, "simulation_failure") < 0.5
        )
        return 1.0 if safe else 0.0
    if field == "safe_energy_saving_pct":
        return as_float(row, "energy_saving_pct") if metric_value(row, "safe_run") else 0.0
    return as_float(row, field)


def feasibility_metric_value(row, field):
    if field == "simulation_failure":
        return 1.0 if is_failed_run(row) else 0.0
    if field == "safe_run":
        return (
            1.0
            if as_float(row, "sla_violation") < 0.5
            and feasibility_metric_value(row, "simulation_failure") < 0.5
            else 0.0
        )
    if field == "safe_energy_saving_pct":
        if feasibility_metric_value(row, "safe_run"):
            return as_float(row, "energy_saving_pct")
        return 0.0
    return as_float(row, field)


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


def scenario_key(row, fields):
    key = []
    for field in fields:
        value = row.get(field, "")
        if field in {"traffic_profile", "policy"}:
            key.append(value)
            continue
        try:
            key.append(f"{float(value):.9g}")
        except ValueError:
            key.append(value)
    return tuple(key)


def write_policy_summary(rows, output_dir):
    summary_rows = summarize_rows(rows, ["policy"])
    summary_csv = output_dir / "policy-summary.csv"
    summary_md = output_dir / "policy-summary.md"
    write_summary_csv(summary_rows, ["policy"], summary_csv)
    write_summary_md(
        summary_rows,
        [
            "policy",
            "runs",
            "throughput_mbps_mean",
            "loss_ratio_mean",
            "mean_delay_ms_mean",
            "energy_saving_pct_mean",
            "safe_energy_saving_pct_mean",
            "unsafe_sleep_actions_mean",
            "sla_violation_mean",
            "simulation_failure_mean",
            "safe_run_mean",
        ],
        summary_md,
    )
    return summary_csv, summary_md, summary_rows


def write_run_status_summary(rows, output_dir):
    group_fields = [
        "policy",
        "traffic_profile",
        "burst_rate_multiplier",
        "uncertainty_scale",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
    ]
    grouped = defaultdict(list)
    for row in rows:
        key = tuple(row.get(field, "") for field in group_fields)
        grouped[key].append(row)

    summary_rows = []
    for key, group_rows in sorted(grouped.items()):
        failed = sum(1 for row in group_rows if is_failed_run(row))
        total = len(group_rows)
        summary_rows.append(
            {
                **dict(zip(group_fields, key)),
                "runs": total,
                "completed_runs": total - failed,
                "failed_runs": failed,
                "failure_rate": failed / total if total else 0.0,
            }
        )

    fields = group_fields + ["runs", "completed_runs", "failed_runs", "failure_rate"]
    summary_csv = output_dir / "run-status-summary.csv"
    with summary_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)

    summary_md = output_dir / "run-status-summary.md"
    write_summary_md(summary_rows, fields, summary_md)
    return summary_csv, summary_md


def summarize_rows(rows, group_fields):
    grouped = defaultdict(list)
    for row in rows:
        key = tuple(row.get(field, "") for field in group_fields)
        grouped[key].append(row)

    summary_rows = []
    for key, group_rows in sorted(grouped.items()):
        summary = dict(zip(group_fields, key))
        summary["runs"] = len(group_rows)
        for metric in METRICS:
            values = [metric_value(row, metric) for row in group_rows]
            summary[f"{metric}_mean"] = mean(values)
            summary[f"{metric}_stdev"] = stdev(values)
        summary_rows.append(summary)
    return summary_rows


def write_summary_csv(summary_rows, group_fields, output_csv):
    fields = list(group_fields) + ["runs"]
    for metric in METRICS:
        fields.extend([f"{metric}_mean", f"{metric}_stdev"])

    with output_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)


def write_summary_md(summary_rows, fields, output_md):
    with output_md.open("w") as handle:
        handle.write("| " + " | ".join(fields) + " |\n")
        handle.write("| " + " | ".join(["---"] * len(fields)) + " |\n")
        for row in summary_rows:
            cells = []
            for field in fields:
                value = row[field]
                if field == "runs" or field.endswith("_runs"):
                    cells.append(str(value))
                else:
                    cells.append(
                        str(value) if isinstance(value, str) else format_number(float(value))
                    )
            handle.write("| " + " | ".join(cells) + " |\n")


def write_scenario_summary(rows, output_dir):
    group_fields = [
        "policy",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
        "uncertainty_scale",
        "adaptive_min_uncertainty_scale",
        "adaptive_max_uncertainty_scale",
        "adaptive_load_shock_gain",
        "adaptive_utilization_gain",
        "adaptive_relaxation",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "traffic_profile",
        "burst_rate_multiplier",
        "shift_start_s",
        "shift_stop_s",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_lead_applied",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
        "controller_shift_start_s",
    ]
    summary_rows = summarize_rows(rows, group_fields)
    summary_csv = output_dir / "scenario-summary.csv"
    summary_md = output_dir / "scenario-summary.md"
    write_summary_csv(summary_rows, group_fields, summary_csv)
    write_summary_md(
        summary_rows,
        group_fields
        + [
            "runs",
            "energy_saving_pct_mean",
            "safe_energy_saving_pct_mean",
            "loss_ratio_mean",
            "unsafe_sleep_actions_mean",
            "sla_violation_mean",
            "simulation_failure_mean",
            "safe_run_mean",
        ],
        summary_md,
    )
    return summary_csv, summary_md


def write_pairwise_comparison(rows, output_dir):
    scenario_fields = [
        "seed",
        "run",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
        "uncertainty_scale",
        "adaptive_min_uncertainty_scale",
        "adaptive_max_uncertainty_scale",
        "adaptive_load_shock_gain",
        "adaptive_utilization_gain",
        "adaptive_relaxation",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "traffic_profile",
        "burst_rate_multiplier",
        "shift_start_s",
        "shift_stop_s",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_lead_applied",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
        "controller_shift_start_s",
    ]
    by_scenario = defaultdict(dict)
    for row in rows:
        key = tuple(row.get(field, "") for field in scenario_fields)
        by_scenario[key][row["policy"]] = row

    comparison_fields = scenario_fields + [
        "energy_saving_twin_minus_threshold",
        "safe_energy_saving_twin_minus_threshold",
        "loss_twin_minus_threshold",
        "sla_violation_twin_minus_threshold",
        "unsafe_twin_minus_threshold",
        "energy_saving_twin_minus_aggressive",
        "safe_energy_saving_twin_minus_aggressive",
        "loss_twin_minus_aggressive",
        "sla_violation_twin_minus_aggressive",
        "unsafe_twin_minus_aggressive",
        "energy_saving_adaptive_minus_twin",
        "safe_energy_saving_adaptive_minus_twin",
        "loss_adaptive_minus_twin",
        "sla_violation_adaptive_minus_twin",
        "unsafe_adaptive_minus_twin",
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
                as_float(twin, "energy_saving_pct")
                - as_float(policies[baseline], "energy_saving_pct")
            )
            row[f"safe_energy_saving_twin_minus_{baseline}"] = (
                metric_value(twin, "safe_energy_saving_pct")
                - metric_value(policies[baseline], "safe_energy_saving_pct")
            )
            row[f"loss_twin_minus_{baseline}"] = (
                as_float(twin, "loss_ratio") - as_float(policies[baseline], "loss_ratio")
            )
            row[f"sla_violation_twin_minus_{baseline}"] = (
                as_float(twin, "sla_violation") - as_float(policies[baseline], "sla_violation")
            )
            row[f"unsafe_twin_minus_{baseline}"] = (
                as_float(twin, "unsafe_sleep_actions")
                - as_float(policies[baseline], "unsafe_sleep_actions")
            )
        if "adaptive-twin" in policies:
            adaptive = policies["adaptive-twin"]
            row["energy_saving_adaptive_minus_twin"] = (
                as_float(adaptive, "energy_saving_pct") - as_float(twin, "energy_saving_pct")
            )
            row["safe_energy_saving_adaptive_minus_twin"] = (
                metric_value(adaptive, "safe_energy_saving_pct")
                - metric_value(twin, "safe_energy_saving_pct")
            )
            row["loss_adaptive_minus_twin"] = (
                as_float(adaptive, "loss_ratio") - as_float(twin, "loss_ratio")
            )
            row["sla_violation_adaptive_minus_twin"] = (
                as_float(adaptive, "sla_violation") - as_float(twin, "sla_violation")
            )
            row["unsafe_adaptive_minus_twin"] = (
                as_float(adaptive, "unsafe_sleep_actions") -
                as_float(twin, "unsafe_sleep_actions")
            )
        comparison_rows.append(row)

    comparison_csv = output_dir / "pairwise-comparison.csv"
    with comparison_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=comparison_fields)
        writer.writeheader()
        writer.writerows(comparison_rows)

    return comparison_csv


def write_feasibility_comparison(rows, output_dir):
    scenario_fields = [
        "seed",
        "run",
        "enbs",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
        "traffic_profile",
        "burst_rate_multiplier",
        "shift_start_s",
        "shift_stop_s",
    ]
    all_on_rows = {}
    for row in rows:
        if row["policy"] != "all-on":
            continue
        key = scenario_key(row, scenario_fields)
        all_on_rows.setdefault(key, row)

    comparison_fields = scenario_fields + [
        "policy",
        "run_status",
        "return_code",
        "uncertainty_scale",
        "adaptive_min_uncertainty_scale",
        "adaptive_max_uncertainty_scale",
        "adaptive_load_shock_gain",
        "adaptive_utilization_gain",
        "adaptive_relaxation",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_lead_applied",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
        "all_on_throughput_mbps",
        "all_on_loss_ratio",
        "all_on_sla_violation",
        "all_on_feasible",
        "throughput_mbps",
        "loss_ratio",
        "sla_violation",
        "energy_saving_pct",
        "safe_energy_saving_pct",
        "excess_loss_vs_all_on",
        "throughput_delta_vs_all_on",
        "controller_induced_sla_violation",
        "simulation_failure",
        "run_log",
    ]
    comparison_rows = []
    for row in rows:
        if row["policy"] == "all-on":
            continue
        key = scenario_key(row, scenario_fields)
        all_on = all_on_rows.get(key)
        if not all_on:
            continue

        all_on_sla_violation = as_float(all_on, "sla_violation")
        controller_sla_violation = as_float(row, "sla_violation")
        comparison_rows.append(
            {
                **dict(zip(scenario_fields, key)),
                "policy": row["policy"],
                "run_status": row.get("run_status", "ok"),
                "return_code": row.get("return_code", ""),
                "uncertainty_scale": row.get("uncertainty_scale", ""),
                "adaptive_min_uncertainty_scale": row.get("adaptive_min_uncertainty_scale", ""),
                "adaptive_max_uncertainty_scale": row.get("adaptive_max_uncertainty_scale", ""),
                "adaptive_load_shock_gain": row.get("adaptive_load_shock_gain", ""),
                "adaptive_utilization_gain": row.get("adaptive_utilization_gain", ""),
                "adaptive_relaxation": row.get("adaptive_relaxation", ""),
                "adaptive_latent_load_threshold": row.get("adaptive_latent_load_threshold", ""),
                "adaptive_wake_relief_threshold": row.get("adaptive_wake_relief_threshold", ""),
                "forecast_lead_time_s": row.get("forecast_lead_time_s", ""),
                "min_forecast_lead_time_s": row.get("min_forecast_lead_time_s", ""),
                "forecast_lead_applied": row.get("forecast_lead_applied", ""),
                "forecast_burst_rate_multiplier": row.get("forecast_burst_rate_multiplier", ""),
                "forecast_burst_rate_error": row.get("forecast_burst_rate_error", ""),
                "forecast_burst_rate_uncertainty": row.get(
                    "forecast_burst_rate_uncertainty",
                    "",
                ),
                "forecast_utilization_margin": row.get("forecast_utilization_margin", ""),
                "all_on_throughput_mbps": as_float(all_on, "throughput_mbps"),
                "all_on_loss_ratio": as_float(all_on, "loss_ratio"),
                "all_on_sla_violation": all_on_sla_violation,
                "all_on_feasible": 1.0 if all_on_sla_violation < 0.5 else 0.0,
                "throughput_mbps": as_float(row, "throughput_mbps"),
                "loss_ratio": as_float(row, "loss_ratio"),
                "sla_violation": controller_sla_violation,
                "energy_saving_pct": as_float(row, "energy_saving_pct"),
                "safe_energy_saving_pct": metric_value(row, "safe_energy_saving_pct"),
                "excess_loss_vs_all_on": as_float(row, "loss_ratio") -
                as_float(all_on, "loss_ratio"),
                "throughput_delta_vs_all_on": as_float(row, "throughput_mbps") -
                as_float(all_on, "throughput_mbps"),
                "controller_induced_sla_violation": 1.0
                if controller_sla_violation > 0.5 and all_on_sla_violation < 0.5
                else 0.0,
                "simulation_failure": 1.0 if is_failed_run(row) else 0.0,
                "run_log": row.get("run_log", ""),
            }
        )

    comparison_csv = output_dir / "feasibility-comparison.csv"
    with comparison_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=comparison_fields)
        writer.writeheader()
        writer.writerows(comparison_rows)

    feasible_summary_csv, feasible_summary_md = write_feasible_policy_summary(
        comparison_rows,
        output_dir,
    )
    envelope_csv, envelope_md = write_feasibility_envelope_summary(
        comparison_rows,
        output_dir,
    )
    return comparison_csv, feasible_summary_csv, feasible_summary_md, envelope_csv, envelope_md


def write_feasible_policy_summary(feasibility_rows, output_dir):
    group_fields = [
        "policy",
        "uncertainty_scale",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_lead_applied",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
    ]
    grouped = defaultdict(list)
    for row in feasibility_rows:
        if as_float(row, "all_on_feasible") < 0.5:
            continue
        key = tuple(row.get(field, "") for field in group_fields)
        grouped[key].append(row)

    summary_rows = []
    for key, group_rows in sorted(grouped.items()):
        summary = dict(zip(group_fields, key))
        summary["runs"] = len(group_rows)
        for metric in FEASIBILITY_METRICS:
            values = [feasibility_metric_value(row, metric) for row in group_rows]
            summary[f"{metric}_mean"] = mean(values)
            summary[f"{metric}_stdev"] = stdev(values)
        summary_rows.append(summary)

    fields = list(group_fields) + ["runs"]
    for metric in FEASIBILITY_METRICS:
        fields.extend([f"{metric}_mean", f"{metric}_stdev"])

    summary_csv = output_dir / "feasible-policy-summary.csv"
    with summary_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)

    summary_md = output_dir / "feasible-policy-summary.md"
    write_summary_md(
        summary_rows,
        group_fields
        + [
            "runs",
            "energy_saving_pct_mean",
            "safe_energy_saving_pct_mean",
            "loss_ratio_mean",
            "excess_loss_vs_all_on_mean",
            "controller_induced_sla_violation_mean",
            "simulation_failure_mean",
            "safe_run_mean",
        ],
        summary_md,
    )
    return summary_csv, summary_md


def write_feasibility_envelope_summary(feasibility_rows, output_dir):
    envelope_fields = [
        "traffic_profile",
        "burst_rate_multiplier",
        "ues",
        "ue_rate_mbps",
        "enb_spacing_m",
    ]
    controller_fields = [
        "policy",
        "uncertainty_scale",
        "adaptive_latent_load_threshold",
        "adaptive_wake_relief_threshold",
        "forecast_lead_time_s",
        "min_forecast_lead_time_s",
        "forecast_lead_applied",
        "forecast_burst_rate_multiplier",
        "forecast_burst_rate_error",
        "forecast_burst_rate_uncertainty",
        "forecast_utilization_margin",
    ]

    all_on_by_envelope = {}
    controller_groups = defaultdict(list)
    for row in feasibility_rows:
        envelope_key = tuple(row.get(field, "") for field in envelope_fields)
        all_on_key = envelope_key + tuple(row.get(field, "") for field in ["seed", "run"])
        all_on_by_envelope.setdefault(envelope_key, {})
        all_on_by_envelope[envelope_key].setdefault(all_on_key, row)

        controller_key = envelope_key + tuple(row.get(field, "") for field in controller_fields)
        controller_groups[controller_key].append(row)

    summary_rows = []
    for key, group_rows in sorted(controller_groups.items()):
        envelope_key = key[: len(envelope_fields)]
        all_on_rows = list(all_on_by_envelope[envelope_key].values())
        feasible_rows = [row for row in group_rows if as_float(row, "all_on_feasible") > 0.5]

        summary = dict(zip(envelope_fields + controller_fields, key))
        summary["all_on_runs"] = len(all_on_rows)
        summary["all_on_feasible_rate"] = mean(
            [as_float(row, "all_on_feasible") for row in all_on_rows]
        )
        summary["all_on_loss_ratio_mean"] = mean(
            [as_float(row, "all_on_loss_ratio") for row in all_on_rows]
        )
        summary["feasible_controller_runs"] = len(feasible_rows)

        for metric in ENVELOPE_METRICS:
            values = [feasibility_metric_value(row, metric) for row in feasible_rows]
            summary[f"{metric}_on_feasible_mean"] = mean(values)
            summary[f"{metric}_on_feasible_stdev"] = stdev(values)
        summary_rows.append(summary)

    fields = (
        envelope_fields
        + controller_fields
        + [
            "all_on_runs",
            "all_on_feasible_rate",
            "all_on_loss_ratio_mean",
            "feasible_controller_runs",
        ]
    )
    for metric in ENVELOPE_METRICS:
        fields.extend([f"{metric}_on_feasible_mean", f"{metric}_on_feasible_stdev"])

    summary_csv = output_dir / "feasibility-envelope-summary.csv"
    with summary_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)

    summary_md = output_dir / "feasibility-envelope-summary.md"
    write_summary_md(
        summary_rows,
        envelope_fields
        + controller_fields
        + [
            "all_on_runs",
            "all_on_feasible_rate",
            "feasible_controller_runs",
            "safe_run_on_feasible_mean",
            "safe_energy_saving_pct_on_feasible_mean",
            "energy_saving_pct_on_feasible_mean",
            "controller_induced_sla_violation_on_feasible_mean",
            "simulation_failure_on_feasible_mean",
        ],
        summary_md,
    )
    return summary_csv, summary_md


def write_energy_risk_svg(summary_rows, output_dir):
    width = 760
    height = 460
    left = 78
    right = 38
    top = 44
    bottom = 72
    plot_width = width - left - right
    plot_height = height - top - bottom
    max_energy = max(5.0, max(row["energy_saving_pct_mean"] for row in summary_rows) * 1.15)
    max_loss = max(0.05, max(row["loss_ratio_mean"] for row in summary_rows) * 1.15)

    def x_pos(value):
        return left + plot_width * (value / max_energy)

    def y_pos(value):
        return top + plot_height * (1.0 - value / max_loss)

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text x="24" y="28" font-family="Arial, sans-serif" font-size="18" '
        'font-weight="700" fill="#111827">Energy and risk tradeoff</text>',
    ]

    for step in range(6):
        x = left + plot_width * step / 5
        energy = max_energy * step / 5
        lines.extend(
            [
                f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" '
                f'y2="{top + plot_height}" stroke="#e5e7eb"/>',
                f'<text x="{x:.2f}" y="{top + plot_height + 24}" '
                'font-family="Arial, sans-serif" font-size="11" '
                f'text-anchor="middle" fill="#4b5563">{energy:.0f}</text>',
            ]
        )

        y = top + plot_height * step / 5
        loss = max_loss * (5 - step) / 5
        lines.extend(
            [
                f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_width}" '
                f'y2="{y:.2f}" stroke="#e5e7eb"/>',
                f'<text x="{left - 10}" y="{y + 4:.2f}" '
                'font-family="Arial, sans-serif" font-size="11" '
                f'text-anchor="end" fill="#4b5563">{loss:.2f}</text>',
            ]
        )

    lines.extend(
        [
            f'<line x1="{left}" y1="{top + plot_height}" x2="{left + plot_width}" '
            f'y2="{top + plot_height}" stroke="#111827"/>',
            f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_height}" '
            'stroke="#111827"/>',
            f'<text x="{left + plot_width / 2:.2f}" y="{height - 20}" '
            'font-family="Arial, sans-serif" font-size="13" '
            'text-anchor="middle" fill="#111827">Mean energy saving (%)</text>',
            f'<text x="18" y="{top + plot_height / 2:.2f}" '
            'font-family="Arial, sans-serif" font-size="13" text-anchor="middle" '
            'fill="#111827" transform="rotate(-90 18 '
            f'{top + plot_height / 2:.2f})">Mean loss ratio</text>',
        ]
    )

    for row in summary_rows:
        policy = str(row["policy"])
        energy = row["energy_saving_pct_mean"]
        loss = row["loss_ratio_mean"]
        safe_rate = row["safe_run_mean"]
        color = POLICY_COLORS.get(policy, "#7c3aed")
        stroke = "#111827" if safe_rate >= 0.999 else "#b91c1c"
        cx = x_pos(energy)
        cy = y_pos(loss)
        label_y = cy - 13 if cy > top + 28 else cy + 23
        lines.extend(
            [
                f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="8" fill="{color}" '
                f'stroke="{stroke}" stroke-width="2"/>',
                f'<text x="{cx:.2f}" y="{label_y:.2f}" '
                'font-family="Arial, sans-serif" font-size="12" '
                f'text-anchor="middle" fill="#111827">{html.escape(policy)}</text>',
            ]
        )

    lines.append("</svg>")
    svg_path = output_dir / "energy-risk.svg"
    with svg_path.open("w") as handle:
        handle.write("\n".join(lines))
        handle.write("\n")
    return svg_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "aggregate_csv",
        nargs="+",
        help="Path to aggregate.csv from the sweep runner",
    )
    parser.add_argument("--output-dir", default="", help="Directory for analysis outputs")
    args = parser.parse_args()

    aggregate_paths = [Path(path) for path in args.aggregate_csv]
    output_dir = Path(args.output_dir) if args.output_dir else aggregate_paths[0].parent
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    for aggregate_csv in aggregate_paths:
        with aggregate_csv.open(newline="") as handle:
            rows.extend(normalize_row(row) for row in csv.DictReader(handle))

    if not rows:
        raise SystemExit("No rows found in aggregate CSV")

    run_status_csv, run_status_md = write_run_status_summary(rows, output_dir)
    summary_csv, summary_md, policy_summary_rows = write_policy_summary(rows, output_dir)
    scenario_csv, scenario_md = write_scenario_summary(rows, output_dir)
    comparison_csv = write_pairwise_comparison(rows, output_dir)
    (
        feasibility_csv,
        feasible_summary_csv,
        feasible_summary_md,
        envelope_csv,
        envelope_md,
    ) = write_feasibility_comparison(
        rows,
        output_dir,
    )
    svg_path = write_energy_risk_svg(policy_summary_rows, output_dir)

    print(f"Wrote {run_status_csv}")
    print(f"Wrote {run_status_md}")
    print(f"Wrote {summary_csv}")
    print(f"Wrote {summary_md}")
    print(f"Wrote {scenario_csv}")
    print(f"Wrote {scenario_md}")
    print(f"Wrote {comparison_csv}")
    print(f"Wrote {feasibility_csv}")
    print(f"Wrote {feasible_summary_csv}")
    print(f"Wrote {feasible_summary_md}")
    print(f"Wrote {envelope_csv}")
    print(f"Wrote {envelope_md}")
    print(f"Wrote {svg_path}")


if __name__ == "__main__":
    main()
