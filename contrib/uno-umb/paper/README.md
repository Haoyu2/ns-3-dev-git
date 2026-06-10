# UNO-UMB Manuscript Draft

This directory contains the first paper scaffold for the UNO-UMB ns-3
prototype.  The draft is intentionally lightweight so it can build with a basic
LaTeX installation while the technical story and results are still moving.

Build:

```bash
make -C contrib/uno-umb/paper
```

Current backbone:

- Reactive latent-demand wakeup for unannounced center-burst shifts.
- Selective overlap-gated forecast margin for anticipated right-edge shifts.
- Main quantitative table in
  `contrib/uno-umb/experiments/main-paper-table-selective.md`.

Before external review, add the literature references, update the limitations
section after the next broader sweep, and replace any placeholder venue-specific
formatting with the target template.
