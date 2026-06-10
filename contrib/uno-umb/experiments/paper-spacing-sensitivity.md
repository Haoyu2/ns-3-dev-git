# Paper Spacing Sensitivity

This note takes the first step beyond the three-placement paper table by adding
two eNB spacings and two seeds for the center-burst and right-edge mechanisms.

## Question

Do the center-burst adaptive-wakeup and right-edge forecast-margin conclusions
survive modest seed and spacing variation?

## Scenario Set

- Policies: `all-on`, `twin`, `adaptive-twin`
- Seeds: `1,2`
- Run: `1`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `450 m`, `650 m`
- Main controller profiles:
  - `center-burst`, burst multiplier `2.0`
  - `right-edge-burst`, burst multiplier `1.5`
- Additional all-on feasibility boundaries:
  - `center-burst`, burst multiplier `1.5`
  - `right-edge-burst`, burst multiplier `2.0`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `4.0`
- Adaptive wake-relief threshold: `0.08`
- Handover guard: `1.25s`

Controller rows are still feasible-only: a controller result counts only when
the matching all-on row, with the same seed, run, spacing, and workload, does
not violate the SLA.

## Commands

All-on reference and boundary rows:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1,2 \
  --runs=1 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=450,650 \
  --traffic-profiles=center-burst,right-edge-burst \
  --burst-rate-multipliers=2.0,1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-paper-spacing-all-on \
  --skip-build \
  --keep-going
```

Center-burst controllers:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=450,650 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=2.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-paper-spacing-center-controllers \
  --skip-build \
  --keep-going
```

Right-edge no-forecast controllers:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=450,650 \
  --traffic-profiles=right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-paper-spacing-right-no-forecast \
  --skip-build \
  --keep-going
```

Right-edge underforecast plus margin:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2 \
  --runs=1 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=450,650 \
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
  --output-dir=/tmp/uno-umb-paper-spacing-right-margin \
  --skip-build \
  --keep-going
```

Standard analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-paper-spacing-all-on/aggregate.csv \
  /tmp/uno-umb-paper-spacing-center-controllers/aggregate.csv \
  /tmp/uno-umb-paper-spacing-right-no-forecast/aggregate.csv \
  /tmp/uno-umb-paper-spacing-right-margin/aggregate.csv \
  --output-dir=/tmp/uno-umb-paper-spacing-analysis
```

Compact paper table:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-paper-spacing-all-on/aggregate.csv \
  /tmp/uno-umb-paper-spacing-center-controllers/aggregate.csv \
  /tmp/uno-umb-paper-spacing-right-no-forecast/aggregate.csv \
  /tmp/uno-umb-paper-spacing-right-margin/aggregate.csv \
  --output-dir=/tmp/uno-umb-paper-spacing-table
```

## Compact Result

| scenario | control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | safe energy saving stdev (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| center 2x | reference | all-on | 4 | 0.750 | 0 | 0.750 | 0.000 | 0.000 | 0.000 | 0.000 |
| center 2x | no forecast | twin | 4 | 0.750 | 3 | 0.667 | 15.947 | 13.810 | 0.333 | 0.000 |
| center 2x | no forecast | adaptive-twin | 4 | 0.750 | 3 | 1.000 | 7.176 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | reference | all-on | 4 | 0.750 | 0 | 0.750 | 0.000 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | twin | 4 | 0.750 | 3 | 1.000 | 12.757 | 9.667 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | adaptive-twin | 4 | 0.750 | 3 | 1.000 | 7.176 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | twin | 4 | 0.750 | 3 | 1.000 | 11.163 | 11.048 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | adaptive-twin | 4 | 0.750 | 3 | 1.000 | 11.163 | 11.048 | 0.000 | 0.000 |
| right-edge 2x | reference | all-on | 4 | 0.250 | 0 | 0.250 | 0.000 | 0.000 | 0.000 | 0.000 |

One center-burst controller row, seed `2` at `450 m`, triggered the LTE RLC
debug assertion.  The matching all-on row was already infeasible, so this
failure is recorded but does not enter the feasible controller rates.

## Interpretation

This spacing slice changes the paper framing in a useful way:

- The center-burst adaptive-wakeup contribution survives the broader slice.
  Among feasible center `2x` rows, static `twin` is safe on 2/3 rows, while
  `adaptive-twin` is safe on 3/3 rows.  Adaptive control spends energy, but it
  removes the controller-induced failure.
- The right-edge result is more placement-sensitive than the original pilot
  suggested.  In this seed/spacing slice, every feasible right-edge `1.5x`
  no-forecast row is safe for both controllers.  The forecast margin remains
  safe, but it is not needed on these particular feasible placements.
- All-on feasibility is not a bookkeeping detail.  At `450 m`, seed `2` is
  overloaded for both center `2x` and right-edge `1.5x`; at `650 m`, the same
  seed becomes feasible for those target rows.  The `right-edge 2x` boundary is
  mostly overload, with only 1/4 all-on-feasible rows.

The conclusion is stronger when phrased as a boundary result rather than a
universal rule.  Reactive adaptive wakeup protects feasible center-burst rows
that static twin can miss.  Forecast margin protects transition-sensitive
right-edge rows, but only some right-edge placements actually need it.

## Next Step

The first follow-up is recorded in `right-edge-transition-boundary.md`: six
placements at `500 m` show all-on feasibility on 6/6 rows, no-forecast controller
safety on 5/6 rows, and forecasted or underforecast-plus-margin safety on 6/6
rows.  The next sweep should extend that boundary to nearby spacings, for
example `475 m`, `500 m`, and `525 m`, across seeds `1,2`.
