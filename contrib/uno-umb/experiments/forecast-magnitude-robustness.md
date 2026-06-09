# Forecast-Magnitude Robustness

This note records a relative forecast-error sweep around the gated right-edge
transition.

## Question

How much forecasted excess-load error can the controller tolerate when the
forecast lead is long enough to be actionable?

## Mechanism

The example exposes ``forecastBurstRateError``.  The value perturbs only the
controller-side forecasted burst excess load:

```text
forecasted multiplier = 1 + (base forecast multiplier - 1) * (1 + error)
```

The application traffic still uses ``burstRateMultiplier``.  This isolates the
effect of controller forecast error from the physical offered load.

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Base forecast burst multiplier: `1.5`
- Forecast burst-rate errors: `-0.5`, `-0.25`, `0.0`, `0.25`, `0.5`
- Forecast lead time: `1.0s`
- Minimum actionable forecast lead: `1.0s`
- Seed: `1`
- Runs: `1,2,3`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacing: `500 m`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `4.0`
- Adaptive wake-relief threshold: `0.08`
- Handover guard: `1.25s`

All three all-on rows for this workload are feasible.

## Commands

Robustness sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
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
  --forecast-burst-rate-errors=-0.5,-0.25,0.0,0.25,0.5 \
  --output-dir=/tmp/uno-umb-forecast-robustness \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-forecast-robustness/aggregate.csv \
  --output-dir=/tmp/uno-umb-forecast-robustness-analysis
```

## Feasible-Only Result

| forecast error | effective forecast multiplier | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- |
| -0.50 | 1.250 | 0.667 | 15.947 | 23.920 | 0.333 |
| -0.25 | 1.375 | 0.667 | 9.568 | 12.757 | 0.333 |
| 0.00 | 1.500 | 1.000 | 11.163 | 11.163 | 0.000 |
| 0.25 | 1.625 | 1.000 | 4.784 | 4.784 | 0.000 |
| 0.50 | 1.750 | 1.000 | 4.784 | 4.784 | 0.000 |

The one-second gated forecast is robust to accurate and conservative
overestimates in this slice.  Underestimating the excess burst load by `25%` or
more leaves one of the three replicated runs unsafe, so the safe region is not
only a timing property; it also depends on forecast magnitude.

## Hard-Run Evidence

Run `2` is the hard right-edge placement.  At `2.0s`, before the real burst
starts, the forecasted load determines whether the risk gate keeps the center
cell active.

| forecast error | effective forecast multiplier | cell 1 action at 2.0s | twin safe | handovers | SLA violation |
| --- | --- | --- | --- | --- | --- |
| -0.50 | 1.250 | sleep | yes | 2 | yes |
| -0.25 | 1.375 | sleep | yes | 6 | yes |
| 0.00 | 1.500 | keep-active | no | 2 | no |
| 0.25 | 1.625 | keep-active | no | 2 | no |
| 0.50 | 1.750 | keep-active | no | 2 | no |

The `-0.25` row is especially useful: the forecast is close to the actual burst
but still causes transition churn and an SLA violation in the hard placement.

## Interpretation

This adds a stronger robustness contribution than the fixed-multiplier
calibration alone:

- Forecast lead gating prevents too-short forecasts from causing extra churn.
- Relative magnitude error shows when an otherwise actionable forecast remains
  too weak for the risk gate.
- Conservative forecasts protect the hard transition but may spend more energy,
  exposing an explicit safety-energy tradeoff.

The follow-on forecast-error uncertainty margin is recorded in
`forecast-uncertainty-margin.md`.
