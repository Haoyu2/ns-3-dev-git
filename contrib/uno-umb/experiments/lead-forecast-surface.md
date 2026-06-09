# Lead-Time Forecast Surface

This note records a compact right-edge surface over forecast lead time and
forecasted burst magnitude.

## Question

For transition-sensitive right-edge bursts, what combination of lead time and
forecast magnitude is needed to avoid controller-induced SLA violations?

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Forecast lead times: `0.0s`, `0.5s`, `1.0s`, `1.5s`
- Forecast burst multipliers: `1.25`, `1.5`
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

All three all-on rows are feasible for this workload.

## Commands

Lead-time and forecast-magnitude surface:

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
  --forecast-lead-times=0.0s,0.5s,1.0s,1.5s \
  --forecast-burst-rate-multipliers=1.25,1.5 \
  --output-dir=/tmp/uno-umb-lead-forecast-surface \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-lead-forecast-surface/aggregate.csv \
  --output-dir=/tmp/uno-umb-lead-forecast-surface-analysis
```

## Feasible-Only Result

| lead time | forecast burst multiplier | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- |
| 0.0s | 1.25 | 0.667 | 15.947 | 23.920 | 0.333 |
| 0.0s | 1.50 | 0.667 | 4.784 | 7.176 | 0.333 |
| 0.5s | 1.25 | 0.667 | 15.947 | 23.920 | 0.333 |
| 0.5s | 1.50 | 0.333 | 1.993 | 5.980 | 0.667 |
| 1.0s | 1.25 | 0.667 | 15.947 | 23.920 | 0.333 |
| 1.0s | 1.50 | 1.000 | 11.163 | 11.163 | 0.000 |
| 1.5s | 1.25 | 0.667 | 15.947 | 23.920 | 0.333 |
| 1.5s | 1.50 | 1.000 | 11.163 | 11.163 | 0.000 |

The safe region in this slice starts at the accurate forecast magnitude and at
least `1.0s` of lead time.  Underprediction at `1.25` fails on the hard run
for every lead time tested.  Accurate forecasts with `0.0s` lead wake at the
burst instant and still fail on the hard run.  Accurate forecasts with `0.5s`
lead are worse in aggregate because two runs violate the SLA.

## Hard-Run Boundary

Run `2` is the transition-sensitive placement.  Its results isolate the
boundary:

| lead time | forecast burst multiplier | action before burst | SLA violation | energy saving (%) |
| --- | --- | --- | --- | --- |
| 0.0s | 1.25 | sleep at `2.0s`, keep-sleep at `3.0s` | yes | 23.920 |
| 0.0s | 1.50 | sleep at `2.0s`, wake at `3.0s` | yes | 7.176 |
| 0.5s | 1.25 | sleep at `2.0s`, keep-sleep at `2.5s` | yes | 23.920 |
| 0.5s | 1.50 | sleep at `2.0s`, wake at `2.5s` | yes | 5.980 |
| 1.0s | 1.25 | sleep at `2.0s` | yes | 23.920 |
| 1.0s | 1.50 | keep-active at `2.0s` | no | 4.784 |
| 1.5s | 1.25 | sleep at `2.0s` | yes | 23.920 |
| 1.5s | 1.50 | keep-active at `2.0s` | no | 4.784 |

This supports a two-condition interpretation: the forecasted magnitude must be
large enough to make sleeping the cell unsafe under the twin estimate, and the
resulting active state must be established early enough for LTE handover and
queue state to settle before the burst traffic arrives.

## Interpretation

The forecast path should not be presented as a generic benefit from looking
ahead.  The surface shows three regimes:

- Underpredicted demand preserves energy but leaves the hard-run SLA violation.
- Too-short accurate lead can create transition churn and worsen safety.
- Accurate forecasts with at least `1.0s` lead remove the right-edge violation
  in all three replicated runs while retaining `11.163%` mean energy saving.

This is a stronger paper contribution than the fixed one-second calibration:
it identifies the joint safety boundary and gives a concrete next target for
forecast-error-aware control.
