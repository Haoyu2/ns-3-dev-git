# Right-Edge Ideal-RRC Margin Comparison

This note compares the right-edge forecast margin variants on the expanded
ideal-RRC surface.  It is the main margin ablation for the current draft.

## Question

On the expanded `3 seed x 2 run x 3 spacing` right-edge surface, does selective
overlap inflation improve safety-energy behavior relative to a base forecast
margin and a global conservative margin?

## Scenario Set

- Policies: `twin`, `adaptive-twin`
- Seeds: `1,2,3`
- Runs: `1,2`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `475 m`, `500 m`, `525 m`
- Traffic profile: `right-edge-burst`
- Burst multiplier: `1.5`
- Simulation time: `12.0s`
- Shift window: `3.0s` to `10.0s`
- Forecast lead: `1.0s`
- Minimum actionable lead: `1.0s`
- Forecasted burst multiplier: `1.5`
- Forecast burst-rate error: `-25%`
- RRC mode: ideal

The all-on reference, no-forecast controller rows, and selective-margin rows
reuse `main-expansion-ideal-rrc-20260610`.  This note adds the base `0.25`
forecast margin and global `0.35` forecast margin rows.

## Commands

Base `0.25` margin:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2,3 \
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
  --use-ideal-rrc-values=true \
  --output-dir=$ROOT/right-margin025 \
  --skip-build \
  --keep-going \
  --jobs=8
```

Global `0.35` margin:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1,2,3 \
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
  --use-ideal-rrc-values=true \
  --output-dir=$ROOT/right-margin035 \
  --skip-build \
  --keep-going \
  --jobs=8
```

Combined paper table:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-main-expansion-ideal-rrc-20260610/all-on-right-edge.csv \
  /tmp/uno-umb-main-expansion-ideal-rrc-20260610/right-no-forecast.csv \
  /tmp/uno-umb-right-edge-margin-comparison-ideal-20260611/right-margin025.csv \
  /tmp/uno-umb-right-edge-margin-comparison-ideal-20260611/right-margin035.csv \
  /tmp/uno-umb-main-expansion-ideal-rrc-20260610/right-selective.csv \
  --output-dir=/tmp/uno-umb-right-edge-margin-comparison-ideal-20260611-table
```

## Result

| scenario | control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| right-edge 1.5x | reference | all-on | 18 | 0.889 | 0 | 0.889 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | twin | 18 | 0.889 | 16 | 0.812 | 16.295 | 0.188 | 0.000 |
| right-edge 1.5x | no forecast | adaptive-twin | 18 | 0.889 | 16 | 0.750 | 10.614 | 0.250 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.25 margin | adaptive-twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.35 margin | twin | 18 | 0.889 | 16 | 0.938 | 10.465 | 0.062 | 0.000 |
| right-edge 1.5x | -25% forecast + 0.35 margin | adaptive-twin | 18 | 0.889 | 16 | 0.938 | 10.465 | 0.062 | 0.000 |
| right-edge 1.5x | -25% forecast + selective overlap margin | twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + selective overlap margin | adaptive-twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |

## Interpretation

The expanded ideal-RRC surface changes the right-edge framing.  The base `0.25`
forecast uncertainty margin is already sufficient on this surface: it recovers
all `16/16` feasible rows for both `twin` and `adaptive-twin`.  The selective
overlap margin matches that result because it keeps the base margin on rows
where the stronger overlap condition is not needed.

The global `0.35` margin is not a free safety improvement.  It reduces mean safe
energy saving from `16.744%` to `10.465%` and leaves one feasible sample unsafe
for both controllers: seed `3`, run `2`, spacing `500 m`.  That row falls just
below the goodput SLA after the stronger margin produces a lower-saving
transition pattern.

The manuscript should therefore emphasize forecast-risk margins as the main
right-edge mechanism, with selective overlap inflation as a guard that preserves
the base-margin result while avoiding the blanket global-margin penalty.

## Draft Implication

The strongest expanded claim is no longer that selective overlap uniquely beats
the base `0.25` margin.  The stronger and more defensible claim is:

> A forecast-risk margin eliminates right-edge transition failures under a
> `25%` underpredicted burst, while overlap-aware selectivity avoids the energy
> and residual safety penalty of applying the stronger bound globally.
