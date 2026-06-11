# UE-Count and Load Sweep

This note records the first compact load-sensitivity campaign for the current
paper backbone.  The campaign keeps the calibrated controller settings fixed
and varies UE count and per-UE offered load around the main `16 UE`, `1.0
Mb/s` operating point.

## Question

Do the current mechanisms remain useful under moderate load variation?

- Center burst: does latent-demand wakeup continue to remove controller-induced
  violations when UE count and per-UE rate vary?
- Right-edge burst: does the calibrated selective forecast margin keep the
  safety-energy advantage over no forecast, the base `0.25` margin, and the
  global `0.35` margin?

## Scenario Set

- Seeds: `1,2,3`
- Runs: `1,2`
- UEs: `12,16,20`
- Per-UE base load: `0.8,1.0,1.2 Mb/s`
- eNB spacing: `500 m`
- Simulation time: `12.0s`
- Shift window: `3.0s` to `10.0s`
- RRC mode: ideal

Center-burst rows use a `2.0x` burst multiplier and no forecast.  Right-edge
rows use a `1.5x` burst multiplier.  Forecasted right-edge rows use a `1.0s`
lead, `1.0s` minimum actionable lead, a `-25%` burst-rate forecast error, and
no correction delay.

The calibrated selective right-edge controller uses:

- Base forecast uncertainty: `0.25`
- Selective forecast uncertainty: `0.35`
- Selective trigger slack: `0.038`
- Selective max offload distance: `437 m`

Total planned runs: `648`.

## Execution Plan

Output root:

```bash
ROOT=~/haoyu/ns-3-uno-umb-results/load-sweep-3seed2run-ideal-20260611
```

The work is split across two Ubuntu hosts.  Each host runs one background
driver script.  Each sweep group uses `--jobs=8`.

`frcc` runs:

- `all-on-center`
- `center-controllers`
- `all-on-right-edge`
- `right-no-forecast`

`frcc2` runs:

- `right-margin025`
- `right-margin035`
- `right-selective-calibrated`

Common sweep arguments:

```bash
--seeds=1,2,3
--runs=1,2
--ue-counts=12,16,20
--ue-rates=0.8,1.0,1.2
--spacings=500
--sim-time=12.0s
--shift-start=3.0s
--shift-stop=10.0s
--uncertainty-scales=1.0
--adaptive-latent-load-thresholds=4.0
--adaptive-wake-relief-thresholds=0.08
--use-ideal-rrc-values=true
--skip-build
--keep-going
--jobs=8
```

## Harvest Commands

After both driver scripts finish:

```bash
OUT=/tmp/uno-umb-load-sweep-3seed2run-ideal-20260611
REMOTE_ROOT=/home/haoyu/haoyu/ns-3-uno-umb-results/load-sweep-3seed2run-ideal-20260611
mkdir -p "$OUT"
scp "frcc:$REMOTE_ROOT/all-on-center/aggregate.csv" "$OUT/all-on-center.csv"
scp "frcc:$REMOTE_ROOT/center-controllers/aggregate.csv" "$OUT/center-controllers.csv"
scp "frcc:$REMOTE_ROOT/all-on-right-edge/aggregate.csv" "$OUT/all-on-right-edge.csv"
scp "frcc:$REMOTE_ROOT/right-no-forecast/aggregate.csv" "$OUT/right-no-forecast.csv"
scp "frcc2:$REMOTE_ROOT/right-margin025/aggregate.csv" "$OUT/right-margin025.csv"
scp "frcc2:$REMOTE_ROOT/right-margin035/aggregate.csv" "$OUT/right-margin035.csv"
scp "frcc2:$REMOTE_ROOT/right-selective-calibrated/aggregate.csv" "$OUT/right-selective-calibrated.csv"
```

Build the compact load-sweep summaries:

```bash
python3 contrib/uno-umb/utils/uno-umb-load-sweep-summary.py \
  "$OUT/all-on-center.csv" \
  "$OUT/center-controllers.csv" \
  "$OUT/all-on-right-edge.csv" \
  "$OUT/right-no-forecast.csv" \
  "$OUT/right-margin025.csv" \
  "$OUT/right-margin035.csv" \
  "$OUT/right-selective-calibrated.csv" \
  --output-dir="$OUT/summary"
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

## Status

Launched and completed on `2026-06-11`.

Remote driver logs:

- `frcc:$ROOT/frcc-driver.log`
- `frcc2:$ROOT/frcc2-driver.log`

All `648` planned runs completed without simulation failures.

## Result

The compact load-sweep summary was generated in:

```bash
/tmp/uno-umb-load-sweep-3seed2run-ideal-20260611/summary
```

Overall feasible-row result:

| scenario | control | policy | feasible rows | safe rows | robust load points | safe energy saving |
| --- | --- | --- | --- | --- | --- | --- |
| center 2x | reference | all-on | 40/54 | 40/54 | 6/9 | 0.000% |
| center 2x | no forecast | twin | 40 | 35 | 5/8 | 20.093% |
| center 2x | no forecast | adaptive-twin | 40 | 40 | 8/8 | 7.176% |
| right-edge 1.5x | reference | all-on | 39/54 | 39/54 | 6/9 | 0.000% |
| right-edge 1.5x | no forecast | twin | 39 | 32 | 3/8 | 18.768% |
| right-edge 1.5x | no forecast | adaptive-twin | 39 | 31 | 4/8 | 16.437% |
| right-edge 1.5x | `0.25` margin | twin | 39 | 37 | 7/8 | 19.258% |
| right-edge 1.5x | `0.25` margin | adaptive-twin | 39 | 37 | 7/8 | 19.258% |
| right-edge 1.5x | `0.35` global margin | twin | 39 | 36 | 6/8 | 16.682% |
| right-edge 1.5x | `0.35` global margin | adaptive-twin | 39 | 36 | 6/8 | 16.682% |
| right-edge 1.5x | calibrated selective margin | twin | 39 | 37 | 7/8 | 18.768% |
| right-edge 1.5x | calibrated selective margin | adaptive-twin | 39 | 36 | 6/8 | 17.786% |

Here a robust load point means every feasible row at that UE-count/rate cell is
safe for the given controller.  The `20 UE`, `1.2 Mb/s` load point is
all-on-infeasible for both scenarios and is not counted as an evaluable
controller point.

All-on feasibility already degrades at the high-load edge:

| scenario | UEs | rate | all-on feasible rows |
| --- | --- | --- | --- |
| center 2x | 16 | 1.2 Mb/s | 2/6 |
| center 2x | 20 | 1.0 Mb/s | 2/6 |
| center 2x | 20 | 1.2 Mb/s | 0/6 |
| right-edge 1.5x | 16 | 1.2 Mb/s | 2/6 |
| right-edge 1.5x | 20 | 1.0 Mb/s | 1/6 |
| right-edge 1.5x | 20 | 1.2 Mb/s | 0/6 |

## Interpretation

The center-burst result is strong.  Across all `40` all-on-feasible center rows,
static `twin` control is safe on `35/40`, while adaptive latent-demand wakeup
is safe on `40/40`.  The adaptive controller pays an energy cost, but it
removes controller-induced violations across every evaluable UE-count/rate
cell.

The right-edge result is useful but less settled.  Forecast margins improve
the no-forecast baseline: no-forecast `twin` is safe on `32/39`, while the
base `0.25` margin and calibrated selective `twin` are each safe on `37/39`.
However, the fixed calibrated selective trigger does not dominate the simpler
base margin under load variation.  The base `0.25` margin reaches the same
`37/39` safety with slightly higher mean safe saving than selective `twin`,
and selective `adaptive-twin` falls to `36/39`.

This makes the next right-edge contribution sharper: the paper can keep the
current calibrated selective result for the replicated spacing surface, but
should present the UE-count/load sweep as evidence that forecast-risk gating
needs load-aware calibration before claiming broad robustness.

## Draft Action

Use this campaign as a limitations-and-next-step result rather than as a new
main table.  The draft can state that adaptive latent-demand wakeup generalized
cleanly across the moderate load grid, while right-edge forecast-risk gating
requires a load-aware trigger or uncertainty rule before becoming a broad
robustness claim.
