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

Current contribution track:

- Per-UE offered-load accounting in the controller.
- Distribution-shift traffic profiles: `steady`, `center-burst`, `edge-burst`,
  `right-edge-burst`, and `global-burst`.
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
./ns3 run "uno-umb-dt-energy --policy=twin --trafficProfile=center-burst --burstRateMultiplier=3.0"
```

CSV outputs:

- `uno-umb-dt-energy-summary.csv`: one-line run summary for paper plots.
- `uno-umb-dt-energy-events.csv`: controller decisions over time.
- Sweep directories also include `aggregate.csv` and `manifest.json`.

Pilot sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py
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
  --burst-rate-multipliers=2.0,3.0,4.0
```

Analyze an aggregate CSV:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py results/uno-umb-dt-energy-*/aggregate.csv
```

Analysis outputs:

- `policy-summary.csv` and `.md`: policy-level means and standard deviations.
- `scenario-summary.csv` and `.md`: scenario-level policy summaries.
- `pairwise-comparison.csv`: twin deltas against threshold and aggressive baselines.
- `energy-risk.svg`: dependency-free energy/loss scatter plot.

Near-term extension list:

- Add mobility and inter-cell correlated bursts to create harder distribution shifts.
- Replace the analytical energy constants with a calibrated LTE/NR base-station power model.
- Port the same controller interface to 5G-LENA once the LTE prototype establishes the claim.
