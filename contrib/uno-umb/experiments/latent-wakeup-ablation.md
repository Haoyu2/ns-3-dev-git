# Latent Wakeup Ablation

This note records a small replicated ablation for the adaptive-twin latent
preferred-cell demand mechanism.

## Question

Does latent-demand wakeup remove controller-induced SLA violations that remain
when adaptive-twin only uses load-shock and utilization uncertainty scaling?

## Scenario

- Policy family: `adaptive-twin`
- Traffic profile: `center-burst`
- Burst multiplier: `3.0`
- Seed: `1`
- Runs: `1,2,3`
- Uncertainty scales: `0.5,0.7,1.0`
- All-on reference: same seed, runs, and traffic profile
- Feasible all-on runs: `1` and `3`
- Infeasible all-on runs: `2`

The all-on feasibility filter is important because run `2` violates the SLA even
when no cell sleeps.

## Commands

Enabled latent wakeup:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=0.5,0.7,1.0 \
  --adaptive-latent-load-thresholds=2.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-latent-center-campaign \
  --skip-build
```

Disabled latent wakeup:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=0.5,0.7,1.0 \
  --adaptive-latent-load-thresholds=999.0 \
  --adaptive-wake-relief-thresholds=999.0 \
  --output-dir=/tmp/uno-umb-latent-disabled-campaign \
  --skip-build
```

All-on reference:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on \
  --seeds=1 \
  --runs=1,2,3 \
  --traffic-profiles=center-burst \
  --burst-rate-multipliers=3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --output-dir=/tmp/uno-umb-latent-center-all-on \
  --skip-build
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-latent-center-campaign/aggregate.csv \
  /tmp/uno-umb-latent-disabled-campaign/aggregate.csv \
  /tmp/uno-umb-latent-center-all-on/aggregate.csv \
  --output-dir=/tmp/uno-umb-latent-ablation-analysis
```

## Feasible-Only Result

The table below summarizes only runs where all-on satisfies the SLA.

| latent load threshold | wake relief threshold | uncertainty scale | feasible runs | safe-run rate | controller-induced violation rate | energy saving mean (%) | safe energy saving mean (%) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2.0 | 0.08 | 0.5 | 2 | 1.000 | 0.000 | 9.568 | 9.568 |
| 2.0 | 0.08 | 0.7 | 2 | 1.000 | 0.000 | 9.568 | 9.568 |
| 2.0 | 0.08 | 1.0 | 2 | 1.000 | 0.000 | 9.568 | 9.568 |
| 999.0 | 999.0 | 0.5 | 2 | 0.500 | 0.500 | 16.744 | 4.784 |
| 999.0 | 999.0 | 0.7 | 2 | 0.500 | 0.500 | 16.744 | 4.784 |
| 999.0 | 999.0 | 1.0 | 2 | 0.500 | 0.500 | 16.744 | 4.784 |

## Trace Evidence

With latent wakeup enabled, run `3` at uncertainty scale `0.5` wakes the center
cell during the burst:

| time (s) | cell | action | latent load (Mbps) | wake relief | uncertainty scale |
| --- | --- | --- | --- | --- | --- |
| 4.000000 | 1 | wake | 6.000000 | 0.166667 | 0.762251 |

Without latent wakeup, the same run remains in the high-energy-saving sleep
state and violates the SLA.  The ablation therefore supports the claim that
latent preferred-cell demand is the mechanism that converts a risky sleep
decision into a safe moderate-energy-saving decision under center-cell bursts.
