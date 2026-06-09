# Forecast Uncertainty Margin

This note records a forecast-error uncertainty margin for the gated right-edge
transition.

## Question

Can the controller recover unsafe underpredicted forecasts without changing the
forecasted load estimate itself?

## Mechanism

The example exposes ``forecastBurstRateUncertainty``.  During an applied
forecast window, the controller adds a utilization margin to the twin risk
estimate:

```text
forecast utilization margin =
    shifted UEs * ue rate * (base forecast multiplier - 1)
    * forecastBurstRateUncertainty / cell capacity
```

The margin changes the risk gate, not the application traffic and not the
forecasted load estimate.  In the scenario below, uncertainty bounds `0.25` and
`0.50` map to utilization margins `0.048611` and `0.097222`.

## Scenario

- Policy: `adaptive-twin`
- Traffic profile: `right-edge-burst`
- Actual burst multiplier: `1.5`
- Base forecast burst multiplier: `1.5`
- Forecast burst-rate errors: `-0.5`, `-0.25`
- Forecast burst-rate uncertainty bounds: `0.0`, `0.25`, `0.5`
- Forecast lead time: `1.0s`
- Minimum actionable forecast lead: `1.0s`
- Seed: `1`
- Runs: `1,2,3`
- UEs: `16`
- Shifted UEs: `7`
- Per-UE base load: `1.0 Mb/s`
- Cell capacity: `18.0 Mb/s`
- eNB spacing: `500 m`
- Shift window: `3.0s` to `10.0s`
- Simulation time: `12.0s`
- Uncertainty scale: `1.0`
- Adaptive latent-load threshold: `4.0`
- Adaptive wake-relief threshold: `0.08`
- Handover guard: `1.25s`

All three all-on rows for this workload are feasible.

## Commands

Forecast-margin sweep:

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
  --forecast-burst-rate-errors=-0.5,-0.25 \
  --forecast-burst-rate-uncertainties=0.0,0.25,0.5 \
  --output-dir=/tmp/uno-umb-forecast-margin \
  --skip-build \
  --keep-going
```

Analysis:

```bash
python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
  /tmp/uno-umb-replicated-envelope-all-on/aggregate.csv \
  /tmp/uno-umb-forecast-margin/aggregate.csv \
  --output-dir=/tmp/uno-umb-forecast-margin-analysis
```

## Feasible-Only Result

| forecast error | uncertainty bound | utilization margin | safe-run rate | safe energy saving mean (%) | energy saving mean (%) | controller-induced violation rate |
| --- | --- | --- | --- | --- | --- | --- |
| -0.50 | 0.00 | 0.000000 | 0.667 | 15.947 | 23.920 | 0.333 |
| -0.50 | 0.25 | 0.048611 | 0.667 | 15.947 | 23.920 | 0.333 |
| -0.50 | 0.50 | 0.097222 | 1.000 | 11.163 | 11.163 | 0.000 |
| -0.25 | 0.00 | 0.000000 | 0.667 | 9.568 | 12.757 | 0.333 |
| -0.25 | 0.25 | 0.048611 | 1.000 | 11.163 | 11.163 | 0.000 |
| -0.25 | 0.50 | 0.097222 | 1.000 | 4.784 | 4.784 | 0.000 |

The result is boundary-shaped.  A `0.25` uncertainty bound recovers the `-25%`
underforecast case, but it does not recover the `-50%` case.  A `0.50` bound
recovers both underforecast cases, but it can spend more energy than needed.

## Hard-Run Evidence

Run `2` is the hard right-edge placement.  At `2.0s`, before the real burst
starts, the margin determines whether the risk gate treats the center-cell
sleep action as safe.

| forecast error | uncertainty bound | cell 1 action at 2.0s | max utilization | total utilization uncertainty | forecast uncertainty | twin safe | SLA violation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| -0.50 | 0.00 | sleep | 0.597222 | 0.128411 | 0.000000 | yes | yes |
| -0.50 | 0.25 | sleep | 0.597222 | 0.177023 | 0.048611 | yes | yes |
| -0.50 | 0.50 | keep-active | 0.597222 | 0.225634 | 0.097222 | no | no |
| -0.25 | 0.00 | sleep | 0.645833 | 0.133273 | 0.000000 | yes | yes |
| -0.25 | 0.25 | keep-active | 0.645833 | 0.181884 | 0.048611 | no | no |

This gives a useful contribution framing: the controller does not need to
overestimate the load directly.  It can keep the best forecast estimate and
carry an explicit uncertainty bound through the safety gate.

## Interpretation

The forecast path now has four separable controls:

- ``forecastLeadTime`` controls when the demand estimate becomes visible.
- ``minForecastLeadTime`` rejects leads too short to settle handovers.
- ``forecastBurstRateError`` tests under- and over-predicted demand magnitude.
- ``forecastBurstRateUncertainty`` inflates the risk gate when the forecast
  magnitude is uncertain.

The next step is to broaden this margin sweep across more traffic profiles and
to compare fixed bounds against an online bound estimated from recent forecast
misses.
