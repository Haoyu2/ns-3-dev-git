# Forecast-Error Calibration

This note records a right-edge forecast-error calibration for the forecast
lead-time controller path.

## Question

How much traffic-shift magnitude must be predicted for a one-second forecast
lead to prevent the right-edge transition failure, and how much energy is spent
when the forecast is conservative?

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Forecast lead time: `1.0s`
- Forecast burst multipliers: `1.0`, `1.25`, `1.5`, `1.75`
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

All three all-on rows for this workload are feasible, so SLA violations below
are controller-induced.

## Commands

Forecast-error sweep:

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
  --forecast-burst-rate-multipliers=1.0,1.25,1.5,1.75 \
  --output-dir=/tmp/uno-umb-forecast-error-right-edge \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-forecast-error-right-edge/aggregate.csv \
  --output-dir=/tmp/uno-umb-forecast-error-analysis
```

## Feasible-Only Result

| forecast burst multiplier | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- |
| 1.00 | 0.667 | 15.947 | 23.920 | 0.333 |
| 1.25 | 0.667 | 15.947 | 23.920 | 0.333 |
| 1.50 | 1.000 | 11.163 | 11.163 | 0.000 |
| 1.75 | 1.000 | 4.784 | 4.784 | 0.000 |

The calibrated boundary is sharp in this slice.  A one-second lead is not
enough if the controller underpredicts the burst magnitude.  Forecasts at the
actual burst magnitude remove the violation, while overprediction remains safe
but can spend additional energy.

## Per-Run Detail

| run | forecast burst multiplier | SLA violation | loss ratio | mean delay (ms) | energy saving (%) |
| --- | --- | --- | --- | --- | --- |
| 1 | 1.00 | no | 0.162716 | 47.297 | 23.920 |
| 1 | 1.25 | no | 0.162716 | 47.297 | 23.920 |
| 1 | 1.50 | no | 0.148389 | 47.601 | 4.784 |
| 1 | 1.75 | no | 0.148389 | 47.601 | 4.784 |
| 2 | 1.00 | yes | 0.190783 | 47.016 | 23.920 |
| 2 | 1.25 | yes | 0.190783 | 47.016 | 23.920 |
| 2 | 1.50 | no | 0.171517 | 53.796 | 4.784 |
| 2 | 1.75 | no | 0.171517 | 53.796 | 4.784 |
| 3 | 1.00 | no | 0.133408 | 39.679 | 23.920 |
| 3 | 1.25 | no | 0.133408 | 39.679 | 23.920 |
| 3 | 1.50 | no | 0.133408 | 39.679 | 23.920 |
| 3 | 1.75 | no | 0.105379 | 39.215 | 4.784 |

## Trace Evidence

For run `2`, the event log shows why underprediction fails:

| forecast burst multiplier | time (s) | cell | action | reason | load (Mbps) | twin safe |
| --- | --- | --- | --- | --- | --- | --- |
| 1.00 | 2.000000 | 1 | sleep | policy-selected-sleep | 2.000000 | 1 |
| 1.25 | 2.000000 | 1 | sleep | policy-selected-sleep | 2.000000 | 1 |
| 1.50 | 2.000000 | 1 | keep-active | no-state-change | 2.000000 | 0 |
| 1.75 | 2.000000 | 1 | keep-active | no-state-change | 2.000000 | 0 |

The forecast does not change the actual traffic, but it changes the controller's
load estimate at `2.0s`.  With forecasts `1.0` and `1.25`, the risk gate still
allows the low-load center cell to sleep.  With forecasts `1.5` and `1.75`, the
same sleep action is no longer safe under the twin estimate, so the controller
keeps the center cell active before the right-edge burst arrives.

## Interpretation

The result adds an important limitation and contribution path:

- Forecast lead time needs a sufficiently accurate magnitude estimate to help
  hard edge transitions.
- Underprediction preserves energy but can leave the same controller-induced
  failure as zero-lead control.
- Overprediction is safe in this slice but can reduce savings from `11.163%`
  to `4.784%` on average.

The follow-on lead-time/forecast-magnitude surface is recorded in
`lead-forecast-surface.md`.  The relative forecast-error sweep is recorded in
`forecast-magnitude-robustness.md`.
