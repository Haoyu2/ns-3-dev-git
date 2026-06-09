# Forecast Correction Delay

This note records a measured-demand correction experiment for the gated
right-edge forecast path.

## Question

Can feedback at the real burst start repair an underpredicted forecast, or does
the controller need pre-burst risk margin?

## Mechanism

The example exposes ``forecastCorrectionDelay``.  A negative value disables
measured-demand correction.  A non-negative value updates the controller-side UE
rate estimate to the actual burst rate after:

```text
correction time = shiftStart + forecastCorrectionDelay
```

When correction is applied, the forecast-error utilization margin is also
cleared at the correction time.  This separates three effects:

- Early forecasted demand before the burst.
- Pre-burst uncertainty margin on the risk gate.
- Post-shift measured-demand correction.

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Base forecast burst multiplier: `1.5`
- Forecast burst-rate error: `-0.25`
- Forecast burst-rate uncertainty bounds: `0.0`, `0.25`
- Forecast correction delays: disabled, `0.0s`
- Forecast lead time: `1.0s`
- Minimum actionable forecast lead: `1.0s`
- Seed: `1`
- Runs: `1,2,3`
- UEs: `16`
- Per-UE base load: `1.0 Mb/s`
- eNB spacing: `500 m`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `4.0`
- Adaptive wake-relief threshold: `0.08`
- Handover guard: `1.25s`

All three all-on rows for this workload are feasible.

## Commands

Correction-delay sweep:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
  --policies=adaptive-twin \
  --seeds=1 \
  --runs=1,2,3 \
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
  --forecast-burst-rate-uncertainties=0.0,0.25 \
  --forecast-correction-delays=off,0.0s \
  --output-dir=/tmp/uno-umb-forecast-correction \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-forecast-correction/aggregate.csv \
  --output-dir=/tmp/uno-umb-forecast-correction-analysis
```

## Feasible-Only Result

| forecast uncertainty | correction delay | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- |
| 0.00 | disabled | 0.667 | 9.568 | 12.757 | 0.333 |
| 0.00 | 0.0s | 0.667 | 9.568 | 11.960 | 0.333 |
| 0.25 | disabled | 1.000 | 11.163 | 11.163 | 0.000 |
| 0.25 | 0.0s | 1.000 | 11.163 | 11.163 | 0.000 |

Measured correction at the burst start does not repair the unsafe hard run when
the pre-burst risk gate already allowed the center cell to sleep.  It spends a
small amount of additional energy but keeps the same `0.667` safe-run rate.
The `0.25` forecast uncertainty margin keeps the hard run safe before the burst
arrives; adding correction at burst start does not change the aggregate result
in this slice.

## Hard-Run Evidence

Run `2` is the hard right-edge placement:

| margin | correction delay | pre-burst cell 1 action | burst-time cell 1 action | handovers | SLA violation |
| --- | --- | --- | --- | --- | --- |
| 0.00 | disabled | sleep at `2.0s` | none | 6 | yes |
| 0.00 | 0.0s | sleep at `2.0s` | wake at `3.0s` | 6 | yes |
| 0.25 | disabled | keep-active at `2.0s` | none | 2 | no |
| 0.25 | 0.0s | keep-active at `2.0s` | keep-active at `3.0s` | 2 | no |

The measured correction is directionally sensible: it wakes the sleeping center
cell at `3.0s` in the no-margin row.  However, that is still too late for the
LTE transition and queue recovery in the hard placement.  The pre-burst margin
avoids the risky sleep decision at `2.0s`.

## Interpretation

This strengthens the contribution framing:

- Feedback correction is useful state hygiene, but it is not a substitute for
  an actionable forecast lead in this hard transition.
- The forecast uncertainty margin protects the controller before the measured
  burst arrives.
- Correction delay is now an explicit experimental dimension, so future sweeps
  can quantify how much post-shift feedback latency is tolerable.

The next step is to run a broader correction-latency surface with `0.0s`,
`0.25s`, `0.5s`, and `1.0s` delays across more traffic profiles.
