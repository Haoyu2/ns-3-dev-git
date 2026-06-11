# 5-Seed Ideal-RRC Replication Campaign

This note records the broader replication campaign for the current paper
backbone.  The campaign expands the main ideal-RRC result from `3 seeds x 2
runs x 3 spacings` to `5 seeds x 3 runs x 3 spacings`.

## Question

Do the two paper mechanisms remain visible under a larger matched sample?

- Center burst: does latent-demand wakeup continue to remove induced
  violations on feasible rows?
- Right-edge burst: does a forecast-risk margin continue to remove transition
  failures under a `25%` underpredicted burst?
- Right-edge margin: does the stronger global margin continue to show a
  safety-energy penalty relative to the base or selective margin?

## Scenario Set

- Policies: `all-on`, `twin`, `adaptive-twin`
- Seeds: `1,2,3,4,5`
- Runs: `1,2,3`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `475 m`, `500 m`, `525 m`
- Simulation time: `12.0s`
- Shift window: `3.0s` to `10.0s`
- RRC mode: ideal

Center-burst rows use a `2.0x` burst multiplier and no forecast.  Right-edge
rows use a `1.5x` burst multiplier.  Forecasted right-edge rows use a `1.0s`
lead, `1.0s` minimum actionable lead, a `-25%` burst-rate forecast error, and
no correction delay.

## Execution Plan

Output root:

```bash
ROOT=~/haoyu/ns-3-uno-umb-results/replication-5seed3run-ideal-20260611
```

The work is split across two Ubuntu hosts.  Each host runs one background
driver script.  The driver executes its assigned sweep groups sequentially, and
each sweep uses `--jobs=8`.

`frcc` runs:

- `all-on-center`
- `center-controllers`
- `all-on-right-edge`
- `right-no-forecast`

`frcc2` runs:

- `right-margin025`
- `right-margin035`
- `right-selective`
- `right-selective-calibrated`

The initial selective run used slack `0.019` and max offload distance `400 m`.
The larger sample showed that this trigger was too narrow and matched the base
`0.25` margin.  The paper-facing calibrated selective run uses slack `0.038`
and max offload distance `437 m`.

## Harvest Commands

After both driver scripts finish:

```bash
OUT=/tmp/uno-umb-replication-5seed3run-ideal-20260611
REMOTE_ROOT=/home/haoyu/haoyu/ns-3-uno-umb-results/replication-5seed3run-ideal-20260611
mkdir -p "$OUT"
scp "frcc:$REMOTE_ROOT/all-on-center/aggregate.csv" "$OUT/all-on-center.csv"
scp "frcc:$REMOTE_ROOT/center-controllers/aggregate.csv" "$OUT/center-controllers.csv"
scp "frcc:$REMOTE_ROOT/all-on-right-edge/aggregate.csv" "$OUT/all-on-right-edge.csv"
scp "frcc:$REMOTE_ROOT/right-no-forecast/aggregate.csv" "$OUT/right-no-forecast.csv"
scp "frcc2:$REMOTE_ROOT/right-margin025/aggregate.csv" "$OUT/right-margin025.csv"
scp "frcc2:$REMOTE_ROOT/right-margin035/aggregate.csv" "$OUT/right-margin035.csv"
scp "frcc2:$REMOTE_ROOT/right-selective-calibrated/aggregate.csv" "$OUT/right-selective-calibrated.csv"
```

Build the compact paper table:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  "$OUT/all-on-center.csv" \
  "$OUT/center-controllers.csv" \
  "$OUT/all-on-right-edge.csv" \
  "$OUT/right-no-forecast.csv" \
  "$OUT/right-margin025.csv" \
  "$OUT/right-margin035.csv" \
  "$OUT/right-selective-calibrated.csv" \
  --output-dir="$OUT/table"
```

Build the broader analysis bundle:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  "$OUT/all-on-center.csv" \
  "$OUT/center-controllers.csv" \
  "$OUT/all-on-right-edge.csv" \
  "$OUT/right-no-forecast.csv" \
  "$OUT/right-margin025.csv" \
  "$OUT/right-margin035.csv" \
  "$OUT/right-selective-calibrated.csv" \
  --output-dir="$OUT/analysis"
```

## Result

| scenario | control | policy | rows | feasible | safe | safe rate | safe saving |
| --- | --- | --- | --- | --- | --- | --- | --- |
| center 2x | reference | all-on | 45 | 44 | 44 | 0.978 | 0.000% |
| center 2x | no forecast | twin | 45 | 44 | 40 | 0.909 | 21.745% |
| center 2x | no forecast | adaptive-twin | 45 | 44 | 44 | 1.000 | 7.176% |
| right-edge 1.5x | reference | all-on | 45 | 40 | 40 | 0.889 | 0.000% |
| right-edge 1.5x | no forecast | twin | 45 | 40 | 33 | 0.825 | 13.873% |
| right-edge 1.5x | no forecast | adaptive-twin | 45 | 40 | 31 | 0.775 | 10.166% |
| right-edge 1.5x | `0.25` margin | twin | 45 | 40 | 37 | 0.925 | 13.515% |
| right-edge 1.5x | `0.35` global margin | twin | 45 | 40 | 39 | 0.975 | 9.927% |
| right-edge 1.5x | calibrated selective margin | twin | 45 | 40 | 39 | 0.975 | 13.754% |

The center result strengthens the latent-demand wakeup claim: adaptive wakeup
removes all induced violations on the `44` feasible center rows, while static
twin control leaves `4` feasible rows unsafe.

The right-edge result is a calibrated safety-energy frontier.  The base
`0.25` forecast margin fixes four feasible rows relative to adaptive
no-forecast control but leaves three feasible rows unsafe.  A global `0.35`
margin reaches `39/40` safety, but cuts mean safe saving to `9.927%`.  The
calibrated selective margin reaches the same `39/40` safety with `13.754%`
mean safe saving by applying the stronger bound only to close-offload
placements within the calibrated trigger.

## Draft Action

Replace the current draft table and figure data with this replication table.
The paper claim should emphasize calibrated uncertainty control rather than a
blanket statement that the base margin is sufficient on every feasible
right-edge row.  The remaining unsafe row should be kept in the limitations
discussion as a boundary case for the next UE-count and load sweep.
