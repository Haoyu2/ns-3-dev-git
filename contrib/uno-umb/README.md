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
```

CSV outputs:

- `uno-umb-dt-energy-summary.csv`: one-line run summary for paper plots.
- `uno-umb-dt-energy-events.csv`: controller decisions over time.

Pilot sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py
```

Fuller sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --seeds=1,2,3 \
  --ue-counts=12,16,20 \
  --ue-rates=0.8,1.0,1.2 \
  --spacings=400,500,650 \
  --uncertainty-scales=0.5,1.0,1.5
```

Analyze an aggregate CSV:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py results/uno-umb-dt-energy-*/aggregate.csv
```

Near-term extension list:

- Add mobility and traffic bursts to create distribution shifts.
- Replace the analytical energy constants with a calibrated LTE/NR base-station power model.
- Port the same controller interface to 5G-LENA once the LTE prototype establishes the claim.
