# Demand-Guard Calibration

This note records a replicated center-burst calibration for guarded
event-triggered latent-demand wakeup.

## Question

Can adaptive-twin react to hidden preferred-cell demand before the next periodic
control tick, while avoiding extra handover churn and preserving low-stress
energy savings?

## Scenario

- Policy family: `adaptive-twin`
- Traffic profile: `center-burst`
- Burst multipliers: `1.5`, `2.0`, `3.0`
- Seed: `1`
- Runs: `1,2,3`
- Uncertainty scale: `1.0`
- Wake relief threshold: `0.08`
- Candidate latent-load thresholds: `2.0`, `4.0`, disabled as `999.0`
- Demand-change reevaluation: enabled
- Handover guard: `1.25s`
- All-on feasible rows: 8 of 9

Run `2` at burst multiplier `3.0` violates the SLA even with all cells active,
so controller results for that row are treated as overload rather than
controller-induced failure.

## Commands

All-on reference:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=2.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-center-threshold-all-on \
  --skip-build
```

Static twin reference:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=2.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-center-threshold-twin \
  --skip-build
```

Guarded adaptive sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=2.0,4.0,999.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-center-demand-guard-adaptive \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-center-threshold-all-on/aggregate.csv \
  /tmp/uno-umb-center-threshold-twin/aggregate.csv \
  /tmp/uno-umb-center-demand-guard-adaptive/aggregate.csv \
  --output-dir=/tmp/uno-umb-center-demand-guard-analysis
```

## Feasible-Only Result

The table below summarizes completed controller rows where all-on satisfies the
SLA.  Failed simulator runs are tracked separately in the run-status summary.

| policy | latent threshold | completed feasible rows | safe-run rate | controller-induced violation rate | energy saving mean (%) | safe energy saving mean (%) |
| --- | --- | --- | --- | --- | --- | --- |
| adaptive-twin | 2.0 | 7 | 1.000 | 0.000 | 7.176 | 7.176 |
| adaptive-twin | 4.0 | 8 | 1.000 | 0.000 | 13.455 | 13.455 |
| adaptive-twin | 999.0 | 8 | 0.750 | 0.250 | 21.827 | 15.847 |
| twin | 2.0 | 8 | 0.750 | 0.250 | 22.126 | 16.146 |

Threshold `4.0` is the best calibrated default in this replicated slice: it
keeps all feasible rows safe, has no simulator failures, and recovers more safe
energy saving than threshold `2.0`.

## Envelope Detail

| burst multiplier | threshold | feasible controller rows | safe-run rate | safe energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- |
| 1.5 | 2.0 | 2 | 1.000 | 7.176 | 0.000 |
| 1.5 | 4.0 | 3 | 1.000 | 23.920 | 0.000 |
| 1.5 | 999.0 | 3 | 1.000 | 23.920 | 0.000 |
| 2.0 | 2.0 | 3 | 1.000 | 7.176 | 0.000 |
| 2.0 | 4.0 | 3 | 1.000 | 7.176 | 0.000 |
| 2.0 | 999.0 | 3 | 0.667 | 15.947 | 0.333 |
| 3.0 | 2.0 | 2 | 1.000 | 7.176 | 0.000 |
| 3.0 | 4.0 | 2 | 1.000 | 7.176 | 0.000 |
| 3.0 | 999.0 | 2 | 0.500 | 3.588 | 0.500 |

The calibrated threshold preserves the high-saving sleep state at `1.5x`, but
wakes immediately at `2.0x` and `3.0x` when latent preferred-cell load reaches
the threshold.

## Trace Evidence

For run `2` at burst multiplier `2.0` and threshold `4.0`, cell `1` wakes at
the demand-change instant:

| time (s) | cell | action | latent load (Mbps) | wake relief | handovers |
| --- | --- | --- | --- | --- | --- |
| 3.000000 | 1 | wake | 4.000000 | 0.222222 | 6 |

The same row with latent wakeup disabled stays in the high-energy sleep state
and violates the SLA.  This supports a contribution framed as guarded
event-triggered latent-demand control rather than simple periodic thresholding.

## Negative Calibration Result

Threshold `2.0` is too reactive.  It wakes at `1.5x`, reducing energy saving to
`7.176%`, and one run at `1.5x` hit the LTE scheduler assertion:
`TxOpportunity (size = 2) too small`.  The sweep harness records that simulator
failure and continues, so this instability remains visible in
`run-status-summary.md` instead of being lost as an aborted campaign.
