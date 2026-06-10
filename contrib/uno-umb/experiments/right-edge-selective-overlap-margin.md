# Right-Edge Selective Overlap Margin

This note records the first selective forecast-margin design after the
calibrated `0.35` margin surface.  The goal is to keep the fully safe behavior
of the calibrated margin on the hard `475 m` right-edge placement without
forcing the same conservative sleep pattern on easier `500/525 m` placements.

## Question

Can the controller use `0.25` forecast-error uncertainty by default, escalate
to `0.35` only on tight overlap-risk decisions, and preserve the higher
energy-saving behavior on wider safe placements?

## Mechanism

The controller now supports two active forecast utilization margins:

- Base margin: `forecastBurstRateUncertainty=0.25`
- Selective margin: `selectiveForecastBurstRateUncertainty=0.35`

The selective margin is used only when both conditions hold:

- The candidate sleep decision has small nonnegative utilization slack under
  the base margin.
- The estimated farthest offloaded UE is within a configured maximum offload
  distance, which acts as a simple overlap/interference-risk gate.

The sweep and analysis tools record the new knobs as:

- `selective_forecast_burst_rate_uncertainty`
- `selective_forecast_utilization_margin`
- `forecast_margin_trigger_slack`
- `forecast_margin_trigger_max_offload_m`

Run IDs were shortened and given a deterministic scenario hash so longer
selective-control sweeps do not hit filesystem filename limits.

## Calibration Evidence

The original hard sample has only `0.018716` utilization slack under the
`0.25` margin.  A slack-only trigger therefore flips at about `0.019`:

| policy | slack trigger | loss ratio | energy saving (%) | active cell-s | SLA violation |
| --- | --- | --- | --- | --- | --- |
| twin | 0.018 | 0.177651 | 23.920 | 26.000 | 1 |
| twin | 0.019 | 0.152998 | 4.784 | 34.000 | 0 |
| twin | 0.020 | 0.152998 | 4.784 | 34.000 | 0 |
| adaptive-twin | 0.018 | 0.177651 | 23.920 | 26.000 | 1 |
| adaptive-twin | 0.019 | 0.152998 | 4.784 | 34.000 | 0 |
| adaptive-twin | 0.020 | 0.152998 | 4.784 | 34.000 | 0 |

However, utilization slack alone cannot separate the hard `475 m` row from
safe wider rows.  For seed `2`, run `2`, all three spacings sleep the same
candidate under the base `0.25` margin, and wider spacings have even smaller
utilization slack:

| spacing | base loss ratio | base energy saving (%) | base SLA violation | utilization slack | offload distance proxy (m) |
| --- | --- | --- | --- | --- | --- |
| 475 m | 0.177651 | 23.920 | 1 | 0.018716 | 389.455 |
| 500 m | 0.162996 | 23.920 | 0 | 0.017218 | 414.411 |
| 525 m | 0.141176 | 23.920 | 0 | 0.015721 | 439.373 |

This motivates the distance gate: the problematic case is the close-overlap
offload, not merely a tight utilization estimate.

## Probe Command

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=2 \
  --runs=2 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=475,500,525 \
  --traffic-profiles=right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --forecast-lead-times=1.0s \
  --min-forecast-lead-times=1.0s \
  --forecast-burst-rate-multipliers=1.5 \
  --forecast-burst-rate-errors=-0.25 \
  --forecast-burst-rate-uncertainties=0.25 \
  --selective-forecast-burst-rate-uncertainties=0.35 \
  --forecast-margin-trigger-slacks=0.019 \
  --forecast-margin-trigger-max-offloads=400 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-selective-overlap-probe \
  --skip-build \
  --keep-going
```

## Probe Result

| policy | spacing | loss ratio | energy saving (%) | active cell-s | SLA violation |
| --- | --- | --- | --- | --- | --- |
| twin | 475 m | 0.152998 | 4.784 | 34.000 | 0 |
| twin | 500 m | 0.162996 | 23.920 | 26.000 | 0 |
| twin | 525 m | 0.141176 | 23.920 | 26.000 | 0 |
| adaptive-twin | 475 m | 0.152998 | 4.784 | 34.000 | 0 |
| adaptive-twin | 500 m | 0.162996 | 23.920 | 26.000 | 0 |
| adaptive-twin | 525 m | 0.141176 | 23.920 | 26.000 | 0 |

The overlap-gated selective margin recovers the hard `475 m` row for both
controllers while preserving the high-saving `0.25` behavior at `500 m` and
`525 m` for the same seed/run.

## Interpretation

This is a stronger design direction than a global calibrated margin.  The
global `0.35` margin is safe but collapses every feasible row on the calibrated
surface to `4.784%` saving.  The selective overlap rule keeps the same
conservative action only where the close-overlap offload indicates the
right-edge interference tail, while allowing wider placements to keep the
aggressive-but-safe sleep action.

The full replicated surface is recorded in
`right-edge-selective-overlap-surface.md`.  The same selective trigger reaches
`11/11` safe feasible rows and raises mean safe energy saving from the global
`0.35` margin's `4.784%` to `13.482%`.
