# Right-Edge Hard-Sample Margin Calibration

This note isolates the remaining hard sample from the transition surface and
calibrates the forecast-error uncertainty margin needed to recover it.

## Question

For the `475 m` placement that remained unsafe with a `0.25` forecast-error
margin, is the row recoverable by stronger risk padding, or only by disabling
the energy-saving sleep action?

## Scenario

- Policies: `twin`, `adaptive-twin`
- Seed: `2`
- Run: `2`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacing: `475 m`
- Traffic profile: `right-edge-burst`
- Burst multiplier: `1.5`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Forecast lead: `1.0s`
- Minimum actionable forecast lead: `1.0s`
- Forecasted burst multiplier: `1.5`
- Forecast burst-rate error: `-25%`

The matching all-on reference is SLA-feasible with loss ratio `0.141053`, so
controller-induced loss remains meaningful on this sample.

## Commands

Forecast-error margin bracket:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=2 \
  --runs=2 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=475 \
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
  --forecast-burst-rate-uncertainties=0.28,0.30,0.32 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-475-hard-margin-bracket \
  --skip-build \
  --keep-going
```

Fine bracket and safe-side checks:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=2 \
  --runs=2 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=475 \
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
  --forecast-burst-rate-uncertainties=0.33,0.34 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-475-hard-margin-fine \
  --skip-build \
  --keep-going
```

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=twin,adaptive-twin \
  --seeds=2 \
  --runs=2 \
  --ue-counts=16 \
  --ue-rates=1.0 \
  --spacings=475 \
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
  --forecast-burst-rate-uncertainties=0.35,0.50 \
  --forecast-correction-delays=off \
  --output-dir=/tmp/uno-umb-475-hard-margin-wide \
  --skip-build \
  --keep-going
```

Lower sleep-threshold comparison, repeated for `twin` and `adaptive-twin` with
`sleepUeThreshold` set to `1.5` and `1.0`:

```bash
./ns3 run "uno-umb-dt-energy \
  --policy=twin \
  --seed=2 \
  --run=2 \
  --numberOfUes=16 \
  --ueRateMbps=1.0 \
  --enbSpacingMeters=475.0 \
  --uncertaintyScale=1.0 \
  --adaptiveLatentLoadThreshold=4.0 \
  --adaptiveWakeReliefThreshold=0.08 \
  --forecastLeadTime=1.0s \
  --minForecastLeadTime=1.0s \
  --forecastBurstRateMultiplier=1.5 \
  --forecastBurstRateError=-0.25 \
  --forecastBurstRateUncertainty=0.25 \
  --sleepUeThreshold=1.5 \
  --trafficProfile=right-edge-burst \
  --burstRateMultiplier=1.5 \
  --shiftStart=3.0s \
  --shiftStop=10.0s \
  --simTime=12.0s"
```

## Result

The forecast-error uncertainty bracket has a sharp controller decision boundary:

| policy | forecast uncertainty | loss ratio | energy saving (%) | active cell-s | handovers | SLA violation |
| --- | --- | --- | --- | --- | --- | --- |
| twin | 0.25 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.28 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.30 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.32 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.33 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.34 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| twin | 0.35 | 0.152998 | 4.784 | 34.000 | 2 | 0 |
| twin | 0.50 | 0.152998 | 4.784 | 34.000 | 2 | 0 |
| adaptive-twin | 0.25 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.28 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.30 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.32 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.33 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.34 | 0.177651 | 23.920 | 26.000 | 2 | 1 |
| adaptive-twin | 0.35 | 0.152998 | 4.784 | 34.000 | 2 | 0 |
| adaptive-twin | 0.50 | 0.152998 | 4.784 | 34.000 | 2 | 0 |

The direct lower sleep-threshold comparison is safe but gives up all energy
savings:

| policy | sleep UE threshold | loss ratio | energy saving (%) | active cell-s | handovers | SLA violation |
| --- | --- | --- | --- | --- | --- | --- |
| twin | 1.5 | 0.141053 | 0.000 | 36.000 | 0 | 0 |
| twin | 1.0 | 0.141053 | 0.000 | 36.000 | 0 | 0 |
| adaptive-twin | 1.5 | 0.141053 | 0.000 | 36.000 | 0 | 0 |
| adaptive-twin | 1.0 | 0.141053 | 0.000 | 36.000 | 0 | 0 |

## Interpretation

The hard `475 m` sample is recoverable without fully disabling cell sleep.  A
forecast-error uncertainty value of `0.35` is enough to change the controller
decision from an aggressive `23.920%` energy-saving action that violates the
SLA to a conservative `4.784%` energy-saving action that stays safe.  Values up
to `0.34` keep the aggressive sleep schedule and still violate the SLA.

This gives a stronger framing than the previous surface note.  The `475 m` row
is not simply outside the controller family; it exposes a calibrated safety
margin threshold.  Lowering `sleepUeThreshold` also recovers the sample, but
only by preventing sleep entirely.  The forecast margin is therefore the more
useful knob because it preserves a small nonzero energy gain while removing the
tail violation.

The static and adaptive controllers behave identically on this sample.  This
reinforces the right-edge conclusion: forecast-risk calibration is the active
mechanism here, while adaptive wakeup remains the center-burst contribution.

## Next Step

Run the `475/500/525 m` transition surface again with `0.35` forecast-error
uncertainty.  The goal is to measure whether the calibrated hard-sample margin
improves the full surface without collapsing energy savings on easier rows.
