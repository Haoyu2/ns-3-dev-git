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
- [ ] Tech debt (post-submission, from code review): consolidate the
      duplicated AoiTracker / error models across the two scratches into
      contrib/aoi-twt/model (they have diverged into supersets; touching
      them now would require re-validating all results); optional scratch
      hot-path cleanups (PhyStateTrace stoul, per-frame packet Copy in
      SpFadingErrorModel).
- [x] Full proofs appendix (appendix.tex; `make techreport` -> 7pp version).
- [x] Mixed-MCS experiment: HeMcs0/HeMcs7 alternating, 700B payload —
      33% gain at N=20 (11.2 vs 16.8 ms), 36% at N=50 (24.6 vs 38.6 ms),
      3 seeds, CIs < 0.1ms (mixedmcs.csv; --mixedMcs flag in multista).
- [x] T3 theorem (exact N=2): rational-ratio reduction + quasiconvexity ->
      O(Q) exact algorithm; non-integer ratios strictly optimal sometimes
      (verified vs exhaustive search, 4000 instances). In paper Sec. VII +
      appendix. T2 scoped as journal work (notes).

## External review processed (2026-06-12)

Verdict received: strong 6.5/10 now, 8/10 after fixes; publishable. Venue
guidance: MSWiM 2026 strongest fit (modeling/simulation/WLAN/energy lane;
deadlines reg Jun 14 / sub Jun 23), WCNC 2027 alternative (needs comms
framing, deadline Jul 1), MobiQuitous 2026 weaker fit + 2-day deadline.
Plan: target MSWiM 2026, not MobiQuitous.

Findings actioned:
- [P1 theorem vs implementation] DONE: rewrote the "practice vs analysis"
  paragraph into "Certified vs. practical variant" making the split explicit
  (certified=dyadic rho*=1/4 is what the theorems bound; practical rho*=0.9
  is what we report); tightening the analyzed budget is flagged as open.
- [P1 baseline-light] DONE (analytical near-optimality): added
  scripts/lower_bound.py computing the relaxation lower bound per regime
  (valid LB on any feasible schedule, hence on any heuristic incl. TASPER);
  new LB column in Table II + a "Near-optimality" paragraph (H-G is 5.6%
  above LB in skew vs 17% for equal-interval; 18% vs 36% in fading+skew).
  STILL OPEN: an actual TASPER-style heuristic implementation as a second
  empirical baseline (the LB is the rigorous stand-in for now).
- [P1 mixed-MCS artifact] DONE: removed stray `done50` sentinel from
  mixedmcs.csv; added clean mixed-MCS jobs to fig_data_sweep.py (no
  sentinels); added fig_mixedmcs() to make_figs.py; embedded Fig. 6 and tied
  the 33/36% prose to it.
- [P2 d_max floor mismatch] DONE: scheduler now enforces
  T_i >= max(Tmin_i, d_max) before water-filling (matches eq:relax /
  Lemma 1'); regression-checked — results unchanged (duty floors dominate
  in the studied regimes), tests pass.
- [P2 scope/limitations] DONE: added a "Scope" paragraph to the system model
  (uplink-only, generate-at-will, scheduled non-overlap, per-SP fading;
  random arrivals + downlink flagged as extensions).

Conference draft now 6pp, tech report 7pp, both 0 errors / 0 overfull.

## Third review + repo/page (2026-06-12)

- TEMPLATE: switched IEEEtran -> ACM acmart (sigconf, nonacm). Real authors
  added: Haoyu Wang + Bo Sheng (UMass Boston), Zerin Shaima Meem + Xiaoqian
  Zhang (Univ Nebraska Omaha). Continuous theorem numbering (ACM standard);
  appendix literal cross-refs converted to \ref. ACM-Reference-Format bib.
- CLARIFICATIONS expanded: queue gating now describes freezing per-AC EDCA
  queues via the PS block-reason hook (preserves seqno/retry/BA state);
  conclusion adds a paragraph on WHY rho*=1/4 is conservative (two serial
  factor-2 losses) + suggests amortized packing analysis as next step.
- REFERENCES: 11 -> 35, all real/verified. Expanded Related Work into 5
  thematic paragraphs (AoI foundations, AoI scheduling/Whittle, energy AoI,
  TWT+Wi-Fi 7/802.11be, periodic real-time/pinwheel scheduling). TASPER
  updated to its published IEEE TMC 2026 version.
- ARTIFACT REPO + PAGE: new public repo github.com/Haoyu2/age-optimal-twt
  (ns-3 module + src/wifi patch + scratch + scripts + both PDFs + primer),
  live GitHub Pages at https://haoyu2.github.io/age-optimal-twt/ (academic
  landing page). Link added to paper (Sec VIII footnote). All Pages assets
  verified 200. Conf 7pp / techreport 8pp.
- NOTE: chose PUBLIC + non-anonymous (real names in paper). If MSWiM turns
  out double-blind, make repo private (`gh repo edit Haoyu2/age-optimal-twt
  --visibility private`) and anonymize page/paper.
- camera-ready TODOs: verify TASPER vol/pages; verify twtsurvey25 author
  list/volume; confirm BibTeX venue once accepted.

## Second review processed (2026-06-12, pre-MSWiM)

Reviewer: strongly positive, best-paper-track; 4 substantive + 2 formatting +
artifact question. All actioned:
- [1 certified/practical gap] DONE: added the *why* — rho*=1/4 absorbs two
  serial factor-2 losses (dyadic duration rounding doubles density; packing
  needs U<=1/2), explained in the certified-vs-practical paragraph.
- [2 queue-gating mechanism] DONE: one sentence — we BLOCK the MAC transmit
  queues at SP end (existing power-save block), not drop frames; updates wait
  and tx at next SP.
- [3 octave quantization example] DONE: concrete example at introduction
  (ideal periods 30 & 50 ms, ratio 1.67<2, round to same grid level).
- [4 N=2 theorem] DONE: dropped "informal", added the quasiconvex cost
  J(r,q)=(a1+a2 r)max(...) and an explicit "Full proof: appendix" pointer.
- [5 artifact/open-source Q] DONE: footnote in Sec VIII — module + scheduler
  + scripts released as open source for artifact eval (anonymized).
- [ref 7 TASPER] DONE: now published, updated to IEEE Trans. Mobile Computing
  2026 (vol 25/3, pp 3469-3487; flagged to verify vol/pages at camera-ready).
- [ref 8 Rajendran] checked — still arXiv preprint, left as-is.
- [variable spacing] checked $4/\ln 2$ etc. render fine; no change needed.
Conf 6pp / techreport 8pp, clean. UNCOMMITTED.

## LaTeX status (2026-06-11)
contrib/aoi-twt/paper/: main.tex (4pp draft, compiles 0 errors/0 warnings,
all sections drafted incl. theorem statements + proof sketches), refs.bib,
Makefile, scripts/{run_sweep,make_macros,make_pareto}.py, README.md.
Experiments executed remotely on frcc (32 cores).
