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


def run_command(command, cwd, log_path=None):
    print(" ".join(command), flush=True)
    if log_path is None:
        return subprocess.run(command, cwd=cwd, check=False).returncode

    with log_path.open("w") as log:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
        return process.wait()


def as_seconds(value):
    return float(value[:-1] if value.endswith("s") else value)


def shifted_ue_count(profile, ues, low_load_ues=2, number_of_enbs=3):
    if profile == "steady":
        return 0
    if profile == "global-burst":
        return ues
    if number_of_enbs == 3 and ues > low_load_ues:
        edge_ues = (ues - low_load_ues) // 2
        if profile == "center-burst":
            return low_load_ues
        if profile in {"edge-burst", "right-edge-burst"}:
            return edge_ues
    return 0


def make_failure_row(
    *,
    policy,
    seed,
    run,
    ues,
    ue_rate,
    spacing,
    uncertainty_scale,
    adaptive_min_uncertainty_scale,
    adaptive_max_uncertainty_scale,
    adaptive_load_shock_gain,
    adaptive_utilization_gain,
    adaptive_relaxation,
    adaptive_latent_load_threshold,
    adaptive_wake_relief_threshold,
    forecast_lead_time,
    traffic_profile,
    burst_rate_multiplier,
    shift_start,
    shift_stop,
    sim_time,
    run_id,
    summary_csv,
    event_csv,
    run_log,
    return_code,
):
    sim_time_s = as_seconds(sim_time)
    shift_start_s = as_seconds(shift_start)
    shift_stop_s = as_seconds(shift_stop)
    forecast_lead_time_s = as_seconds(forecast_lead_time)
    measurement_start_s = 0.7
    measurement_stop_s = max(sim_time_s - 0.1, measurement_start_s)
    measurement_s = max(measurement_stop_s - measurement_start_s, 1.0)
    burst_duration_s = max(
        min(shift_stop_s, measurement_stop_s) - max(shift_start_s, measurement_start_s),
        0.0,
    )
    shift_ues = shifted_ue_count(traffic_profile, ues)
    burst_extra_rate_mbps = ue_rate * (burst_rate_multiplier - 1.0)
    burst_extra_load_mbps = shift_ues * burst_extra_rate_mbps
    base_offered_load_mbps = ues * ue_rate
    offered_load_mbps = base_offered_load_mbps + burst_extra_load_mbps * (
        burst_duration_s / measurement_s
    )
    controller_shift_start_s = shift_start_s
    if shift_ues > 0 and burst_rate_multiplier > 1.0 and forecast_lead_time_s > 0.0:
        controller_shift_start_s = max(shift_start_s - forecast_lead_time_s, 0.6)

    return {
        "policy": policy,
        "traffic_profile": traffic_profile,
        "seed": seed,
        "run": run,
        "enbs": 3,
        "ues": ues,
        "ue_rate_mbps": ue_rate,
        "enb_spacing_m": spacing,
        "shift_ues": shift_ues,
        "sim_time_s": sim_time_s,
        "offered_load_mbps": offered_load_mbps,
        "base_offered_load_mbps": base_offered_load_mbps,
        "burst_extra_load_mbps": burst_extra_load_mbps,
        "burst_rate_multiplier": burst_rate_multiplier,
        "shift_start_s": shift_start_s,
        "shift_stop_s": shift_stop_s,
        "forecast_lead_time_s": forecast_lead_time_s,
        "controller_shift_start_s": controller_shift_start_s,
        "burst_duration_s": burst_duration_s,
        "uncertainty_scale": uncertainty_scale,
        "adaptive_min_uncertainty_scale": adaptive_min_uncertainty_scale,
        "adaptive_max_uncertainty_scale": adaptive_max_uncertainty_scale,
        "adaptive_load_shock_gain": adaptive_load_shock_gain,
        "adaptive_utilization_gain": adaptive_utilization_gain,
        "adaptive_relaxation": adaptive_relaxation,
        "adaptive_latent_load_threshold": adaptive_latent_load_threshold,
        "adaptive_wake_relief_threshold": adaptive_wake_relief_threshold,
        "throughput_mbps": 0.0,
        "tx_packets": 0,
        "rx_packets": 0,
        "tx_bytes": 0,
        "rx_bytes": 0,
        "loss_ratio": 1.0,
        "mean_delay_ms": 0.0,
        "energy_j": 0.0,
        "all_on_energy_j": 0.0,
        "energy_saving_pct": 0.0,
        "active_cell_seconds": 0.0,
        "unsafe_sleep_actions": 0,
        "handover_requests": 0,
        "sla_violation": 1,
        "run_id": run_id,
        "summary_csv": str(summary_csv),
        "event_csv": str(event_csv),
        "run_status": "failed",
        "return_code": return_code,
        "failure_reason": "nonzero-exit",
        "run_log": str(run_log),
    }


def write_aggregate_csv(aggregate_rows, aggregate_csv):
    fieldnames = []
    for row in aggregate_rows:
        for field in row:
            if field not in fieldnames:
                fieldnames.append(field)

    with aggregate_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(aggregate_rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policies", default="all-on,threshold,aggressive,twin,adaptive-twin")
    parser.add_argument("--seeds", default="1")
    parser.add_argument("--runs", default="1")
    parser.add_argument("--ue-counts", default="16")
    parser.add_argument("--ue-rates", default="1.0")
    parser.add_argument("--spacings", default="500")
    parser.add_argument("--uncertainty-scales", default="1.0")
    parser.add_argument("--adaptive-min-uncertainty-scales", default="0.5")
    parser.add_argument("--adaptive-max-uncertainty-scales", default="2.5")
    parser.add_argument("--adaptive-load-shock-gains", default="1.5")
    parser.add_argument("--adaptive-utilization-gains", default="1.0")
    parser.add_argument("--adaptive-relaxations", default="0.25")
    parser.add_argument("--adaptive-latent-load-thresholds", default="4.0")
    parser.add_argument("--adaptive-wake-relief-thresholds", default="0.08")
    parser.add_argument("--forecast-lead-times", default="0.0s")
    parser.add_argument("--traffic-profiles", default="steady,center-burst")
    parser.add_argument("--burst-rate-multipliers", default="3.0")
    parser.add_argument("--shift-start", default="3.0s")
    parser.add_argument("--shift-stop", default="10.0s")
    parser.add_argument("--sim-time", default="12.0s")
    parser.add_argument("--output-dir", default="")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument(
        "--keep-going",
        action="store_true",
        help="Record failed runs and continue the sweep",
    )
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
    adaptive_min_uncertainty_scales = split_csv(args.adaptive_min_uncertainty_scales, float)
    adaptive_max_uncertainty_scales = split_csv(args.adaptive_max_uncertainty_scales, float)
    adaptive_load_shock_gains = split_csv(args.adaptive_load_shock_gains, float)
    adaptive_utilization_gains = split_csv(args.adaptive_utilization_gains, float)
    adaptive_relaxations = split_csv(args.adaptive_relaxations, float)
    adaptive_latent_load_thresholds = split_csv(args.adaptive_latent_load_thresholds, float)
    adaptive_wake_relief_thresholds = split_csv(args.adaptive_wake_relief_thresholds, float)
    forecast_lead_times = split_csv(args.forecast_lead_times)
    traffic_profiles = split_csv(args.traffic_profiles)
    burst_rate_multipliers = split_csv(args.burst_rate_multipliers, float)

    if not args.skip_build:
        return_code = run_command(["./ns3", "build", "uno-umb-dt-energy"], repo)
        if return_code != 0:
            raise subprocess.CalledProcessError(return_code, ["./ns3", "build", "uno-umb-dt-energy"])

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
            adaptive_min_uncertainty_scales,
            adaptive_max_uncertainty_scales,
            adaptive_load_shock_gains,
            adaptive_utilization_gains,
            adaptive_relaxations,
            adaptive_latent_load_thresholds,
            adaptive_wake_relief_thresholds,
            forecast_lead_times,
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
        adaptive_min_uncertainty_scale,
        adaptive_max_uncertainty_scale,
        adaptive_load_shock_gain,
        adaptive_utilization_gain,
        adaptive_relaxation,
        adaptive_latent_load_threshold,
        adaptive_wake_relief_threshold,
        forecast_lead_time,
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
            adaptive_min_uncertainty_scale,
            adaptive_max_uncertainty_scale,
            adaptive_load_shock_gain,
            adaptive_utilization_gain,
            adaptive_relaxation,
            adaptive_latent_load_threshold,
            adaptive_wake_relief_threshold,
            forecast_lead_time,
            traffic_profile,
            effective_burst_rate_multiplier,
        )
        if scenario_key in seen_scenarios:
            continue
        seen_scenarios.add(scenario_key)

        run_id = (
            f"{index:04d}-{policy}-seed{seed}-run{run}-ues{ues}-rate{ue_rate}"
            f"-spacing{spacing}-unc{uncertainty_scale}-{traffic_profile}"
            f"-burst{effective_burst_rate_multiplier}-amin{adaptive_min_uncertainty_scale}"
            f"-amax{adaptive_max_uncertainty_scale}"
            f"-ashock{adaptive_load_shock_gain}-autil{adaptive_utilization_gain}"
            f"-arelax{adaptive_relaxation}"
            f"-alatent{adaptive_latent_load_threshold}-arelief{adaptive_wake_relief_threshold}"
            f"-flead{forecast_lead_time}"
        ).replace(".", "p")
        summary_csv = out_dir / f"{run_id}-summary.csv"
        event_csv = out_dir / f"{run_id}-events.csv"
        run_log = out_dir / f"{run_id}.log"
        program = (
            "uno-umb-dt-energy "
            f"--policy={policy} "
            f"--seed={seed} "
            f"--run={run} "
            f"--numberOfUes={ues} "
            f"--ueRateMbps={ue_rate} "
            f"--enbSpacingMeters={spacing} "
            f"--uncertaintyScale={uncertainty_scale} "
            f"--adaptiveMinUncertaintyScale={adaptive_min_uncertainty_scale} "
            f"--adaptiveMaxUncertaintyScale={adaptive_max_uncertainty_scale} "
            f"--adaptiveLoadShockGain={adaptive_load_shock_gain} "
            f"--adaptiveUtilizationGain={adaptive_utilization_gain} "
            f"--adaptiveRelaxation={adaptive_relaxation} "
            f"--adaptiveLatentLoadThreshold={adaptive_latent_load_threshold} "
            f"--adaptiveWakeReliefThreshold={adaptive_wake_relief_threshold} "
            f"--forecastLeadTime={forecast_lead_time} "
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
                "adaptive_min_uncertainty_scale": adaptive_min_uncertainty_scale,
                "adaptive_max_uncertainty_scale": adaptive_max_uncertainty_scale,
                "adaptive_load_shock_gain": adaptive_load_shock_gain,
                "adaptive_utilization_gain": adaptive_utilization_gain,
                "adaptive_relaxation": adaptive_relaxation,
                "adaptive_latent_load_threshold": adaptive_latent_load_threshold,
                "adaptive_wake_relief_threshold": adaptive_wake_relief_threshold,
                "forecast_lead_time": forecast_lead_time,
                "traffic_profile": traffic_profile,
                "burst_rate_multiplier": effective_burst_rate_multiplier,
                "command": ["./ns3", "run", program],
                "summary_csv": str(summary_csv),
                "event_csv": str(event_csv),
                "run_log": str(run_log),
            }
        )
        return_code = run_command(["./ns3", "run", program], repo, run_log)
        if return_code != 0:
            manifest["runs"][-1]["run_status"] = "failed"
            manifest["runs"][-1]["return_code"] = return_code
            if not args.keep_going:
                raise subprocess.CalledProcessError(return_code, ["./ns3", "run", program])
            print(f"Run failed with return code {return_code}; continuing.", flush=True)
            aggregate_rows.append(
                make_failure_row(
                    policy=policy,
                    seed=seed,
                    run=run,
                    ues=ues,
                    ue_rate=ue_rate,
                    spacing=spacing,
                    uncertainty_scale=uncertainty_scale,
                    adaptive_min_uncertainty_scale=adaptive_min_uncertainty_scale,
                    adaptive_max_uncertainty_scale=adaptive_max_uncertainty_scale,
                    adaptive_load_shock_gain=adaptive_load_shock_gain,
                    adaptive_utilization_gain=adaptive_utilization_gain,
                    adaptive_relaxation=adaptive_relaxation,
                    adaptive_latent_load_threshold=adaptive_latent_load_threshold,
                    adaptive_wake_relief_threshold=adaptive_wake_relief_threshold,
                    forecast_lead_time=forecast_lead_time,
                    traffic_profile=traffic_profile,
                    burst_rate_multiplier=effective_burst_rate_multiplier,
                    shift_start=args.shift_start,
                    shift_stop=args.shift_stop,
                    sim_time=args.sim_time,
                    run_id=run_id,
                    summary_csv=summary_csv,
                    event_csv=event_csv,
                    run_log=run_log,
                    return_code=return_code,
                )
            )
            write_aggregate_csv(aggregate_rows, out_dir / "aggregate.csv")
            continue
        manifest["runs"][-1]["run_status"] = "ok"
        manifest["runs"][-1]["return_code"] = 0

        with summary_csv.open(newline="") as handle:
            row = next(csv.DictReader(handle))
        row.update(
            {
                "run_id": run_id,
                "ue_rate_mbps": ue_rate,
                "enb_spacing_m": spacing,
                "uncertainty_scale": uncertainty_scale,
                "adaptive_min_uncertainty_scale": adaptive_min_uncertainty_scale,
                "adaptive_max_uncertainty_scale": adaptive_max_uncertainty_scale,
                "adaptive_load_shock_gain": adaptive_load_shock_gain,
                "adaptive_utilization_gain": adaptive_utilization_gain,
                "adaptive_relaxation": adaptive_relaxation,
                "adaptive_latent_load_threshold": adaptive_latent_load_threshold,
                "adaptive_wake_relief_threshold": adaptive_wake_relief_threshold,
                "forecast_lead_time_s": as_seconds(forecast_lead_time),
                "traffic_profile": traffic_profile,
                "burst_rate_multiplier": effective_burst_rate_multiplier,
                "shift_start": args.shift_start,
                "shift_stop": args.shift_stop,
                "summary_csv": str(summary_csv),
                "event_csv": str(event_csv),
                "run_status": "ok",
                "return_code": 0,
                "failure_reason": "",
                "run_log": str(run_log),
            }
        )
        aggregate_rows.append(row)

        write_aggregate_csv(aggregate_rows, out_dir / "aggregate.csv")

    manifest["completed_utc"] = datetime.now(timezone.utc).isoformat()
    manifest["aggregate_csv"] = str(out_dir / "aggregate.csv")
    with (out_dir / "manifest.json").open("w") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")

    print(f"Wrote {out_dir / 'aggregate.csv'}")
    print(f"Wrote {out_dir / 'manifest.json'}")


if __name__ == "__main__":
    main()
