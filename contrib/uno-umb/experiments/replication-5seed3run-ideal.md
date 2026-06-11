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
scp "frcc2:$REMOTE_ROOT/right-selective/aggregate.csv" "$OUT/right-selective.csv"
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
  "$OUT/right-selective.csv" \
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
  "$OUT/right-selective.csv" \
  --output-dir="$OUT/analysis"
```

## Decision Rule

If the larger matched sample preserves the same qualitative ordering, replace
the current draft table and figure data with the replication table.  If it does
not, keep this campaign as the boundary-finding result and inspect the
newly unsafe rows by seed, run, and spacing before changing the paper claim.
