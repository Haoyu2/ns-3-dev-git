# Paper Outline: AoI-Optimal Target Wake Time Scheduling

Working title: "Age-Optimal Target Wake Time: Provably Good Wake Schedules
for Energy-Constrained Wi-Fi Status Updating"

Primary target: IEEE ICC 2027 (deadline ~Oct 2026; per-symposium best paper).
Stretch: INFOCOM 2027 if the theory core is polished in time.
Companion paper: ICNS3 2027 — "A Target Wake Time mechanism for ns-3" (the
TwtPowerSaveManager + validation methodology + protocol lessons; upstreamable).

## One-paragraph pitch

TWT lets an AP schedule when each station wakes; all prior schedulers optimize
energy or throughput with freshness at best a constraint (TASPER) and offer no
guarantees. We design the wake schedule itself for information freshness:
minimize weighted Age of Information across stations subject to per-station
energy budgets, where the decision variables are the TWT triples (period,
offset, SP duration). We give a renewal-exact AoI model validated against
802.11ax packet-level simulation to <1%, a scheduling algorithm with a
constant-factor guarantee (the first for this problem), a matching structural
counterexample showing non-preemptive harmonic packing fundamentally differs
from preemptive utilization bounds, and an open ns-3 implementation in which
the algorithm beats the natural baseline by 10-12% exactly in the regimes the
theory predicts.

## Section plan (mapped to existing assets)

1. Introduction
   - Status updating over Wi-Fi 6/7; TWT as the freshness-energy control
     surface; gap: no freshness-objective TWT scheduling with guarantees.
   - Contributions list (model, lemma k*=1, algorithm, 8/5.77/12-approx,
     CE1 impossibility structure, validated open implementation, regime map).

2. Related work [from RESEARCH-PLAN boundary table]
   - TASPER (AoI as constraint, heuristic); Bedewy et al. (CSMA sleep-wake,
     continuous rates); age-agnostic cyclic schedulers (no energy/sleep);
     Whittle-index AoI line; TWT implementations/measurements (WNS3 24).

3. System model [THEORY-NOTES 1, 4e]
   - TWT agreements, generate-at-will sampling at SP start, per-SP block
     fading (coherence >= SP; justify: in-SP retries share fate; per-attempt
     i.i.d. is the degenerate case), energy = awake time, weighted average
     AoI objective.

4. Single-station structure [THEORY-NOTES 2]
   - Renewal-reward closed form E[A] = delta + T(1/p - 1/2) (+ peak).
   - Lemma (k*=1 without wake overhead; overhead is load-bearing): batching
     exists only because of c_w. Energy-tight SP sizing.
   - Validation anchor table (errors 0.007-1.2%). [scratch/twt-aoi-validation]

5. The multi-station problem and its structure
   - Formulation; Lemma 1' (any feasible schedule has T_i >= d_max);
     relaxation lower bound (Lemma 1) and water-filling form (Lemma 2).
   - CE1 impossibility: density 1 infeasible non-preemptively — contrast
     with preemptive harmonic RM optimality; this motivates the granularity
     condition. [THEORY-NOTES 3a correction block]

6. Algorithm: HARMONIC-GREEDY
   - Relaxation -> harmonic rounding with candidate-argmin anchor ->
     post-rounding SP re-expansion (gated on p<1) -> SF-BF packing ->
     best-of-uniform safeguard. Pseudocode. Octave-quantization discussion
     (when and why it collapses to uniform).

7. Guarantees [THEORY-NOTES 3a]
   - Packing: staircase lemma, count invariant (tight 2^j), packing lemma
     d' <= t0(1-U) (+ tightness remark R2).
   - Theorem 9: 8-approx under (A2'); Corollary 10: 4/ln2 ~= 5.77 via
     candidate-argmin anchor (deterministic); Theorem 11: unconditional 12,
     6/ln2 ~= 8.66 with anchor argmin.
   - All lemmas machine-checked on randomized instances (mention; artifact).

8. Implementation [src/wifi TwtPowerSaveManager + contrib/aoi-twt]
   - PowerSaveManager-subclass design; AnchorTime (TSF-like grid); queue
     gating at SP boundaries (REQUESTED vs GRANTED distinction);
     announced vs unannounced trade-offs. Protocol findings sidebar:
     beacon-loss, DL deadlock/asymmetric BA, unACKable doze announcements,
     retry-storm overrun, PM-null wedge (upstream bug report). [lessons a-f]

9. Evaluation [scratch/twt-aoi-multista]
   - Validation: model vs sim <1% across regimes.
   - Regime map table (parity/10.0%/0.8%/11.7%) + scaling (N=25, 50).
   - Practice-vs-analysis: dyadic variant collapses to uniform under the
     conservative budget; implemented budget wins; the gap as open theory.
   - Energy exactness: duty == d/T to 4 decimals in all runs.

10. Conclusion + future: index policy asymptotics (T2), N=2 exact (T3),
    AP-side schedule-aware delivery, negotiation (TWT Setup frames).

## Figures (planned)
F1 mechanism/timeline diagram (SPs, doze, announce);
F2 single-station validation: measured vs predicted AoI across (T, p) grid;
F3 CE1 illustration;
F4 algorithm pipeline;
F5 regime map (bar chart: gain vs regime);
F6 AoI-energy Pareto frontier (TODO: sweep budgetScale);
F7 scaling (N on x-axis).

## TODO list to paper-ready
- [x] Pareto frontier sweep (budgetScale x scheduler) — F6
      (paper/scripts/run_sweep.py, run on frcc; make_pareto.py).
- [x] Seed sweeps + confidence intervals (10 seeds main regimes, 5 Pareto,
      3 validation; make_macros.py -> results_macros.tex consumed by main.tex).
- [x] (A2) removal (Lemma 1' + Theorem 11; in paper as thm:uncond).
- [x] Peak-AoI variant (Proposition in paper + notes; peak measured in sim).
- [x] Artifact packaging (contrib/aoi-twt/paper/ with README, scripts,
      Makefile; LaTeX compiles clean).
- [ ] Upstream the PM-null bug report (independent task already spawned).
- [x] Bib author lists completed (Busacca et al. TASPER; Gamgam/Akar/Ulukus;
      Rajendran/Agnihotri; Mozaffariahrar/Menth/Avallone).
- [x] Figures: F1 mechanism (TikZ), F2 validation grid (3 seeds, CI),
      F3 CE1 (TikZ), F5 regime bars, F6 Pareto, F7 scaling (N=10..50).
      F4 (pipeline) intentionally covered by Algorithm 1 pseudocode.
      Data: valgrid.csv, scaling.csv (frcc); plots via scripts/make_figs.py.
- [ ] De-anonymize / author block before submission (user action).
- [x] Full proofs appendix (appendix.tex; `make techreport` -> 7pp version).
- [x] Mixed-MCS experiment: HeMcs0/HeMcs7 alternating, 700B payload —
      33% gain at N=20 (11.2 vs 16.8 ms), 36% at N=50 (24.6 vs 38.6 ms),
      3 seeds, CIs < 0.1ms (mixedmcs.csv; --mixedMcs flag in multista).
- [x] T3 theorem (exact N=2): rational-ratio reduction + quasiconvexity ->
      O(Q) exact algorithm; non-integer ratios strictly optimal sometimes
      (verified vs exhaustive search, 4000 instances). In paper Sec. VII +
      appendix. T2 scoped as journal work (notes).

## LaTeX status (2026-06-11)
contrib/aoi-twt/paper/: main.tex (4pp draft, compiles 0 errors/0 warnings,
all sections drafted incl. theorem statements + proof sketches), refs.bib,
Makefile, scripts/{run_sweep,make_macros,make_pareto}.py, README.md.
Experiments executed remotely on frcc (32 cores).
