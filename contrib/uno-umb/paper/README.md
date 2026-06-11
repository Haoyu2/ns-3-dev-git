# UNO-UMB Manuscript Draft

This directory contains the first paper scaffold for the UNO-UMB ns-3
prototype.  The draft is intentionally lightweight so it can build with a basic
LaTeX installation while the technical story and results are still moving.

Build:

```bash
make -C contrib/uno-umb/paper
```

The build regenerates `figures/evaluation-tradeoff.tex` from
`data/main-evaluation-summary.csv`.  To refresh only the figure source after
updating the compact result summary:

```bash
make -C contrib/uno-umb/paper figures
```

Current backbone:

- Reactive latent-demand wakeup for unannounced center-burst shifts.
- Calibrated selective forecast margin for anticipated right-edge shifts.
- Actionability-gated forecast margin for right-edge load-grid robustness.
- Compact 5-seed result summary in `data/main-evaluation-summary.csv`.
- Reproducibility notes in
  `contrib/uno-umb/experiments/replication-5seed3run-ideal.md`.
- UE-count/load sweep note in
  `contrib/uno-umb/experiments/load-ue-rate-sweep.md`.
- Forecast actionability-gate note in
  `contrib/uno-umb/experiments/forecast-actionability-gate.md`.

Before external review, decide whether the actionability-gated load-grid result
should move into a main figure, run a forecast-error stress check if time
allows, and replace any placeholder venue-specific formatting with the target
template.
