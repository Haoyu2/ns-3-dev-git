# UNO-UMB DT Energy Prototype

Goal: build a conference-paper-ready experiment around QoS-safe RAN cell sleep.

Initial claim to test:

> An uncertainty-aware lightweight digital twin can save RAN energy while bounding QoS risk
> under traffic/load shifts better than load-threshold or aggressive cell-sleep control.

First baselines:

- `all-on`: no cell sleep.
- `threshold`: sleep cells with low served UE count.
- `aggressive`: sleep more cells using a looser UE-count threshold.
- `twin`: sleep low-load cells only when the digital-twin risk estimate is safe.
- `adaptive-twin`: twin control with online uncertainty scaling from load shock,
  utilization stress, and latent preferred-cell demand.

Current contribution track:

- Per-UE offered-load accounting in the controller.
- Distribution-shift traffic profiles: `steady`, `center-burst`, `edge-burst`,
  `right-edge-burst`, and `global-burst`.
- Online uncertainty adaptation for risk-aware sleep decisions.
- Counterfactual wakeup of sleeping cells when latent preferred-cell demand would
  materially reduce peak active-cell utilization.
- Event-triggered demand-change reevaluation with a handover guard so adaptive
  wakeups react before the next periodic control tick without stacking LTE
  handover requests.
- Forecast lead-time control for anticipated traffic shifts so the controller
  can settle handovers before burst traffic arrives.
- Time-averaged offered-load reporting so shifted and steady runs can be compared
  without mixing scenario definitions.

Build:

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build uno-umb-dt-energy uno-umb-test
```

Useful first runs:

```bash
./ns3 run "uno-umb-dt-energy --policy=all-on"
./ns3 run "uno-umb-dt-energy --policy=threshold"
./ns3 run "uno-umb-dt-energy --policy=aggressive"
./ns3 run "uno-umb-dt-energy --policy=twin"
./ns3 run "uno-umb-dt-energy --policy=adaptive-twin"
./ns3 run "uno-umb-dt-energy --policy=twin --trafficProfile=center-burst --burstRateMultiplier=3.0"
./ns3 run "uno-umb-dt-energy --policy=adaptive-twin --trafficProfile=right-edge-burst --burstRateMultiplier=1.5 --forecastLeadTime=1.0s --forecastBurstRateMultiplier=1.25"
```

CSV outputs:

- `uno-umb-dt-energy-summary.csv`: one-line run summary for paper plots.
- `uno-umb-dt-energy-events.csv`: controller decisions over time.
- Sweep directories also include `aggregate.csv` and `manifest.json`.

Pilot sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py
```

Adaptive calibration sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --traffic-profiles=center-burst \
  --uncertainty-scales=0.5,0.7,1.0
```

Fuller sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --seeds=1,2,3 \
  --runs=1,2,3 \
  --ue-counts=12,16,20 \
  --ue-rates=0.8,1.0,1.2 \
  --spacings=400,500,650 \
  --uncertainty-scales=0.5,1.0,1.5 \
  --traffic-profiles=steady,center-burst,edge-burst,global-burst \
  --burst-rate-multipliers=2.0,3.0,4.0 \
  --forecast-lead-times=0.0s,0.5s,1.0s \
  --forecast-burst-rate-multipliers=0.0,1.5,2.0
```

Analyze an aggregate CSV:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py results/uno-umb-dt-energy-*/aggregate.csv
```

Analysis outputs:

- `policy-summary.csv` and `.md`: policy-level means and standard deviations.
- `scenario-summary.csv` and `.md`: scenario-level policy summaries.
- `pairwise-comparison.csv`: twin deltas against threshold and aggressive baselines.
- `feasibility-comparison.csv`: controller deltas against all-on references.
- `feasible-policy-summary.csv` and `.md`: controller metrics only on workloads
  that are feasible with all cells active.
- `feasibility-envelope-summary.csv` and `.md`: burst-profile/rate feasibility
  envelope with controller metrics restricted to all-on-feasible rows.
- `energy-risk.svg`: dependency-free energy/loss scatter plot.

Experiment notes:

- `experiments/latent-wakeup-ablation.md`: replicated center-burst ablation for
  latent preferred-cell demand wakeup.
- `experiments/feasibility-envelope-pilot.md`: pilot burst-profile/rate envelope
  for separating feasible control regimes from overload regimes.
- `experiments/demand-guard-calibration.md`: replicated center-burst calibration
  for guarded event-triggered latent-demand wakeup.
- `experiments/replicated-feasibility-envelope.md`: replicated burst-profile
  envelope plus right-edge forecast lead-time ablation.
- `experiments/forecast-error-calibration.md`: right-edge calibration for
  under- and over-predicted traffic-shift magnitude.
- `experiments/lead-forecast-surface.md`: compact surface over forecast lead
  time and forecasted burst magnitude.

Near-term extension list:

- Add mobility and inter-cell correlated bursts to create harder distribution shifts.
- Replace the analytical energy constants with a calibrated LTE/NR base-station power model.
- Port the same controller interface to 5G-LENA once the LTE prototype establishes the claim.
