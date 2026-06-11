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
- Selective overlap-gated forecast margin for anticipated right-edge shifts.
- Compact result summary in `data/main-evaluation-summary.csv`.
- Reproducibility notes in
  `contrib/uno-umb/experiments/expanded-sweep-control-plane-audit.md` and
  `contrib/uno-umb/experiments/right-edge-ideal-margin-comparison.md`.

Before external review, add the literature references, update the limitations
section after the next broader sweep, and replace any placeholder venue-specific
formatting with the target template.
