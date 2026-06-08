# Feasibility Envelope Pilot

This note records a one-run pilot sweep for separating feasible cell-sleep
control regimes from overload regimes.

## Question

Across burst profile and burst intensity, where does the workload remain
feasible with all cells active, and where should controller SLA violations be
treated as overload rather than policy failure?

## Scenario

- Policies: `all-on`, `twin`, `adaptive-twin`
- Seed: `1`
- Run: `1`
- UE count: `16`
- UE offered load: `1.0 Mbps`
- eNB spacing: `500 m`
- Traffic profiles: `center-burst`, `edge-burst`, `right-edge-burst`,
  `global-burst`
- Burst multipliers: `1.5`, `2.0`, `3.0`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `2.0`
- Adaptive wake-relief threshold: `0.08`

Because this is a pilot with one run, the results should guide the larger
replicated campaign rather than serve as final evidence by themselves.

## Commands

Sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=all-on,twin,adaptive-twin \
  --seeds=1 \
  --runs=1 \
  --traffic-profiles=center-burst,edge-burst,right-edge-burst,global-burst \
  --burst-rate-multipliers=1.5,2.0,3.0 \
  --sim-time=12.0s \
  --shift-start=3.0s \
  --shift-stop=10.0s \
  --uncertainty-scales=1.0 \
  --adaptive-latent-load-thresholds=2.0 \
  --adaptive-wake-relief-thresholds=0.08 \
  --output-dir=/tmp/uno-umb-envelope-pilot \
  --skip-build
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-envelope-pilot/aggregate.csv \
  --output-dir=/tmp/uno-umb-envelope-pilot-analysis
```

## All-On Feasibility

| traffic profile | burst multiplier | all-on feasible |
| --- | --- | --- |
| center-burst | 1.5 | yes |
| center-burst | 2.0 | yes |
| center-burst | 3.0 | yes |
| edge-burst | 1.5 | yes |
| edge-burst | 2.0 | no |
| edge-burst | 3.0 | no |
| right-edge-burst | 1.5 | yes |
| right-edge-burst | 2.0 | no |
| right-edge-burst | 3.0 | no |
| global-burst | 1.5 | no |
| global-burst | 2.0 | no |
| global-burst | 3.0 | no |

## Controller Results Inside the Feasible Envelope

The table below summarizes only rows where the matching all-on run satisfies
the SLA.

| traffic profile | burst multiplier | policy | safe run | safe energy saving (%) | controller-induced violation |
| --- | --- | --- | --- | --- | --- |
| center-burst | 1.5 | twin | 1.000 | 23.920 | 0.000 |
| center-burst | 1.5 | adaptive-twin | 1.000 | 9.568 | 0.000 |
| center-burst | 2.0 | twin | 1.000 | 23.920 | 0.000 |
| center-burst | 2.0 | adaptive-twin | 1.000 | 9.568 | 0.000 |
| center-burst | 3.0 | twin | 1.000 | 9.568 | 0.000 |
| center-burst | 3.0 | adaptive-twin | 1.000 | 9.568 | 0.000 |
| edge-burst | 1.5 | twin | 1.000 | 23.920 | 0.000 |
| edge-burst | 1.5 | adaptive-twin | 1.000 | 23.920 | 0.000 |
| right-edge-burst | 1.5 | twin | 1.000 | 9.568 | 0.000 |
| right-edge-burst | 1.5 | adaptive-twin | 1.000 | 9.568 | 0.000 |

## Interpretation

This pilot suggests three operating regions:

- Center-cell bursts provide the main feasible hidden-demand regime and remain
  feasible through `3.0x` in run `1`.
- Edge-localized bursts have a narrower feasible region: `1.5x` remains
  feasible, while `2.0x` and above overload all-on.
- Global bursts overload all-on at every tested multiplier and should be treated
  as capacity stress rather than a controller-failure regime.

Within the feasible rows of this one-run pilot, both twin and adaptive-twin
avoid controller-induced SLA violations.  Static twin saves more energy in the
low-stress center-burst rows, while adaptive-twin keeps the conservative
moderate-saving action that was safer under the replicated center-burst
ablation.  The next paper-grade campaign should replicate the feasible rows
across multiple runs and sweep the adaptive latent-demand thresholds to recover
more low-stress energy saving without reintroducing the run-3 hidden-demand
failure.
