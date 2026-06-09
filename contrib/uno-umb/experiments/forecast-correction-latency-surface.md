# Forecast Correction Latency Surface

This note broadens the measured-demand correction experiment from a single
instant correction point to a small latency surface.

## Question

How much post-shift feedback latency can the hard right-edge transition tolerate
when the forecast underpredicts the burst magnitude?

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Base forecast burst multiplier: `1.5`
- Forecast burst-rate error: `-0.25`
- Forecast burst-rate uncertainty bounds: `0.0`, `0.25`
- Forecast correction delays: disabled, `0.0s`, `0.25s`, `0.5s`, `1.0s`
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

All three all-on rows for this workload are feasible.  The analyzer counts
failed simulator rows as unsafe rows with zero energy saving, so failure-rate
and safe-run-rate columns should be read together.

## Commands

Correction-latency sweep:

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
  --forecast-correction-delays=off,0.0s,0.25s,0.5s,1.0s \
  --output-dir=/tmp/uno-umb-forecast-correction-latency \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-forecast-correction-latency/aggregate.csv \
  --output-dir=/tmp/uno-umb-forecast-correction-latency-analysis
```

## Feasible-Only Result

| forecast uncertainty | correction delay | completed / attempted | failure rate | safe-run rate | safe energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- | --- |
| 0.00 | disabled | 3 / 3 | 0.000 | 0.667 | 9.568 | 0.333 |
| 0.00 | 0.0s | 3 / 3 | 0.000 | 0.667 | 9.568 | 0.333 |
| 0.00 | 0.25s | 3 / 3 | 0.000 | 0.667 | 9.568 | 0.333 |
| 0.00 | 0.5s | 2 / 3 | 0.333 | 0.667 | 9.568 | 0.333 |
| 0.00 | 1.0s | 3 / 3 | 0.000 | 0.667 | 9.568 | 0.333 |
| 0.25 | disabled | 3 / 3 | 0.000 | 1.000 | 11.163 | 0.000 |
| 0.25 | 0.0s | 3 / 3 | 0.000 | 1.000 | 11.163 | 0.000 |
| 0.25 | 0.25s | 3 / 3 | 0.000 | 1.000 | 11.163 | 0.000 |
| 0.25 | 0.5s | 3 / 3 | 0.000 | 1.000 | 11.163 | 0.000 |
| 0.25 | 1.0s | 3 / 3 | 0.000 | 1.000 | 11.163 | 0.000 |

The no-margin rows remain unsafe for one of the three placements regardless of
whether the measured correction happens immediately, after `0.25s`, or after
`1.0s`.  The no-margin `0.5s` row also records one debug-mode LTE RLC failure
in the hard placement, so it is counted as an unsafe simulator failure rather
than as a successful controller run.

The `0.25` forecast uncertainty margin keeps all three placements safe at every
tested feedback latency.  In this slice, the margin determines the pre-burst
sleep decision, while measured feedback mainly keeps the controller state
consistent after the real traffic shift is visible.

## Hard-Run Evidence

Run `2` is the hard right-edge placement.

| margin | correction delay | pre-burst cell 1 action | correction-time cell 1 action | handovers | outcome |
| --- | --- | --- | --- | --- | --- |
| 0.00 | disabled | sleep at `2.0s` | wake at `4.0s` | 6 | SLA violation |
| 0.00 | 0.0s | sleep at `2.0s` | wake at `3.0s` | 6 | SLA violation |
| 0.00 | 0.25s | sleep at `2.0s` | wake at `3.25s` | 6 | SLA violation |
| 0.00 | 0.5s | not recorded | none recorded | 0 | LTE RLC assertion at `8.005s` |
| 0.00 | 1.0s | sleep at `2.0s` | wake at `4.0s` | 6 | SLA violation |
| 0.25 | disabled | keep-active at `2.0s` | keep-active at `4.0s` | 2 | safe |
| 0.25 | 0.0s | keep-active at `2.0s` | keep-active at `3.0s` | 2 | safe |
| 0.25 | 0.25s | keep-active at `2.0s` | keep-active at `3.25s` | 2 | safe |
| 0.25 | 0.5s | keep-active at `2.0s` | keep-active at `3.5s` | 2 | safe |
| 0.25 | 1.0s | keep-active at `2.0s` | keep-active at `4.0s` | 2 | safe |

The hard row shows why this is not just a feedback-latency problem.  Without
margin, the controller has already allowed cell 1 to sleep at `2.0s`.  Waking it
at `3.0s` or `3.25s` lowers the energy saving but does not avoid the SLA
failure.  With the margin, the same cell stays active before the burst, which
removes the transition failure and reduces handovers from 6 to 2.

## Analysis-Script Note

This sweep also exposed a grouping issue in the analysis script: failed rows can
format numeric fields differently from completed summary rows, for example
`1.375` versus `1.375000`.  The analyzer now canonicalizes numeric scenario keys
before grouping run-status, scenario, pairwise, feasible-policy, and feasibility
envelope summaries.  This keeps failed and completed rows in the same scenario
bucket.

## Interpretation

The result sharpens the forecast contribution:

- Measured-demand correction is useful for state consistency, but it does not
  replace pre-shift risk margin in this transition-sensitive placement.
- A small forecast uncertainty bound changes the risk decision before the burst,
  where it can still avoid the risky sleep action.
- Simulator failures are now accounted for conservatively instead of being
  hidden by scenario-key formatting differences.

The next step should move from this single hard transition to a compact
policy-comparison table: all-on, twin, and adaptive-twin over center-burst and
right-edge-burst feasible rows, with forecast lead, error margin, and correction
latency reported as separate controls.
