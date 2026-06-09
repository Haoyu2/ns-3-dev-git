# Forecast Lead Gate

This note records a minimum actionable forecast lead gate that avoids applying
traffic forecasts too close to the burst start.

## Question

Can the controller avoid the harmful `0.5s` forecast-lead regime while
preserving the safe region at `1.0s` and above?

## Mechanism

The example now exposes ``minForecastLeadTime``.  A forecast updates the
controller-side offered-load estimate early only when:

```text
forecastLeadTime > 0 and forecastLeadTime >= minForecastLeadTime
```

If the lead is shorter than the minimum, the controller receives the demand
change at the actual traffic-shift start.  The summary CSV records
``min_forecast_lead_time_s`` and ``forecast_lead_applied`` so gated and
ungated rows can be compared directly.

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Forecast burst multiplier: `1.5`
- Forecast lead times: `0.0s`, `0.5s`, `1.0s`, `1.5s`
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

All three all-on rows are feasible for this workload.

## Commands

Gated lead-time sweep:

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
  --min-forecast-lead-times=1.0s \
  --forecast-burst-rate-multipliers=1.5 \
  --output-dir=/tmp/uno-umb-gated-lead-surface \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-gated-lead-surface/aggregate.csv \
  --output-dir=/tmp/uno-umb-gated-lead-surface-analysis
```

## Feasible-Only Result

| lead time | lead applied | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- |
| 0.0s | no | 0.667 | 4.784 | 7.176 | 0.333 |
| 0.5s | no | 0.667 | 4.784 | 7.176 | 0.333 |
| 1.0s | yes | 1.000 | 11.163 | 11.163 | 0.000 |
| 1.5s | yes | 1.000 | 11.163 | 11.163 | 0.000 |

The gate removes the harmful half-second early action observed in the ungated
surface.  With the gate, `0.5s` behaves like zero lead instead of producing the
previous `0.333` safe-run rate.  Leads of `1.0s` and `1.5s` remain fully safe.

## Hard-Run Evidence

Run `2` is the hard right-edge placement:

| lead time | lead applied | action before burst | SLA violation | energy saving (%) |
| --- | --- | --- | --- | --- |
| 0.0s | no | sleep at `2.0s`, wake at `3.0s` | yes | 7.176 |
| 0.5s | no | sleep at `2.0s`, wake at `3.0s` | yes | 7.176 |
| 1.0s | yes | keep-active at `2.0s` | no | 4.784 |
| 1.5s | yes | keep-active at `2.0s` | no | 4.784 |

This confirms that the gate is a safety filter, not a universal fix.  It
prevents too-short forecast leads from causing extra transition churn, while
still requiring enough lead time and enough forecasted demand to protect the
hard edge transition.

## Interpretation

The forecast path now has three explicit pieces:

- Forecast magnitude decides whether the twin risk gate keeps a cell active.
- Forecast lead time decides whether there is enough time for LTE transition
  state to settle.
- ``minForecastLeadTime`` rejects forecast leads that are too short to use
  safely.

The next research step should replace the fixed minimum with an adaptive
settling-time estimate based on handover timing, observed packet loss, and
queue recovery.
