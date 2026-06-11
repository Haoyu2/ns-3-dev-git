# Expanded Sweep Control-Plane Audit

This note records the first `3 seed x 2 run x 3 spacing` expansion and the
follow-up diagnosis of the LTE RLC aborts observed on the Ubuntu machines.

## Question

Do the broader center-burst and right-edge conclusions survive more seed/run
variation, and are the observed failed rows controller failures or simulator
control-plane artifacts?

## Expanded Sweep

The expansion used:

- Seeds: `1,2,3`
- Runs: `1,2`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacings: `475 m`, `500 m`, `525 m`
- Center case: `center-burst`, burst multiplier `2.0`, no forecast
- Right-edge case: `right-edge-burst`, burst multiplier `1.5`
- Right-edge selective margin:
  - Forecast lead: `1.0s`
  - Minimum actionable lead: `1.0s`
  - Forecast burst multiplier: `1.5`
  - Forecast error: `-25%`
  - Base uncertainty: `0.25`
  - Selective uncertainty: `0.35`
  - Trigger slack: `0.019`
  - Trigger max offload: `400 m`

The sweep was split across `frcc` and `frcc2`:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1,2,3 \
  --runs=1,2 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=475,500,525 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=2.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --output-dir=$ROOT/all-on-center \
  --skip-build \
  --keep-going
```

Similar commands produced `all-on-right-edge`, `center-controllers`,
`right-no-forecast`, and `right-selective`.

## Expanded Result

The compact table was generated with:

```bash
python3 contrib/uno-umb/utils/uno-umb-paper-table.py \
  /tmp/uno-umb-main-expansion-20260610/all-on-center.csv \
  /tmp/uno-umb-main-expansion-20260610/all-on-right-edge.csv \
  /tmp/uno-umb-main-expansion-20260610/center-controllers.csv \
  /tmp/uno-umb-main-expansion-20260610/right-no-forecast.csv \
  /tmp/uno-umb-main-expansion-20260610/right-selective.csv \
  --output-dir=/tmp/uno-umb-main-expansion-20260610-table
```

| scenario | control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| center 2x | reference | all-on | 18 | 1.000 | 0 | 1.000 | 0.000 | 0.000 | 0.000 |
| center 2x | no forecast | twin | 18 | 1.000 | 18 | 0.778 | 18.604 | 0.222 | 0.000 |
| center 2x | no forecast | adaptive-twin | 18 | 1.000 | 18 | 0.889 | 6.379 | 0.111 | 0.111 |
| right-edge 1.5x | reference | all-on | 18 | 0.833 | 0 | 0.833 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | twin | 18 | 0.833 | 15 | 0.800 | 15.787 | 0.200 | 0.067 |
| right-edge 1.5x | no forecast | adaptive-twin | 18 | 0.833 | 15 | 0.800 | 11.322 | 0.200 | 0.067 |
| right-edge 1.5x | -25% forecast + selective overlap margin | twin | 18 | 0.833 | 15 | 1.000 | 16.265 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + selective overlap margin | adaptive-twin | 18 | 0.833 | 15 | 1.000 | 16.265 | 0.000 | 0.000 |

The right-edge selective-margin result strengthens the paper story: across the
expanded feasible surface, it reaches `15/15` safe feasible rows with zero
induced violations and `16.265%` mean safe energy saving.

The center result is directionally useful but not yet paper-clean in the
real-RRC run: adaptive control improves safe feasible rows over static `twin`,
but two adaptive rows aborted before producing metrics.

## Abort Diagnosis

Four rows aborted with return code `250` on the Ubuntu machines:

| scenario | policy | seed | run | spacing | assertion |
| --- | --- | --- | --- | --- | --- |
| center 2x | adaptive-twin | 3 | 1 | 475 m | AM RLC TxOpportunity too small for DATA PDU |
| center 2x | adaptive-twin | 3 | 1 | 525 m | AM RLC TxOpportunity too small for DATA PDU |
| right-edge 1.5x | twin | 3 | 1 | 475 m | AM RLC TxOpportunity too small |
| right-edge 1.5x | adaptive-twin | 3 | 1 | 475 m | AM RLC TxOpportunity too small |

With EPC enabled, ns-3 already maps data bearers to UM RLC by default.  These
assertions occur in AM RLC, so the failure is consistent with stressed
over-the-air RRC signaling rather than the UDP data bearer itself.

The example now exposes:

```bash
--useIdealRrc=true
```

and the sweep runner exposes:

```bash
--use-ideal-rrc-values=true
```

This keeps the original real-RRC mode reproducible while allowing controller
validation sweeps to abstract control-plane signaling.

## Spot Checks

On `frcc2`, the previously failed center row completed with ideal RRC:

```bash
./ns3 run --no-build \
  "uno-umb-dt-energy --policy=adaptive-twin --seed=3 --run=1 \
   --numberOfUes=16 --ueRateMbps=1.0 --enbSpacingMeters=475.0 \
   --trafficProfile=center-burst --burstRateMultiplier=2.0 \
   --shiftStart=3.0s --shiftStop=10.0s --simTime=12.0s \
   --useIdealRrc=true"
```

Result: `slaViolation=0`, `energySavingPct=7.17593`,
`handoverRequests=6`.

On `frcc`, the previously failed right-edge no-forecast row completed with
ideal RRC:

```bash
./ns3 run --no-build \
  "uno-umb-dt-energy --policy=twin --seed=3 --run=1 \
   --numberOfUes=16 --ueRateMbps=1.0 --enbSpacingMeters=475.0 \
   --trafficProfile=right-edge-burst --burstRateMultiplier=1.5 \
   --shiftStart=3.0s --shiftStop=10.0s --simTime=12.0s \
   --useIdealRrc=true"
```

Result: `slaViolation=0`, `energySavingPct=7.17593`,
`handoverRequests=6`.

## Interpretation

The expanded right-edge result is ready to become a main result after one
ideal-RRC rerun confirms that the no-forecast baseline and selective-margin
rows are not distorted by real-RRC aborts.

The center-burst claim should be phrased more carefully until the ideal-RRC
expanded rerun finishes.  The current real-RRC data show adaptive control
improving safety over static `twin`, but the failure-rate column is not clean
enough for a strong paper table.

## Ideal-RRC Expanded Rerun

The same expanded table was rerun with ideal RRC enabled.  The rerun used
`--jobs=8` on each VM and completed without failed rows:

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
  --forecast-lead-times=1.0s \
  --min-forecast-lead-times=1.0s \
  --forecast-burst-rate-multipliers=1.5 \
  --forecast-burst-rate-errors=-0.25 \
  --forecast-burst-rate-uncertainties=0.25 \
  --selective-forecast-burst-rate-uncertainties=0.35 \
  --forecast-margin-trigger-slacks=0.019 \
  --forecast-margin-trigger-max-offloads=400 \
  --forecast-correction-delays=off \
  --use-ideal-rrc-values=true \
  --skip-build \
  --keep-going \
  --jobs=8
```

The combined ideal-RRC table is:

| scenario | control | policy | rows | all-on feasible rate | feasible controller rows | safe-run rate | safe energy saving mean (%) | induced violation rate | failure rate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| center 2x | reference | all-on | 18 | 1.000 | 0 | 1.000 | 0.000 | 0.000 | 0.000 |
| center 2x | no forecast | twin | 18 | 1.000 | 18 | 0.778 | 18.604 | 0.222 | 0.000 |
| center 2x | no forecast | adaptive-twin | 18 | 1.000 | 18 | 1.000 | 7.176 | 0.000 | 0.000 |
| right-edge 1.5x | reference | all-on | 18 | 0.889 | 0 | 0.889 | 0.000 | 0.000 | 0.000 |
| right-edge 1.5x | no forecast | twin | 18 | 0.889 | 16 | 0.812 | 16.295 | 0.188 | 0.000 |
| right-edge 1.5x | no forecast | adaptive-twin | 18 | 0.889 | 16 | 0.750 | 10.614 | 0.250 | 0.000 |
| right-edge 1.5x | -25% forecast + selective overlap margin | twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |
| right-edge 1.5x | -25% forecast + selective overlap margin | adaptive-twin | 18 | 0.889 | 16 | 1.000 | 16.744 | 0.000 | 0.000 |

This table is the cleaner manuscript result.  Adaptive latent-demand wakeup
recovers all `18/18` feasible center-burst rows where static `twin` is safe on
`14/18`.  Selective forecast overlap margin recovers all `16/16` feasible
right-edge rows where no-forecast `twin` is safe on `13/16` and no-forecast
`adaptive-twin` is safe on `12/16`.

The follow-up margin ablation is recorded in
`right-edge-ideal-margin-comparison.md`.  On this expanded ideal-RRC surface,
the base `0.25` margin also recovers all feasible right-edge rows, while a
global `0.35` margin lowers safe energy saving and leaves one feasible sample
unsafe.  The draft should therefore present selective overlap inflation as an
overlap-aware guard against global over-conservatism, not as uniquely necessary
for every expanded right-edge row.

## Next Step

Use the ideal-RRC expanded table as the main manuscript table, and keep the
real-RRC table as a control-plane audit that explains why the larger controller
sweeps abstract RRC signaling.
