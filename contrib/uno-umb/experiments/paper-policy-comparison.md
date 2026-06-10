# Paper Policy Comparison

This note consolidates the replicated feasibility rows into a compact table for
paper framing.  It keeps the all-on reference visible, separates forecast
controls from policy names, and evaluates controllers only on rows where all
cells active can satisfy the SLA.

## Question

Which controller contribution explains each safe energy-saving region?

## Scenario Set

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

Controller rows are feasible-only: rows where all-on violates the SLA are
excluded from controller rates.  The all-on rows report the feasibility rate of
the offered-load scenario.

## Commands

The all-on reference and no-forecast controller rows reuse the replicated
feasibility-envelope commands from `replicated-feasibility-envelope.md`.

The right-edge perfect-lead rows reuse:

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

The fair underforecast-plus-margin policy rows are:

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
  --min-forecast-lead-times=1.0s \
  --forecast-burst-rate-multipliers=1.5 \
  --forecast-burst-rate-errors=-0.25 \
  --forecast-burst-rate-uncertainties=0.25 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-paper-forecast-margin-policy \
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
  /tmp/uno-umb-paper-forecast-margin-policy/aggregate.csv \
  --output-dir=/tmp/uno-umb-paper-policy-comparison-balanced
```

The correction-latency rows are intentionally kept out of the main table to
avoid double-counting the no-correction margin case.  They are recorded
separately in `forecast-correction-latency-surface.md`.

## Compact Result

| scenario | control | policy | runs | safe/feasible rate | safe energy saving (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| center 1.5x | reference | all-on | 3 | 1.000 | 0.000 | 0.000 | 0.000 |
| center 1.5x | no forecast | twin | 3 | 1.000 | 23.920 | 0.000 | 0.000 |
| center 1.5x | no forecast | adaptive-twin | 3 | 1.000 | 23.920 | 0.000 | 0.000 |
| center 2x | reference | all-on | 3 | 1.000 | 0.000 | 0.000 | 0.000 |
| center 2x | no forecast | twin | 3 | 0.667 | 15.947 | 0.333 | 0.000 |
| center 2x | no forecast | adaptive-twin | 3 | 1.000 | 7.176 | 0.000 | 0.000 |
| center 3x | reference | all-on | 3 | 0.667 | 0.000 | 0.000 | 0.000 |
| center 3x | no forecast | twin | 2 | 0.500 | 3.588 | 0.500 | 0.000 |
| center 3x | no forecast | adaptive-twin | 2 | 1.000 | 7.176 | 0.000 | 0.000 |
| right-edge 1.5x | reference | all-on | 3 | 1.000 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | twin | 3 | 0.667 | 10.365 | 0.333 | 0.000 |
| right-edge 1.5x | no forecast | adaptive-twin | 3 | 0.667 | 4.784 | 0.333 | 0.000 |
| right-edge 1.5x | perfect 1s lead | twin | 3 | 1.000 | 11.163 | 0.000 | 0.000 |
| right-edge 1.5x | perfect 1s lead | adaptive-twin | 3 | 1.000 | 11.163 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | twin | 3 | 1.000 | 11.163 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | adaptive-twin | 3 | 1.000 | 11.163 | 0.000 | 0.000 |

## Interpretation

The table separates two mechanisms:

- Reactive latent-demand wakeup is the center-burst contribution.  Static
  `twin` is unsafe at `2x` and `3x`; `adaptive-twin` restores all feasible rows
  to safe operation by spending energy earlier.
- Forecasted transition preparation is the right-edge contribution.  With no
  forecast, both `twin` and `adaptive-twin` fail one hard feasible placement.
  A `1.0s` lead removes the failure.  When the forecast underpredicts the burst
  by `25%`, the `0.25` uncertainty margin still recovers the same safe result.

The underforecast-plus-margin rows are especially useful for the paper claim:
the robust forecast gate works for both `twin` and `adaptive-twin`, so it is not
only an adaptive-threshold side effect.  The adaptive controller contributes the
reactive latent-demand protection; the forecast margin contributes the
transition-sensitive protection.

## Paper Claim Draft

In feasible burst regimes, UNO-UMB decomposes QoS-safe cell sleep into two
control surfaces.  Online latent-demand wakeup removes controller-induced
violations for unannounced center-cell load shifts, while a lead-time forecast
gate with an explicit uncertainty margin removes transition failures for
right-edge shifts, even under a `25%` underpredicted burst.  The controller
trades peak energy saving for safety: the rows that recover every feasible
placement retain `7.176%` to `11.163%` safe energy saving, while the easier
center `1.5x` case reaches `23.920%`.

## Next Step

The first seed/spacing expansion is recorded in
`paper-spacing-sensitivity.md`.
