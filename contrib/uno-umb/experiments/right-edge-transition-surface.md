# Right-Edge Transition Surface

This note extends the `500 m` right-edge boundary check to a small spacing
surface around the transition region.

## Question

Does forecast lead and forecast-error margin remove right-edge controller
failures across nearby spacings, or only at the original `500 m` placement?

## Scenario Set

- Policies: `all-on`, `twin`, `adaptive-twin`
- Seeds: `1,2`
- Runs: `1,2`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `475 m`, `500 m`, `525 m`
- Traffic profile: `right-edge-burst`
- Burst multiplier: `1.5`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `4.0`
- Adaptive wake-relief threshold: `0.08`
- Forecast cases:
  - no forecast
  - perfect `1.0s` forecast lead
  - `1.0s` forecast lead with `-25%` burst-rate error and `0.25` uncertainty
    margin

Controller rows are interpreted only when the matching all-on row with the same
seed, run, and spacing is SLA-feasible.

## Commands

All-on feasibility surface:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1,2 \
  --runs=1,2 \
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
  --output-dir=/tmp/uno-umb-transition-surface-all-on \
  --skip-build \
  --keep-going
```

No-forecast controllers:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1,2 \
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
  --output-dir=/tmp/uno-umb-transition-surface-no-forecast \
  --skip-build \
  --keep-going
```

Perfect forecast lead:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1,2 \
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
  --output-dir=/tmp/uno-umb-transition-surface-perfect-lead \
  --skip-build \
  --keep-going
```

Underforecast plus margin:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1,2 \
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
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-transition-surface-margin \
  --skip-build \
  --keep-going
```

Compact table:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-transition-surface-all-on/aggregate.csv \
  /tmp/uno-umb-transition-surface-no-forecast/aggregate.csv \
  /tmp/uno-umb-transition-surface-perfect-lead/aggregate.csv \
  /tmp/uno-umb-transition-surface-margin/aggregate.csv \
  --output-dir=/tmp/uno-umb-transition-surface-table
```

## Overall Result

| control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| reference | all-on | 12 | 0.917 | 0 | 0.917 | 0.000 | 0.000 |
| no forecast | twin | 12 | 0.917 | 11 | 0.818 | 13.482 | 0.182 |
| no forecast | adaptive-twin | 12 | 0.917 | 11 | 0.818 | 5.871 | 0.182 |
| perfect 1s lead | twin | 12 | 0.917 | 11 | 0.909 | 13.047 | 0.091 |
| perfect 1s lead | adaptive-twin | 12 | 0.917 | 11 | 0.909 | 9.568 | 0.091 |
| -25% forecast + 0.25 margin | twin | 12 | 0.917 | 11 | 0.909 | 13.047 | 0.091 |
| -25% forecast + 0.25 margin | adaptive-twin | 12 | 0.917 | 11 | 0.909 | 13.047 | 0.091 |

## Spacing Surface

| spacing | policy | control | all-on feasible | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 475 m | twin | no forecast | 3/4 | 3 | 0.667 | 10.365 | 0.333 |
| 475 m | twin | perfect 1s lead | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 475 m | twin | -25% forecast + 0.25 margin | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 475 m | adaptive-twin | no forecast | 3/4 | 3 | 0.667 | 4.784 | 0.333 |
| 475 m | adaptive-twin | perfect 1s lead | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 475 m | adaptive-twin | -25% forecast + 0.25 margin | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 500 m | twin | no forecast | 4/4 | 4 | 0.750 | 13.754 | 0.250 |
| 500 m | twin | perfect 1s lead | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 500 m | twin | -25% forecast + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 500 m | adaptive-twin | no forecast | 4/4 | 4 | 0.750 | 5.382 | 0.250 |
| 500 m | adaptive-twin | perfect 1s lead | 4/4 | 4 | 1.000 | 9.568 | 0.000 |
| 500 m | adaptive-twin | -25% forecast + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | twin | no forecast | 4/4 | 4 | 1.000 | 15.548 | 0.000 |
| 525 m | twin | perfect 1s lead | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | twin | -25% forecast + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | adaptive-twin | no forecast | 4/4 | 4 | 1.000 | 7.176 | 0.000 |
| 525 m | adaptive-twin | perfect 1s lead | 4/4 | 4 | 1.000 | 9.568 | 0.000 |
| 525 m | adaptive-twin | -25% forecast + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |

One `475 m` sample, seed `1` run `2`, is infeasible even with all cells on.
The no-forecast controller sweeps hit the LTE RLC scheduler assertion on that
same overloaded sample, but the failure does not enter feasible-controller
rates.

## Interpretation

The surface separates the transition into three regimes:

- `525 m` is safe for every tested controller and forecast mode.
- `500 m` is forecast-sensitive: all-on is feasible on 4/4 samples, reactive
  no-forecast control is safe on 3/4, and both perfect lead and
  underforecast-plus-margin are safe on 4/4.
- `475 m` is closer to the overload edge: all-on is feasible on only 3/4
  samples, and one feasible placement remains unsafe for both controllers even
  with perfect lead or forecast-error margin.

This improves the paper framing.  Forecast lead and forecast-error margin do
not make every lower-spacing row safe; they remove the measured
controller-induced tail at the `500 m` transition while revealing a stricter
spacing edge at `475 m`.  That makes the contribution more defensible: the
method expands the safe energy-saving region, and the feasible-envelope filter
identifies where the workload is too close to overload for this controller
family.

The adaptive controller does not rescue the hard right-edge rows by itself.  It
is more useful for the center-burst latent-demand case, while right-edge
protection is primarily a forecast-timing and margin effect.

## Next Step

The next targeted experiment should focus on the remaining `475 m` hard sample:
try a wider forecast-error uncertainty margin, for example `0.35` and `0.50`,
and compare it with a lower target energy-saving gate.  The goal is to learn
whether the `475 m` failure is recoverable by stronger risk padding or marks the
practical lower edge of the current controller.
