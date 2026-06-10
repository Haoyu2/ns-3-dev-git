# Right-Edge Transition Boundary

This note follows the spacing-sensitivity result by intentionally revisiting the
`500 m` right-edge transition where the original three-placement table found a
forecast-sensitive hard case.

## Question

At `right-edge-burst 1.5x`, is the `500 m` forecast result a single lucky or
unlucky placement, or a measurable transition boundary where anticipatory risk
gating removes controller-induced SLA violations?

## Scenario Set

- Policies: `all-on`, `twin`, `adaptive-twin`
- Seed: `1`
- Runs: `1,2,3,4,5,6`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacing: `500 m`
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

Runs `1,2,3` reuse the existing replicated aggregates.  Runs `4,5,6` add the
same scenario with new placement realizations.

## Commands

Additional all-on placements:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1 \
  --runs=4,5,6 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=500 \
  --traffic-profiles=right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-transition-boundary-all-on-extra \
  --skip-build \
  --keep-going
```

Additional no-forecast controller placements:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=4,5,6 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=500 \
  --traffic-profiles=right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-transition-boundary-no-forecast-extra \
  --skip-build \
  --keep-going
```

Additional perfect-lead controller placements:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=4,5,6 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=500 \
  --traffic-profiles=right-edge-burst \
  --burst-rate-multipliers=1.5 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=4.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --forecast-lead-times=1.0s \
  --output-dir=/tmp/uno-umb-transition-boundary-perfect-lead-extra \
  --skip-build \
  --keep-going
```

Additional underforecast plus margin placements:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=4,5,6 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=500 \
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
  --output-dir=/tmp/uno-umb-transition-boundary-margin-extra \
  --skip-build \
  --keep-going
```

Combined compact table:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-transition-boundary-all-on-extra/aggregate.csv \
  /tmp/uno-umb-replicated-envelope-controllers-edge/aggregate.csv \
  /tmp/uno-umb-transition-boundary-no-forecast-extra/aggregate.csv \
  /tmp/uno-umb-forecast-right-edge-3run/aggregate.csv \
  /tmp/uno-umb-transition-boundary-perfect-lead-extra/aggregate.csv \
  /tmp/uno-umb-paper-forecast-margin-policy/aggregate.csv \
  /tmp/uno-umb-transition-boundary-margin-extra/aggregate.csv \
  --output-dir=/tmp/uno-umb-transition-boundary-table
```

## Compact Result

| control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | safe energy saving stdev (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| reference | all-on | 6 | 1.000 | 0 | 1.000 | 0.000 | 0.000 | 0.000 | 0.000 |
| no forecast | twin | 6 | 1.000 | 6 | 0.833 | 17.142 | 10.742 | 0.167 | 0.000 |
| no forecast | adaptive-twin | 6 | 1.000 | 6 | 0.833 | 8.771 | 7.957 | 0.167 | 0.000 |
| perfect 1s lead | twin | 6 | 1.000 | 6 | 1.000 | 17.541 | 9.882 | 0.000 | 0.000 |
| perfect 1s lead | adaptive-twin | 6 | 1.000 | 6 | 1.000 | 17.541 | 9.882 | 0.000 | 0.000 |
| -25% forecast + 0.25 margin | twin | 6 | 1.000 | 6 | 1.000 | 17.541 | 9.882 | 0.000 | 0.000 |
| -25% forecast + 0.25 margin | adaptive-twin | 6 | 1.000 | 6 | 1.000 | 17.541 | 9.882 | 0.000 | 0.000 |

## Interpretation

All six all-on placements are SLA-feasible, so the unsafe no-forecast row is a
controller-induced transition failure rather than an overloaded workload.  With
no forecast, both `twin` and `adaptive-twin` are safe on 5/6 feasible placements.
The perfect `1.0s` forecast lead recovers the hard placement for both
controllers, and the `-25%` underforecast with `0.25` uncertainty margin also
stays safe on 6/6 placements.

The paper claim should therefore be framed as boundary protection: right-edge
traffic shifts have a narrow feasible transition region where reactive control
usually works, but a pre-burst risk gate removes the observed tail failure.  The
uncertainty margin is the stronger result because it keeps the same safe rate
even when the forecasted burst magnitude is intentionally low.

This also clarifies the role of `adaptive-twin`.  In the center-burst cases,
adaptive wakeup is the main protection mechanism.  In this right-edge boundary,
the decisive factor is forecast timing and margin; once those are present, the
static and adaptive controllers converge to the same safe energy-saving outcome.

## Next Step

Turn this into a small transition surface around the boundary: repeat the same
comparison at `475 m`, `500 m`, and `525 m`, with seeds `1,2`, and keep only
all-on-feasible rows in the paper table.  That would separate placement
sensitivity from spacing sensitivity while keeping the run budget controlled.
