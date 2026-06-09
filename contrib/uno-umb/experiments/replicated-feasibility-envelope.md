# Replicated Feasibility Envelope

This note records a replicated burst-profile envelope after adding guarded
event-triggered latent-demand wakeup, then tests a small forecast lead-time hook
on the remaining right-edge transition failure.

## Question

Which burst regimes are valid controller benchmarks, and where does the current
controller need either reactive latent wakeup or forecast lead time to avoid
controller-induced SLA violations?

## Scenario

- Policies: `all-on`, `twin`, `adaptive-twin`
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

## Commands

All-on envelope:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst,edge-burst,right-edge-burst,global-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-replicated-envelope-all-on \
  --skip-build \
  --keep-going
```

Center-burst controllers:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-replicated-envelope-controllers-center \
  --skip-build \
  --keep-going
```

Edge-burst and right-edge-burst controllers:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=edge-burst,right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-replicated-envelope-controllers-edge \
  --skip-build \
  --keep-going
```

Right-edge forecast lead-time ablation:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
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
  --output-dir=/tmp/uno-umb-forecast-right-edge-3run \
  --skip-build \
  --keep-going
```

Combined analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-replicated-envelope-controllers-center/aggregate.csv \
  /tmp/uno-umb-replicated-envelope-controllers-edge/aggregate.csv \
  /tmp/uno-umb-forecast-right-edge-3run/aggregate.csv \
  --output-dir=/tmp/uno-umb-replicated-envelope-analysis-with-forecast
```

## All-On Feasibility

All-on feasibility defines which rows are valid controller benchmarks.  A row
that violates the SLA with every cell active is treated as overload, not as a
controller failure.

| profile | multiplier | all-on feasible runs |
| --- | --- | --- |
| center-burst | 1.5 | 3/3 |
| center-burst | 2.0 | 3/3 |
| center-burst | 3.0 | 2/3 |
| edge-burst | 1.5 | 2/3 |
| edge-burst | 2.0 | 0/3 |
| edge-burst | 3.0 | 0/3 |
| right-edge-burst | 1.5 | 3/3 |
| right-edge-burst | 2.0 | 0/3 |
| right-edge-burst | 3.0 | 0/3 |
| global-burst | 1.5 | 0/3 |
| global-burst | 2.0 | 0/3 |
| global-burst | 3.0 | 0/3 |

This sharply narrows the near-term benchmark envelope: center bursts up to
`3.0x`, edge bursts at `1.5x`, and right-edge bursts at `1.5x` are useful
controller tests; heavier edge/global bursts are overload tests.

## Feasible-Only Controller Results

| profile | multiplier | policy | forecast lead | feasible rows | safe-run rate | safe energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| center-burst | 1.5 | twin | 0.0s | 3 | 1.000 | 23.920 | 0.000 |
| center-burst | 1.5 | adaptive-twin | 0.0s | 3 | 1.000 | 23.920 | 0.000 |
| center-burst | 2.0 | twin | 0.0s | 3 | 0.667 | 15.947 | 0.333 |
| center-burst | 2.0 | adaptive-twin | 0.0s | 3 | 1.000 | 7.176 | 0.000 |
| center-burst | 3.0 | twin | 0.0s | 2 | 0.500 | 3.588 | 0.500 |
| center-burst | 3.0 | adaptive-twin | 0.0s | 2 | 1.000 | 7.176 | 0.000 |
| edge-burst | 1.5 | twin | 0.0s | 2 | 1.000 | 23.920 | 0.000 |
| edge-burst | 1.5 | adaptive-twin | 0.0s | 2 | 1.000 | 15.548 | 0.000 |
| right-edge-burst | 1.5 | twin | 0.0s | 3 | 0.667 | 10.365 | 0.333 |
| right-edge-burst | 1.5 | adaptive-twin | 0.0s | 3 | 0.667 | 4.784 | 0.333 |
| right-edge-burst | 1.5 | twin | 1.0s | 3 | 1.000 | 11.163 | 0.000 |
| right-edge-burst | 1.5 | adaptive-twin | 1.0s | 3 | 1.000 | 11.163 | 0.000 |

The center-burst rows support the guarded latent-demand wakeup contribution:
adaptive-twin removes static-twin controller-induced violations at `2.0x` and
`3.0x` by waking the latent preferred cell when the hidden demand becomes large
enough.

The right-edge rows expose a different failure mode.  With zero forecast lead,
both twin variants wake at the burst instant but still violate one feasible
run.  With a `1.0s` forecast lead, both variants are safe in all three feasible
right-edge runs.  The result suggests that transition-sensitive edge bursts need
handover and queue-settling lead time, not only instantaneous latent-demand
detection.

## Trace Evidence

Zero-lead adaptive-twin on `right-edge-burst`, run `2`, wakes the sleeping cell
at `3.000000s` with `2.000000 Mb/s` latent load and `0.111111` peak-utilization
relief, but the run still violates the SLA:

| forecast lead | controller shift start | loss ratio | mean delay (ms) | energy saving (%) | SLA violation |
| --- | --- | --- | --- | --- | --- |
| 0.0s | 3.0s | 0.194276 | 51.386 | 7.176 | yes |
| 1.0s | 2.0s | 0.171517 | 53.796 | 4.784 | no |

The forecasted run updates the controller-side demand estimate at `2.0s`, while
the actual burst traffic still starts at `3.0s`.  The offered-load accounting is
unchanged; only the controller is allowed to act on the predicted shift early.

## Interpretation

The strongest current claim is not a single threshold improvement.  It is a
two-timescale safety controller:

- Reactive latent-demand wakeup handles unannounced center-cell demand changes.
- Forecast lead time handles transition-sensitive edge bursts where waking at
  the exact burst instant is too late for handover and queue settling.
- The all-on feasibility envelope prevents overload rows from being counted as
  controller failures.

The next calibration should sweep forecast lead time, imperfect forecasts, and
mobility.  The paper contribution will be stronger if it reports both benefits
and forecast-dependence rather than hiding the right-edge negative case.
