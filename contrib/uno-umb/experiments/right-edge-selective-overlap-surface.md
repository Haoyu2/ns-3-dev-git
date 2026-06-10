# Right-Edge Selective Overlap Surface

This note reruns the full `475/500/525 m` right-edge transition surface with
the selective overlap-gated forecast margin from
`right-edge-selective-overlap-margin.md`.

## Question

Does the selective `0.25/0.35` overlap-gated forecast margin recover the full
feasible-row safety of the global `0.35` margin while retaining the higher
energy savings of the `0.25` margin on wider safe placements?

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
- Base forecast burst-rate uncertainty: `0.25`
- Selective forecast burst-rate uncertainty: `0.35`
- Selective utilization slack trigger: `0.019`
- Selective max offload distance: `400 m`

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
  --forecast-burst-rate-uncertainties=0.25 \
  --selective-forecast-burst-rate-uncertainties=0.35 \
  --forecast-margin-trigger-slacks=0.019 \
  --forecast-margin-trigger-max-offloads=400 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-selective-overlap-surface \
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
  /tmp/uno-umb-selective-overlap-surface/aggregate.csv \
  --output-dir=/tmp/uno-umb-selective-overlap-surface-analysis
```

## Overall Result

| policy | control | feasible rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- |
| adaptive-twin | no forecast | 11 | 0.818 | 5.871 | 0.182 |
| adaptive-twin | -25% + 0.25 margin | 11 | 0.909 | 13.047 | 0.091 |
| adaptive-twin | -25% + selective overlap margin | 11 | 1.000 | 13.482 | 0.000 |
| adaptive-twin | -25% + 0.35 margin | 11 | 1.000 | 4.784 | 0.000 |
| twin | no forecast | 11 | 0.818 | 13.482 | 0.182 |
| twin | -25% + 0.25 margin | 11 | 0.909 | 13.047 | 0.091 |
| twin | -25% + selective overlap margin | 11 | 1.000 | 13.482 | 0.000 |
| twin | -25% + 0.35 margin | 11 | 1.000 | 4.784 | 0.000 |

## Spacing Surface

| spacing | policy | all-on feasible | feasible rows | safe-run rate | safe energy saving mean (%) | induced violation rate |
| --- | --- | --- | --- | --- | --- | --- |
| 475 m | adaptive-twin | 3/4 | 3 | 1.000 | 11.163 | 0.000 |
| 475 m | twin | 3/4 | 3 | 1.000 | 11.163 | 0.000 |
| 500 m | adaptive-twin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 500 m | twin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | adaptive-twin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |
| 525 m | twin | 4/4 | 4 | 1.000 | 14.352 | 0.000 |

The one all-on-infeasible row is still seed `1`, run `2`, spacing `475 m`.
It is excluded from the feasible-controller rates.

## Trigger Audit

At the first forecasted decision, the selected margin confirms that the
overlap gate is selective:

| policy | spacing | selected margin | action/safety pattern at cell 1 |
| --- | --- | --- | --- |
| twin | 475 m | base on 3 rows, selective on 1 row | one sleep, three keep-active |
| twin | 500 m | base on 4 rows | two sleep, two keep-active |
| twin | 525 m | base on 4 rows | two sleep, two keep-active |
| adaptive-twin | 475 m | base on 3 rows, selective on 1 row | one sleep, three keep-active |
| adaptive-twin | 500 m | base on 4 rows | two sleep, two keep-active |
| adaptive-twin | 525 m | base on 4 rows | two sleep, two keep-active |

The selective margin is used only on the previously unsafe hard sample.  Wider
placements retain the base `0.25` forecast margin and keep the high-saving
sleep action when it is already safe.

## Interpretation

The selective overlap margin dominates the global calibrated margin on this
surface.  It preserves the same `11/11` safe feasible-row rate and zero induced
violations, but raises mean safe energy saving from `4.784%` to `13.482%`.

This gives the paper a stronger contribution than a single calibrated safety
margin.  The controller now has a spatially conditioned uncertainty mechanism:
it applies the strong margin when a tight decision also has close offload
geometry, which is the condition associated with the right-edge interference
tail.  It avoids charging that conservative margin to wider placements where
the base forecast margin is already safe.

## Draft Implication

This result is strong enough to become one of the main paper tables.  Together
with the center-burst latent-wakeup result, the current prototype now supports
a first LaTeX draft organized around two mechanisms: reactive latent-demand
wakeup for center bursts and selective forecast-risk gating for anticipated
right-edge shifts.

## Next Step

Run one broader replicated policy table across the main paper scenarios:
center burst with adaptive wakeup, right-edge forecast without margin, right-edge
with `0.25`, global `0.35`, and selective overlap margin.  The two `32`-CPU
Ubuntu VMs are good targets for that larger sweep.
