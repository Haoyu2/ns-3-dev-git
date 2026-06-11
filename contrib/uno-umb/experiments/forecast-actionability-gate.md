# Forecast Actionability Gate

This note records the follow-up to the UE-count/load sweep.  The earlier
right-edge load grid showed that a fixed forecast margin improved safety, but a
small subset of moderate-load rows preferred the reactive controller path.

## Question

Can the controller apply forecast protection only when the forecasted shifted
load is large enough to justify it, while letting smaller shifts use the
reactive adaptive path?

## Mechanism

The example now exposes:

```bash
--forecastMinBurstExtraLoadMbps=<value>
```

For a burst forecast, the gate computes:

```text
shifted UEs * per-UE load * max(forecast burst multiplier - 1, 0)
```

If this forecasted shifted extra load is below the threshold, the controller
does not apply the early forecast margin and uses the actual burst multiplier at
the traffic-shift time.  If the value reaches the threshold, the controller
applies the configured forecast lead, burst-rate error, and uncertainty margin.

The tested threshold is `3.25 Mb/s`.  This keeps the `12 UE`, `1.2 Mb/s`
right-edge row on the adaptive reactive path (`3.0 Mb/s` forecasted shifted
extra load), while applying the forecast margin to `16 UE`, `1.0 Mb/s` and
above.

## Scenario Set

- Seeds: `1,2,3`
- Runs: `1,2`
- UEs: `12,16,20`
- Per-UE base load: `0.8,1.0,1.2 Mb/s`
- eNB spacing: `500 m`
- Traffic profile: `right-edge-burst`
- Burst multiplier: `1.5x`
- Forecast lead: `1.0s`
- Minimum lead: `1.0s`
- Forecast burst-rate error: `-25%`
- Forecast uncertainty margin: `0.25`
- RRC mode: ideal

Total planned runs: `108`.

## Execution

The corrected campaign was run on `2026-06-11` after fixing the gate so that a
below-threshold forecast suppresses both the early lead/margin and the
forecasted burst-rate update.

Output root:

```bash
ROOT=~/haoyu/ns-3-uno-umb-results/gated-forecast-extra325-fixed-3seed2run-ideal-20260611
```

`frcc` ran the `twin` rows:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin \
  --seeds=1,2,3 --runs=1,2 \
  --ue-counts=12,16,20 --ue-rates=0.8,1.0,1.2 --spacings=500 \
  --traffic-profiles=right-edge-burst --burst-rate-multipliers=1.5 \
  --sim-time=12.0s --shift-start=3.0s --shift-stop=10.0s \
  --forecast-lead-times=1.0s --min-forecast-lead-times=1.0s \
  --forecast-min-burst-extra-loads=3.25 \
  --forecast-burst-rate-multipliers=1.5 \
  --forecast-burst-rate-errors=-0.25 \
  --forecast-burst-rate-uncertainties=0.25 \
  --forecast-correction-delays=off \
  --use-ideal-rrc-values=true --skip-build --keep-going --jobs=8 \
  --output-dir="$ROOT/right-gated-extra325-twin"
```

`frcc2` ran the same command with `--policies=adaptive-twin` and
`--output-dir="$ROOT/right-gated-extra325-adaptive"`.

Both `54`-row sweeps completed without failures.

## Result

The fixed comparison summary was generated in:

```bash
/tmp/uno-umb-gated-forecast-extra325-fixed-3seed2run-ideal-20260611/summary
```

| scenario | control | policy | feasible rows | safe rows | robust load points | safe energy saving | induced violations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| right-edge 1.5x | no forecast | twin | 39 | 32 | 3/8 | 18.768% | 7 |
| right-edge 1.5x | no forecast | adaptive-twin | 39 | 31 | 4/8 | 16.437% | 8 |
| right-edge 1.5x | `0.25` margin | twin | 39 | 37 | 7/8 | 19.258% | 2 |
| right-edge 1.5x | `0.25` margin | adaptive-twin | 39 | 37 | 7/8 | 19.258% | 2 |
| right-edge 1.5x | gated `0.25` margin | twin | 39 | 37 | 7/8 | 19.258% | 2 |
| right-edge 1.5x | gated `0.25` margin | adaptive-twin | 39 | 39 | 8/8 | 19.626% | 0 |

Per-cell adaptive-twin details:

| UEs | rate | all-on feasible | forecast applied | safe rows |
| --- | --- | --- | --- | --- |
| 12 | 0.8 Mb/s | 6/6 | 0/6 | 6/6 |
| 12 | 1.0 Mb/s | 6/6 | 0/6 | 6/6 |
| 12 | 1.2 Mb/s | 6/6 | 0/6 | 6/6 |
| 16 | 0.8 Mb/s | 6/6 | 0/6 | 6/6 |
| 16 | 1.0 Mb/s | 6/6 | 6/6 | 6/6 |
| 16 | 1.2 Mb/s | 2/6 | 6/6 | 2/2 |
| 20 | 0.8 Mb/s | 6/6 | 6/6 | 6/6 |
| 20 | 1.0 Mb/s | 1/6 | 6/6 | 1/1 |
| 20 | 1.2 Mb/s | 0/6 | 6/6 | not evaluable |

## Interpretation

The gate is a stronger right-edge story than the fixed selective trigger on the
load grid.  For `adaptive-twin`, it preserves the base `0.25` forecast margin on
larger shifted-load rows, removes the induced violations on the moderate
`12 UE`, `1.2 Mb/s` row, and reaches `39/39` safe feasible rows with no
simulation failures.

The same gate does not repair static `twin`; its below-threshold reactive path
remains unsafe on `12 UE`, `1.2 Mb/s`.  The mechanism is therefore best framed
as a combination: adaptive latent-demand wakeup handles small and moderate
unannounced shifts, while an actionability threshold reserves forecast-margin
protection for larger predicted shifts.

## Spacing-Load Cross-Check

A follow-up campaign checked the same adaptive gated controller across the full
right-edge load grid at `475 m`, `500 m`, and `525 m` spacing.  This separates
the actionability result from a single-spacing artifact.

Output root:

```bash
ROOT=~/haoyu/ns-3-uno-umb-results/gated-load-spacing-3seed2run-ideal-20260611
```

The reference all-on grid ran on `frcc`; the adaptive gated grid ran on
`frcc2`.

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=adaptive-twin \
  --seeds=1,2,3 --runs=1,2 \
  --ue-counts=12,16,20 --ue-rates=0.8,1.0,1.2 \
  --spacings=475,500,525 \
  --traffic-profiles=right-edge-burst --burst-rate-multipliers=1.5 \
  --sim-time=12.0s --shift-start=3.0s --shift-stop=10.0s \
  --forecast-lead-times=1.0s --min-forecast-lead-times=1.0s \
  --forecast-min-burst-extra-loads=3.25 \
  --forecast-burst-rate-multipliers=1.5 \
  --forecast-burst-rate-errors=-0.25 \
  --forecast-burst-rate-uncertainties=0.25 \
  --forecast-correction-delays=off \
  --use-ideal-rrc-values=true --skip-build --keep-going --jobs=8 \
  --output-dir="$ROOT/right-gated-extra325-adaptive"
```

The all-on reference uses the same grid with `--policies=all-on` and no
forecast arguments.

The combined summary was generated in:

```bash
/tmp/uno-umb-gated-load-spacing-3seed2run-ideal-20260611/summary
```

| scenario | control | policy | cells | evaluable cells | robust cells | feasible rows | safe rows | safe saving | induced violations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| right-edge 1.5x | reference | all-on | 27 | 27 | 17 | 115/162 | 115 | 0.000% | 0 |
| right-edge 1.5x | gated `0.25` margin | adaptive-twin | 27 | 24 | 24 | 115 | 115 | 19.510% | 0 |

All `324` planned runs completed without failures.  The adaptive gated
controller is safe on every all-on-feasible row across the cross-product of
three spacings, three UE counts, and three per-UE loads.  The all-on reference
is itself infeasible on the heaviest cells, so those rows remain outside the
controller safety denominator.
