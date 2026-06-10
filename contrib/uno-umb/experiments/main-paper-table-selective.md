# Main Paper Table With Selective Overlap Margin

This note consolidates the current paper-facing results after adding the
selective overlap-gated forecast margin.  It combines the replicated
center-burst rows with the full `475/500/525 m` right-edge transition surface.

## Question

Can the current controller support a first manuscript around two distinct
mechanisms?

- Reactive latent-demand wakeup for unannounced center-burst shifts.
- Selective overlap-gated forecast margins for anticipated right-edge
  transitions with underpredicted demand.

## Scenario Set

- Policies: `all-on`, `twin`, `adaptive-twin`
- Center burst:
  - Seeds/runs: three replicated rows at `500 m`
  - UEs: `16`
  - Per-UE base load: `1.0 Mb/s`
  - Burst multipliers: `1.5`, `2.0`, `3.0`
  - Traffic profile: `center-burst`
  - No forecast
- Right-edge burst:
  - Seeds: `1,2`
  - Runs: `1,2`
  - UEs: `16`
  - Per-UE base load: `1.0 Mb/s`
  - eNB spacings: `475 m`, `500 m`, `525 m`
  - Burst multiplier: `1.5`
  - Traffic profile: `right-edge-burst`
  - Forecast lead: `1.0s`
  - Minimum actionable forecast lead: `1.0s`
  - Forecasted burst multiplier: `1.5`
  - Forecast burst-rate error: `-25%`
  - Base forecast burst-rate uncertainty: `0.25`
  - Selective forecast burst-rate uncertainty: `0.35`
  - Selective utilization slack trigger: `0.019`
  - Selective max offload distance: `400 m`

Controller rows are feasible-only: a controller result counts only when the
matching all-on row, with the same seed, run, spacing, and offered-load
scenario, does not violate the SLA.  All-on rows report offered-load
feasibility.

## Commands

The center rows reuse the replicated envelope data from
`replicated-feasibility-envelope.md`.  The right-edge rows reuse the full
surface from `right-edge-selective-overlap-surface.md`.

The clean center-only input table was built from the replicated center rows:

```bash
mkdir -p /tmp/uno-umb-main-paper-table-clean
python3 - <<'PY'
import csv
from pathlib import Path

inputs = [
    Path("/tmp/uno-umb-replicated-envelope-all-on/aggregate.csv"),
    Path("/tmp/uno-umb-replicated-envelope-controllers-center/aggregate.csv"),
]
out = Path("/tmp/uno-umb-main-paper-table-clean/center-aggregate.csv")
rows = []
fields = []
for path in inputs:
    with path.open(newline="") as infile:
        reader = csv.DictReader(infile)
        for field in reader.fieldnames or []:
            if field not in fields:
                fields.append(field)
        for row in reader:
            if row.get("traffic_profile") == "center-burst":
                rows.append(row)

with out.open("w", newline="") as outfile:
    writer = csv.DictWriter(outfile, fieldnames=fields, extrasaction="ignore")
    writer.writeheader()
    writer.writerows(rows)
PY
```

The compact table was then generated with:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-main-paper-table-clean/center-aggregate.csv \
  /tmp/uno-umb-transition-surface-all-on/aggregate.csv \
  /tmp/uno-umb-transition-surface-no-forecast/aggregate.csv \
  /tmp/uno-umb-transition-surface-margin/aggregate.csv \
  /tmp/uno-umb-selective-overlap-surface/aggregate.csv \
  --output-dir=/tmp/uno-umb-main-paper-table-clean
```

## Compact Result

| scenario | control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | safe energy saving stdev (%) | induced violation rate | failure rate | spacings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| center 1.5x | reference | all-on | 3 | 1.000 | 0 | 1.000 | 0.000 | 0.000 | 0.000 | 0.000 | 500 |
| center 1.5x | no forecast | twin | 3 | 1.000 | 3 | 1.000 | 23.920 | 0.000 | 0.000 | 0.000 | 500 |
| center 1.5x | no forecast | adaptive-twin | 3 | 1.000 | 3 | 1.000 | 23.920 | 0.000 | 0.000 | 0.000 | 500 |
| center 2x | reference | all-on | 3 | 1.000 | 0 | 1.000 | 0.000 | 0.000 | 0.000 | 0.000 | 500 |
| center 2x | no forecast | twin | 3 | 1.000 | 3 | 0.667 | 15.947 | 13.810 | 0.333 | 0.000 | 500 |
| center 2x | no forecast | adaptive-twin | 3 | 1.000 | 3 | 1.000 | 7.176 | 0.000 | 0.000 | 0.000 | 500 |
| center 3x | reference | all-on | 3 | 0.667 | 0 | 0.667 | 0.000 | 0.000 | 0.000 | 0.000 | 500 |
| center 3x | no forecast | twin | 3 | 0.667 | 2 | 0.500 | 3.588 | 5.074 | 0.500 | 0.000 | 500 |
| center 3x | no forecast | adaptive-twin | 3 | 0.667 | 2 | 1.000 | 7.176 | 0.000 | 0.000 | 0.000 | 500 |
| right-edge 1.5x | reference | all-on | 12 | 0.917 | 0 | 0.917 | 0.000 | 0.000 | 0.000 | 0.000 | 475,500,525 |
| right-edge 1.5x | no forecast | twin | 12 | 0.917 | 11 | 0.818 | 13.482 | 10.331 | 0.182 | 0.000 | 475,500,525 |
| right-edge 1.5x | no forecast | adaptive-twin | 12 | 0.917 | 11 | 0.818 | 5.871 | 2.903 | 0.182 | 0.000 | 475,500,525 |
| right-edge 1.5x | -25% forecast + 0.25 margin | twin | 12 | 0.917 | 11 | 0.909 | 13.047 | 10.501 | 0.091 | 0.000 | 475,500,525 |
| right-edge 1.5x | -25% forecast + 0.25 margin | adaptive-twin | 12 | 0.917 | 11 | 0.909 | 13.047 | 10.501 | 0.091 | 0.000 | 475,500,525 |
| right-edge 1.5x | -25% forecast + selective overlap margin | twin | 12 | 0.917 | 11 | 1.000 | 13.482 | 9.993 | 0.000 | 0.000 | 475,500,525 |
| right-edge 1.5x | -25% forecast + selective overlap margin | adaptive-twin | 12 | 0.917 | 11 | 1.000 | 13.482 | 9.993 | 0.000 | 0.000 | 475,500,525 |

## Interpretation

The table now supports a stronger paper framing than a single energy-saving
heuristic.

First, unannounced center bursts expose why static risk gating is not enough.
At center `2x`, static `twin` is safe on `2/3` feasible rows, while
`adaptive-twin` is safe on `3/3` feasible rows.  At center `3x`, only `2/3`
offered-load rows are feasible with all cells active; on those feasible rows,
static `twin` is safe on `1/2` and `adaptive-twin` is safe on `2/2`.  The
adaptive controller spends energy earlier, but it removes controller-induced
violations in the feasible subset.

Second, right-edge transition failures are not solved by reactive wakeup alone.
Across the `475/500/525 m` surface, no-forecast control is safe on `9/11`
feasible rows.  A `0.25` underforecast margin improves safety to `10/11`, but
still leaves one tight overlap placement unsafe.  The selective overlap margin
recovers all `11/11` feasible rows with `13.482%` mean safe energy saving,
while the global `0.35` margin previously reached the same safety only at
`4.784%` mean safe energy saving.

## Manuscript Claim

UNO-UMB should be framed as an uncertainty-aware cell sleep controller with two
separable safety mechanisms:

1. A latent-demand wakeup gate that reacts to unannounced load shifts by
   reactivating sleeping preferred cells when doing so relieves active-cell
   utilization stress.
2. A forecast-risk gate with selective overlap inflation that applies a
   stronger uncertainty margin only to tight close-offload decisions, preserving
   savings when the base margin is already safe.

The current result is ready for a first LaTeX draft.  Before submission, the
main remaining validation need is a larger replicated sweep that broadens the
seed, run, spacing, UE-count, and UE-rate envelope around these two mechanisms.

## Next Step

Use this note as the main quantitative backbone of the draft.  The next
experiment should expand the same table to more seeds and moderate UE/rate
variation, preferably on the two Ubuntu machines or a cloud CPU instance.
