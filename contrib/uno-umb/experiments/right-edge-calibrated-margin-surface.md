# Right-Edge Calibrated-Margin Surface

This note reruns the `475/500/525 m` right-edge transition surface with the
`0.35` forecast-error uncertainty value found in the hard-sample calibration.

## Question

Does the calibrated `0.35` forecast-error uncertainty margin remove the
remaining right-edge tail failures across the full spacing surface, and how
much energy saving does that safety cost?

## Scenario Set

- Policies: `twin`, `adaptive-twin`
- Seeds: `1,2`
- Runs: `1,2`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `475 m`, `500 m`, `525 m`
- Traffic profile: `right-edge-burst`
- Burst multiplier: `1.5`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Forecast lead: `1.0s`
- Minimum actionable forecast lead: `1.0s`
- Forecasted burst multiplier: `1.5`
- Forecast burst-rate error: `-25%`
- Forecast burst-rate uncertainty: `0.35`

The all-on, no-forecast, and `0.25` margin aggregates are reused from
`right-edge-transition-surface.md`.

## Command

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
  --forecast-burst-rate-uncertainties=0.35 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-transition-surface-margin035 \
  --skip-build \
  --keep-going
```

Combined analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-transition-surface-all-on/aggregate.csv \
  /tmp/uno-umb-transition-surface-no-forecast/aggregate.csv \
  /tmp/uno-umb-transition-surface-margin/aggregate.csv \
  /tmp/uno-umb-transition-surface-margin035/aggregate.csv \
  --output-dir=/tmp/uno-umb-transition-surface-margin035-analysis
```

## Overall Result

| policy | control | all-on feasible | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- | --- |
| all-on | reference | 11/12 | 0 | 0.917 | 0.000 | 0.000 |
| twin | no forecast | 11/12 | 11 | 0.818 | 13.482 | 0.182 |
| twin | -25% + 0.25 margin | 11/12 | 11 | 0.909 | 13.047 | 0.091 |
| twin | -25% + 0.35 margin | 11/12 | 11 | 1.000 | 4.784 | 0.000 |
| adaptive-twin | no forecast | 11/12 | 11 | 0.818 | 5.871 | 0.182 |
| adaptive-twin | -25% + 0.25 margin | 11/12 | 11 | 0.909 | 13.047 | 0.091 |
| adaptive-twin | -25% + 0.35 margin | 11/12 | 11 | 1.000 | 4.784 | 0.000 |

## Spacing Surface

| spacing | policy | control | all-on feasible | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 475 m | twin | no forecast | 3/4 | 3 | 0.667 | 10.365 | 0.333 |
| 475 m | twin | -25% + 0.25 margin | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 475 m | twin | -25% + 0.35 margin | 3/4 | 3 | 1.000 | 4.784 | 0.000 |
| 475 m | adaptive-twin | no forecast | 3/4 | 3 | 0.667 | 4.784 | 0.333 |
| 475 m | adaptive-twin | -25% + 0.25 margin | 3/4 | 3 | 0.667 | 9.568 | 0.333 |
| 475 m | adaptive-twin | -25% + 0.35 margin | 3/4 | 3 | 1.000 | 4.784 | 0.000 |
| 500 m | twin | no forecast | 4/4 | 4 | 0.750 | 13.754 | 0.250 |
| 500 m | twin | -25% + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 500 m | twin | -25% + 0.35 margin | 4/4 | 4 | 1.000 | 4.784 | 0.000 |
| 500 m | adaptive-twin | no forecast | 4/4 | 4 | 0.750 | 5.382 | 0.250 |
| 500 m | adaptive-twin | -25% + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 500 m | adaptive-twin | -25% + 0.35 margin | 4/4 | 4 | 1.000 | 4.784 | 0.000 |
| 525 m | twin | no forecast | 4/4 | 4 | 1.000 | 15.548 | 0.000 |
| 525 m | twin | -25% + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | twin | -25% + 0.35 margin | 4/4 | 4 | 1.000 | 4.784 | 0.000 |
| 525 m | adaptive-twin | no forecast | 4/4 | 4 | 1.000 | 7.176 | 0.000 |
| 525 m | adaptive-twin | -25% + 0.25 margin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | adaptive-twin | -25% + 0.35 margin | 4/4 | 4 | 1.000 | 4.784 | 0.000 |

The one all-on-infeasible row remains seed `1`, run `2`, spacing `475 m`; it is
excluded from feasible-controller rates.

## Interpretation

The calibrated `0.35` margin removes all controller-induced violations on this
surface.  Both `twin` and `adaptive-twin` improve from 9/11 safe feasible rows
without forecast, to 10/11 with the original `0.25` margin, to 11/11 with the
calibrated margin.  The previously unresolved `475 m` hard sample is recovered
for both controllers.

The cost is visible and important.  The `0.35` margin forces the same
conservative sleep pattern across the full surface, with safe energy saving
`4.784%` on every feasible controller row.  The original `0.25` margin keeps
higher mean energy saving, `13.047%`, but leaves one tail failure.  This creates
a clean safety-energy Pareto point: `0.25` is a higher-saving operating point,
while `0.35` is the fully safe operating point for this surface.

The result strengthens the contribution because the controller now exposes a
tunable risk margin rather than a binary success/failure story.  It also points
to the next design problem: apply the stronger margin only when a transition
appears tight, so easy rows do not pay the full energy cost of the hard row.

## Next Step

Design a selective forecast-margin policy.  A good first experiment is a
two-level rule that uses `0.25` by default and escalates to `0.35` only when the
forecasted post-shift utilization is close to the risk limit.  The paper target
would be to recover the 11/11 safe-run rate while retaining more of the
`0.25` margin energy savings on `500 m` and `525 m` rows.
