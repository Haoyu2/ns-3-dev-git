# Theory Notes: AoI under TWT Schedules (working draft)

Companion to `RESEARCH-PLAN-aoi-twt.md`. Math here is sketch-level; to be made
rigorous in the paper. Notation: ASCII only per project style.

## 1. Model

- N stations, one AP. Station i holds a TWT agreement (T_i, phi_i, d_i):
  wake interval (period) T_i, offset phi_i, SP duration d_i. Station sleeps outside
  its SPs; inside an SP it can transmit one (or more) status updates uplink.
- Sampling: generate-at-will — station samples its source at the moment it wakes
  (fresh sample at SP start). Extension later: random arrivals (sample available
  with prob. q per period, or Poisson sources with buffering).
- Channel: each in-SP transmission attempt succeeds i.i.d. with prob. p_i
  (captures MCS/distance; retransmissions within the SP allowed up to SP capacity
  k_i = floor(d_i / s_i), s_i = per-attempt airtime).
- Energy: per period, station spends e_a * d_i (awake) + e_s * (T_i - d_i) (sleep).
  Budget: average power <= P_i  <=>  d_i (e_a - e_s) / T_i <= P_i - e_s
  => duty-cycle constraint d_i / T_i <= rho_i where
  rho_i := (P_i - e_s) / (e_a - e_s).
- AoI of source i at AP: A_i(t), sawtooth; jumps down to (transmission delay) on
  successful delivery, grows at unit rate otherwise.

## 2. Single-station renewal-reward (warm-up; simulator validation anchor)

Successful periods form a renewal process. Let p be the prob. that at least one
attempt in the SP succeeds: p_succ = 1 - (1 - p)^k for k attempts of success prob p.
Number of periods between successful deliveries: M ~ Geometric(p_succ),
E[M] = 1/p_succ.

With generate-at-will sampling at SP start and delivery within the SP (delay <= d,
treat as delta := mean in-SP delivery delay), the age at a successful delivery
resets to approximately delta, then grows for M periods until the next success.

Time-average AoI (standard renewal-reward over cycles of length M*T):

    E[A] = delta + T * ( E[M^2] / (2 E[M]) + 1/2 ) - T/2 ... (to re-derive carefully)

For Geometric(p_succ): E[M] = 1/p_succ, E[M^2] = (2 - p_succ)/p_succ^2, giving

    E[A] ~= delta + T * (1/p_succ - 1/2)

Sanity checks: p_succ = 1 => E[A] ~= delta + T/2 (classic periodic sampling);
p_succ -> 0 => blows up like T/p_succ. Matches intuition.

Peak AoI: E[A_peak] ~= delta + T / p_succ.

### Energy-optimal single station

minimize_{T, d}  delta + T (1/p_succ(d) - 1/2)   s.t.  d/T <= rho.

For fixed d, objective increasing in T => constraint tight: T = d/rho. Then minimize
over d (equivalently over k = floor(d/s)):

    f(k) = (k s / rho) * ( 1/(1 - (1-p)^k) - 1/2 )

Discrete, unimodal in k (to prove; ratio test on f(k+1)/f(k)). Optimal k* balances
"longer SP costs proportionally longer period under the energy budget" against
"more retries per wake". For p -> 1, k* = 1: wake briefly and often. For small p,
k* > 1: batching retries beats more frequent short wakes (since each wake re-pays
the period cost). Closed-form threshold on p for k*=1 vs k*>1 is a nice lemma.

**LEMMA (proved 2026-06-11, discovered via failing unit test):** with zero per-wake
overhead, k* = 1 for EVERY p in (0,1]:

    f(k+1) - f(k) at k=1:  f(2) - f(1) = p / (2 (2 - p)) > 0
    (general k: g(k) = k/(1-q^k) is strictly increasing in k for q in (0,1),
     and the -k/2 correction never compensates — write out the full proof.)

So batching retries is NEVER optimal without overhead — my earlier guess ("small p
=> k* > 1") was wrong. Corollary: the per-wake fixed overhead c_w (radio rampup,
beacon sync; real and present in the 802.11 energy model) is exactly what makes the
problem TWT-specific vs. abstract sampling models. With c = c_w/s the objective is

    f(k) = (k + c) * (1 / (1 - (1-p)^k) - 1/2)

and k* > 1 appears for moderate c even at high p. KEEP c_w IN THE MODEL — it is
load-bearing for every batching result.

**ALGORITHMIC NOTE (from first implementation, contrib/aoi-twt):** harmonic
rounding-up of periods can nearly double a station's AoI (worst case factor 2 loss).
Fix implemented and validated: *post-rounding SP re-expansion* — rounding T up
leaves energy (duty <= rho) and density (d/T_ideal share) slack; spend it on extra
in-SP attempts, which at fixed T strictly increases pSucc at zero AoI cost. Cap
d' <= min(d * T'/T_ideal, rho * T'). In the 10-station example this turned a 22%
LOSS vs the equal-interval baseline into a 14% win (204.2 -> 175.4 ms weighted AoI).
The paper's approximation-factor proof should bake this step in — it tightens the
constant.

**MULTI-STATION FINDINGS (2026-06-11, scratch/twt-aoi-multista.cc, end-to-end with
real TWT mechanism):**

1. *Grid anchor optimization is necessary, not optional.* With loose energy budgets
   (density 0.52) the naive min-period anchor made harmonic-greedy LOSE to
   equal-interval (41.6 vs 39.7 ms weighted AoI). Choosing the anchor among the
   ideal periods folded into (tmin/2, tmin] (each candidate makes one station's
   rounding lossless, evaluate weighted-AoI estimate) recovered exact parity —
   it found the uniform all-75ms schedule. IMPLEMENTED in
   HarmonicGreedyTwtScheduler.
2. *SP re-expansion must be gated on pSucc < 1* — at p=1 it spent the rounding
   slack on useless extra attempts (pure energy waste). IMPLEMENTED.
3. *Octave quantization limit (THEORY-RELEVANT):* a power-of-two harmonic grid can
   only differentiate stations whose ideal periods differ by >= 2x. Weight ratios
   w_max/w_min < 4 (period ratio sqrt(w) < 2) collapse to a single grid level ->
   harmonic-greedy == equal-interval in homogeneous populations even when the
   density constraint binds (verified: 20.90 vs 21.49 ms at density 1.04). The
   algorithm differentiates (and should win) only with: weight skew >= 4x,
   heterogeneous SP sizes (different MCS/payload), or lossy channels (k* batching
   diversifies d_i). These are the experiment regimes for the paper — and the
   approximation analysis should make the octave-loss explicit (factor <= 2 on
   periods == factor <= 2 on AoI, tight exactly when ratios are just below a
   power of two).
4. The end-to-end pipeline works: scheduler output -> per-STA TwtPowerSaveManager
   (AnchorTime grid) -> disjoint SPs on the air, all duty budgets exactly met,
   per-STA AoI within ~8% of the rough prediction (4ms delay constant), zero
   contention. Multi-STA startup required WifiStaticSetupHelper (static assoc +
   static BA + PM alignment) — simultaneous in-band PM transitions deadlock
   (see protocol lesson f).

5. DIFFERENTIATING-REGIME RESULTS (300s runs, 10 stations, weighted mean AoI,
   equal-interval vs harmonic-greedy with anchor opt + best-of-uniform):

   | Regime                                   | Equal  | Harmonic | Gain  |
   |------------------------------------------|--------|----------|-------|
   | loose budgets, p=1, weights 1..3         | 39.65  | 39.65    | 0%    |
   | tight (x2), p=1, weights 1..3            | 20.90  | 21.49*   | ~par  |
   | tight, p=1, weight skew 8x (E1)          | 20.90  | 18.82    | 10.0% |
   | tight, per-SP fading .15-.6, w 1..3 (E2) | 43.51  | 43.17    | 0.8%  |
   | tight, fading + skew 8x (E3)             | 51.18  | 45.20    | 11.7% |
   (* pre-best-of run; safeguard now returns the uniform schedule there)

   SCALING (200s, skew 8x, x2 budgets): N=25 -> both 39.65 ms; N=50 -> both
   77.14 ms (predicted 79.00). At these populations the schedule-capacity
   constraint dominates (sum rho >> 1), the periods become capacity-bound
   (T ~ N d), and the best-of safeguard correctly detects that the sqrt-law
   gain for a 2-class weight mix ((E sqrt w)^2 / E w ~ 18%) is below the
   harmonic rounding cost — so ALG = uniform, never worse. Pipeline runs
   clean at N=50: exact duty cycles, zero contention, predictions within 2%.
   Wins at scale require heterogeneous SP sizes (mixed MCS/payload) — paper
   TODO.

   ANALYSIS-VARIANT COMPARISON (DyadicReservations + DensityTarget 0.25,
   i.e. the algorithm covered by Theorems 9/11): E1 -> 20.90 ms,
   E3 -> 51.02 ms — in both regimes the conservative density budget makes
   the best-of-uniform safeguard fire, so the analyzed variant returns the
   uniform schedule (= equal-interval baseline) while the implemented
   variant (density 0.9, true-duration greedy) wins by 10-12%. Expected and
   honest: the theorem certifies the worst case; practice uses the
   aggressive budget. The gap *is* the journal-version question (gap 6).

   Reading: the algorithm wins exactly when per-station ideal periods spread
   beyond an octave (weight skew >= 4x, or skew x channel heterogeneity
   combining); otherwise it detects the collapse and returns uniform. Matches
   the octave-quantization analysis. Two more scheduler fixes implemented from
   E2: best-of-uniform safeguard (rounding loss had made harmonic WORSE than
   uniform under fading: 47.9 vs 43.5); and the mid-exchange doze bug found by
   E2's 3ms SPs (sleep guard now distinguishes REQUESTED access, which must
   not block dozing, from GRANTED, which must complete first — old guard was
   the opposite extreme and caused the FEM m_sentFrameTo assert).

   Per-SP fading at multi-station scale uses SpFadingErrorModel (per-source
   verdicts; source recovered from the IPv4 header inside the MSDU; gap-based
   SP detection is sound BECAUSE queue gating guarantees inter-SP silence).
   Under block fading, scheduler input must be pSucc = 1 - e with
   MaxAttemptsPerSp = 1 (batching cannot beat block fading).

   f. NEW protocol lesson (upstream bug, REPORT TO ns-3): a non-QoS Data Null in
      an EDCA queue is not granted channel access again after a missed ack -> a
      single collision permanently wedges the legacy PM transition of a QoS STA
      (StaWifiMac::SetPowerSaveMode), and synchronized PmModeSwitchTimeout +
      lifetime timers phase-lock many STAs into colliding forever (reproduced:
      7/10 STAs never entered PS). Candidate fix — send a QoS Null (TID 0)
      instead — fixes the wedge but breaks the announced TWT toggling path
      (AP-side CAM assert: ack-policy/BA-agreement interaction with QoS nulls
      under an established TID-0 agreement); REVERTED for now, needs a careful
      upstream patch (probably QoS Null + explicit NormalAck policy + BA window
      exemption). We sidestep it entirely: multi-station experiments use
      WifiStaticSetupHelper (no in-band PM transitions) + unannounced mode.
   g. Seed-sweep-revealed race (FIXED in TwtPowerSaveManager): allowing doze
      during WIFI_PM_SWITCHING_TO_PS before the SP cycle is anchored lets a
      beacon-triggered SleepIfIdle strand the pending PM Null (access only
      REQUESTED, not GRANTED) -> STA sleeps forever, zero deliveries (hit at
      RngRun=2 in validation). Fix: doze during SWITCHING_TO_PS only once
      m_cycleRunning. Single-seed validation hid this completely; the seed
      sweep caught it immediately.

   SEED-SWEEP RESULTS (frcc, 10 seeds main / 5 Pareto / 5 validation):
   validation p=1: 0.003% all seeds; lossy: mean ~1%, max 4.4% (geometric
   tail at 300s). Regimes (mean +- 95% CI): skew 18.82+-0.00 vs 20.90+-0.00
   (10.0%); fadingskew 44.68+-0.28 vs 51.26+-0.32 (12.8%, firmed up from
   single-seed 11.7%); fading-only: statistical tie (43.51 vs 43.42);
   dyadic variant == equal-interval in both skew regimes. PARETO (skew, vs
   budgetScale): harmonic dominates the whole energy-constrained range —
   14.2% at 0.5 (66.16 vs 77.15 ms), 13.9% at 1.0 (34.15 vs 39.65), 10.0%
   at 2.0, converging once energy stops binding (>=3). Peak-AoI gains match
   (10.5%/12.3%/14.5%) per the peak proposition. In the skew regimes the
   harmonic schedule also uses LESS energy (duty 0.075 vs 0.080):
   Pareto-dominant, not a trade.

## 2b. STRONG-BASELINE FINDING (2026-06-12) — what AoI-weighting actually buys

Added EnergyGreedyTwtScheduler: a credible TASPER-style heuristic that gives
each station its most aggressive (shortest) budget-feasible period
T_i = max(d_i/rho_i, d_max), uniformly stretches to just-feasible density 1
if overbooked, then reuses our harmonic rounding + greedy packing + uniform
safeguard. It differs from HarmonicGreedy ONLY in lacking the AoI-weighted
sqrt-law period assignment.

KEY (uncomfortable but important) FINDING: against this STRONG baseline, our
method does NOT win in the originally-reported heterogeneous-budget regimes
(single-seed 60-80s):
- skew p=1, budgetScale=2: Energy 19.11, Harmonic 18.82 (~tie; both beat
  equal-interval 20.90)
- fading+skew, budgetScale=2: Energy 44.19, Harmonic 45.04 (Energy slightly
  AHEAD)
Reason: when per-station ENERGY FLOORS (T_i >= d_i/rho_i) already pin the
periods, "minimize all periods then pack" is near-AoI-optimal; the sqrt-law
adds little. Our earlier 10-12% "wins" were vs the NAIVE equal-interval
baseline, and much of that gain is just PERIOD DIVERSITY, which EnergyGreedy
also achieves -- NOT AoI-weighting per se.

THE REGIME THAT ISOLATES AoI-WEIGHTING (uniform budgets -> EnergyGreedy
collapses to equal-interval, since uniform rho gives uniform energy-tight
periods; only the sqrt-law redistributes the binding schedule density toward
high-weight / lossy stations):
- uniformBudget=0.12 (density 1.2, binding), skew, p=1: Equal=Energy 17.15,
  Harmonic 16.04 -> 6.5% win, AoI-weighting the SOLE differentiator.
- uniformBudget=0.12, fading+skew: Equal=Energy 41.50, Harmonic 39.44 ->
  5% win (channel-aware via a_i = w_i(1/p_i - 1/2)).
- non-binding (density<=0.9) or over-binding (>=1.6 capacity floor): all tie
  (no slack to redistribute, resp. everyone pinned at floor). Correct.

IMPLICATION FOR THE PAPER (honest reframing, STRONGER not weaker):
1. Report THREE schedulers: equal-interval (naive), EnergyGreedy (strong
   period-diverse heuristic), Harmonic-Greedy (ours).
2. The headline isolating experiment becomes uniform-budget binding-density:
   ONLY Harmonic-Greedy beats both baselines, by ~5-6.5%, exactly where the
   theory says AoI-weighting matters.
3. In heterogeneous-budget regimes, state plainly that EnergyGreedy ties us
   (energy floors pin periods) -- this is a precise characterization, the
   kind reviewers reward, and it is backed by the lower-bound column
   (both Energy and Harmonic sit near LB there).
4. Re-examine the 36% mixed-MCS claim: heterogeneous SP sizes make periods
   diverse, so EnergyGreedy likely captures most of it too -> that win is
   PERIOD DIVERSITY, must be re-attributed (pending EnergyGreedy mixed-MCS
   run). Do NOT claim it as an AoI-weighting win.

RESOLVED (2026-06-12): mixed-MCS DECOMPOSES cleanly (single-seed):
N=50: equal 38.54 -> EnergyGreedy 29.52 (period diversity, +23%) ->
HarmonicGreedy 24.61 (AoI-weighting, +17% more, +36% total). N=20:
16.79 -> 13.28 -> 11.22. So we beat the STRONG baseline by 15-17% in
mixed-MCS (binding density + diverse airtimes -> both ingredients active).
Paper reframed: report all 3 schedulers; uniform-budget binding regime is
the clean AoI-weighting isolator (5-6.5% over both baselines); mixed-MCS is
the decomposition (period diversity + AoI-weighting both contribute, we win
outright); het-budget regimes honestly stated as ties with EnergyGreedy
(energy floors pin periods). LB delta lowered to 2ms (provable lower bound:
generation is 2ms before SP) so LB <= measured in every regime. Full
3-scheduler + ubind + ubindfade sweep running on frcc (sweep2). This makes
the paper MORE rigorous: precise attribution + a strong baseline that ties
us elsewhere is the kind of self-critical evaluation reviewers reward.

FINAL FULL-SWEEP NUMBERS (10 seeds, frcc sweep3, folded into paper):
- het.budget skew: Equal 20.9 / Energy 19.1 / H-G 18.8 / LB 15.8 (Energy ~ H-G)
- het.budget fade+skew: 51.3 / 45.1 / 44.7 / 35.7 (tie)
- UNIF budget skew: Equal 17.2 = Energy 17.2 / H-G 16.0 / LB 15.2 (6.5%, isolated)
- UNIF budget fade+skew: 41.6 = 41.6 / 39.8 / 35.9 (4.3%, channel-aware)
- mixed-MCS N=50: 38.5 -> 29.5 (Energy +23% diversity) -> 24.6 (H-G +36%)
Uniform-budget rows: Energy ≡ Equal EXACTLY (17.15=17.15, 41.59=41.59) — cleanest
possible demo that AoI-weighting is the sole differentiator there. Paper rebuilt:
6pp conf / 7pp techreport, 0 err / 0 overfull, all 3 schedulers in Figs 5/6/7,
Table II het/unif split. THIS REVISION IS UNCOMMITTED (user commits).

## 3. Multi-station structure (the real problem)

Decision: {(T_i, phi_i, d_i)}_{i=1..N} such that SPs do not overlap (or share with
bounded contention), minimizing sum_i w_i E[A_i] s.t. duty-cycle budgets.

Three nested difficulties:

1. **Period assignment**: choosing {T_i} — weighted AoI suggests T_i proportional to
   sqrt-type rules (cf. square-root laws in cyclic AoI scheduling, arXiv:2311.18791
   has the no-energy analog). With energy: T_i >= d_i/rho_i couples in.
2. **Offset packing (feasibility)**: given {(T_i, d_i)}, do offsets {phi_i} exist
   with disjoint SPs? This is exactly a pinwheel/periodic-maintenance feasibility
   problem — NP-hard in general; restrict to harmonic periods (T_i in {2^j * T_0})
   => feasibility is a simple density condition sum_i d_i/T_i <= 1 and packing is
   constructive (binary-tree/TDMA-tree assignment). Harmonic restriction costs at
   most a constant factor (<= 2) in AoI — prove via rounding argument. THIS is the
   approximation-guarantee skeleton: round optimal fractional periods to powers of
   two, pack greedily, bound the loss.
3. **Stochastics**: with random arrivals / failures, per-SP success couples stations
   only through the schedule; given the packing, stations decouple => the relaxed
   problem is separable. Lagrangian on the density constraint sum d_i/T_i <= 1 plays
   the role of the Whittle relaxation; "index" = marginal AoI reduction per unit
   density. Candidate result: index policy asymptotically optimal as N -> infinity
   with density held fixed.

Headline theorem targets (in descending ambition):
- T1: constant-factor approximation for weighted average AoI with energy budgets via
  harmonic rounding + greedy packing (deterministic arrivals, lossy channel).
  DRAFT PROOF BELOW (Section 3a) — 4-approximation, fully chained.
- T2: density-Lagrangian index policy, asymptotic optimality (stochastic case).
- T3: exact solution for N=2 (mirrors the 2-source closed form of 2311.18791 but
  with energy + SP overhead) — good warm-up paper section.

## 3a. T1 draft: a 4-approximation (2026-06-11)

### Setup

Station i has SP length d_i > 0 (chosen upstream: d_i = k_i s_i + c_i), per-SP
success probability p_i in (0,1], weight w_i, duty budget rho_i in (0,1], hence
minimum period Tmin_i = d_i / rho_i. A schedule assigns (T_i, phi_i) with pairwise
disjoint SPs [phi_i + m T_i, phi_i + m T_i + d_i). By the renewal formula the cost
is J(T) = sum_i a_i T_i + C where a_i := w_i (1/p_i - 1/2) > 0 and C collects the
T-independent reset terms. We compare against OPT = the minimum cost over ALL
feasible (not necessarily harmonic, not necessarily periodic-aligned) schedules.

Standing assumptions for this draft (mild; general case deferred):
(A1) rho_i <= 1/2 for all i (duty cycles below 50%).
(A2) d_max := max_i d_i <= tmin := min_i T_i^* where T^* is the relaxation
     solution defined below. (Typical: SPs of ms scale, periods of 10-100 ms.)

### Lemma 1 (relaxation lower bound)

Any feasible schedule has pairwise disjoint SPs, hence density U = sum_i d_i/T_i
<= 1, and T_i >= Tmin_i. Therefore

    OPT >= J* := min { sum_i a_i T_i : sum_i d_i/T_i <= 1, T_i >= Tmin_i } + C.

The program is convex (substitute x_i = 1/T_i). KKT gives the water-filling form

    T_i(lambda) = max( Tmin_i, sqrt(lambda d_i / a_i) ),

with lambda = 0 if sum_i rho_i <= 1, else the unique lambda making the density
equal 1 (monotone in lambda; bisection). This is exactly
HarmonicGreedyTwtScheduler::AssignPeriods.

### Lemma 2 (density scaling)

Let T^*(rho) denote the relaxation solution with density target rho <= 1. Then
J(T^*(1/2)) <= 2 J(T^*(1)). Proof: 2 T^*(1) is feasible for target 1/2 (density
halves, Tmin still met), and costs 2 J(T^*(1)); the optimum for target 1/2 can
only be cheaper.

### Lemma 3 (dyadic rounding)

Fix the grid G(t0) = { t0 2^j : j >= 0 }. Rounding each T_i up to G(t0) with
t0 <= tmin multiplies each period by < 2, hence the cost by < 2, and only
decreases density. Likewise, rounding each d_i up to the dyadic divisors
{ t0 / 2^m : m >= 0 } of t0 multiplies each d_i by < 2 (possible by (A2):
d_i <= t0), hence at most doubles the density, and only helps feasibility of
the energy constraint check d'_i / T'_i <= 2 d_i / T_i <= 2 rho_i <= 1 by (A1).
[Note: duty constraint with rounded d' is violated by up to 2x; the SCHEDULE
only needs the original d_i on the air -- the rounded d'_i is used for PLACEMENT
(the station occupies a reserved dyadic slot of length d'_i but only transmits
d_i). Energy is governed by actual awake time d_i, so no violation. Make this
explicit in the paper.]

### IMPORTANT CORRECTION (2026-06-11, found while writing the proof)

The packing lemma as first drafted ("harmonic periods + dyadic lengths +
density <= 1 implies packable") is FALSE. Counterexample (CE1):

    t0 = 1.  A1: T = 1, d = 1/2.   A2: T = 1, d = 1/4.   B: T = 2, d = 1/2.
    Density U = 1/2 + 1/4 + (1/2)/2 = 1.

A1 and A2 repeat in EVERY base window, so each unit window has occupied
measure 3/4 and free measure 1/4 (some set S of measure 1/4, identical in
every window by periodicity). B's SP is a contiguous interval of length 1/2;
it meets at most two consecutive windows, as a suffix [x,1) of one and a
prefix [0, x - 1/2 mod 1) of the next; both pieces must lie inside the SAME
free set S, so |S| >= 1/2 > 1/4. Contradiction -- infeasible for EVERY
placement (aligned or not). Hence density alone can never suffice; a
granularity condition tying SP lengths to the residual capacity of the base
window is NECESSARY. The corrected lemma below states exactly such a
condition and it is essentially tight (Remark R2).

### Packing setup (rigorous)

Fix a unit u > 0 and let t0 = 2^L u for some integer L >= 0. All quantities
below are integer powers of two times u.

- A *block of size s* (s = 2^k u, k >= 0) is an interval [r s, (r+1) s),
  r a nonnegative integer ("aligned").
- Periods: T_i = t0 2^{j_i}, j_i >= 0; call j_i the *level* of station i.
  Let J = max_i j_i and W_j = t0 2^j (the level-j window).
- Reserved lengths: d'_i = 2^{k_i} u <= t0.
- Density: U = sum_i d'_i / T_i.
- A *placement* of station i is a block of size d'_i inside [0, W_{j_i});
  station i then occupies that block translated by every multiple of T_i.
  Two stations are *compatible* if their occupied sets are disjoint.

Algorithm SF-BF (small-first best-fit). Process levels j = 0, 1, ..., J in
increasing order; stations within a level in arbitrary order. Maintain the
free set F restricted to the current level window [0, W_j): when moving from
level j to j+1, the free set is replaced by its two periodic copies in
[0, W_{j+1}) (valid because everything placed so far has period dividing
W_j, hence is W_j-periodic, hence W_{j+1}-periodic). To place a request of
size q: among free blocks of size >= q, pick one of MINIMUM size (best fit;
ties arbitrary), carve [b, b+q) from its left end b, and return the rest of
the block to F as the canonical staircase (Lemma 4). Fail if no free block
has size >= q.

Throughout, F is a finite union of pairwise disjoint blocks; we maintain it
as an explicit collection of blocks (never merging), and `count(s)` denotes
the number of blocks of size s currently in the collection.

### Lemma 4 (staircase carving)

Let B = [b, b + s) be a block of size s and q <= s a power-of-two size.
Then [b + q, b + s) is the disjoint union of exactly one block of each size
q, 2q, 4q, ..., s/2 (empty union if q = s), placed in this order from left
to right; each is aligned.

Proof. b is a multiple of s, hence of every power of two <= s. Write
s - q = q (2^M - 1) with 2^M = s/q, i.e. s - q = q + 2q + ... + (s/2 - ...
precisely sum_{l=0}^{M-1} 2^l q. Define the intervals I_l = [b + 2^l q,
b + 2^{l+1} q) for l = 0, ..., M-1. They are disjoint, their union is
[b + q, b + s), and I_l has size 2^l q with left endpoint b + 2^l q, a
multiple of 2^l q (since b is a multiple of s >= 2^{l+1} q). QED.

### Lemma 5 (count invariant)

(i) Within one level's processing, a successful SF-BF allocation never
raises `count(s)` for any size s that currently has count >= 1; new blocks
are only created at sizes with count 0 (raising them to 1).
(ii) Consequently max_s count(s) does not increase during a level.
(iii) At the start of level-j processing, count(s) <= 2^j for every s.

Proof. (i) Let q be the request and let b be the minimum size of a free
block with b >= q (best fit). The staircase blocks created by Lemma 4 have
sizes q, 2q, ..., b/2, all lying in the half-open size-interval [q, b). By
minimality of b, the collection contains NO block whose size lies in [q, b).
The staircase contains exactly one block of each such size. Hence every
created size moves from count 0 to count 1; the consumed block lowers
count(b) by one; no other counts change.
(ii) Immediate from (i).
(iii) Induction on j. Base j = 0: the initial collection is the single
block [0, t0), all counts <= 1 = 2^0. Step: by (ii), counts stay <= 2^j
through level j; duplicating the free set at the transition doubles every
count, so level j+1 starts with counts <= 2^{j+1}. QED.

### Lemma 6 (failure implies small free measure)

If a level-j request of size q fails, then |F| < 2^j q.

Proof. Failure means every free block has size <= q/2. Distinct sizes are
q/2, q/4, ..., u, and by Lemma 5 each has count <= 2^j, so
|F| <= 2^j (q/2 + q/4 + ... + u) < 2^j q. QED.

### Lemma 7 (packing lemma, corrected)

If every reserved length satisfies

    d'_i <= t0 (1 - U),        where U = sum_i d'_i / T_i,

then SF-BF places all stations (in particular a compatible placement exists).

Proof. Suppose a request q = d'_i at level j fails. At that moment the
occupied measure within [0, W_j) is W_j times the density of the stations
placed so far, which is at most W_j U, so |F| >= W_j (1 - U) = 2^j t0 (1-U)
>= 2^j q by hypothesis. This contradicts Lemma 6 (which gives |F| < 2^j q).
Hence no request fails. QED.

### Corollary 8 (clean sufficient condition)

If U <= 1/2 and d'_i <= t0/2 for all i, then SF-BF packs all stations.
(Indeed t0 (1 - U) >= t0/2 >= d'_i.)

### Remarks on tightness and slack

R1. The 2^j count bound of Lemma 5 is TIGHT: numerically (5000 random
    instances), counts reach 2^j whenever intermediate levels are sparse
    (doubling outpaces consumption), even with count-aware tie-breaking
    rules -- an earlier conjecture that counts stay <= 2 is REFUTED (max
    count 16 observed at level 4). This costs nothing for the theorem,
    because the failure threshold in Lemma 7 is level-independent: the 2^j
    in Lemma 6 cancels against W_j = t0 2^j. Constant improvements must
    come from elsewhere (randomized anchor, or analyzing the implemented
    greedy directly), not from count-bounding.
    Mechanical verification: Lemma 7 + Lemma 5 checked on 2801 random
    harmonic instances (no violation); CE1 fails under SF-BF exactly at
    the predicted free measure.
R2. Lemma 7 is essentially tight in the following sense: generalize CE1 by
    letting level-0 stations occupy measure (1 - eps) t0 of the base window
    with a contiguous free gap of length eps t0. Any station of level >= 1
    must place its SP inside one periodic copy of the gap (a contiguous
    interval avoiding the level-0 pattern cannot span two gap copies), so
    d' <= eps t0 = t0 (1 - U_0) is NECESSARY. Lemma 7's condition replaces
    U_0 (utilization of the other stations) by the total U -- the gap
    between necessary and sufficient is only the requesting station's own
    density contribution and the count slack of R1.
R3. CE1 should go in the paper: it kills the natural "utilization bound"
    intuition inherited from preemptive scheduling (harmonic rate-monotonic
    is optimal at U = 1 only because preemption can split B's service across
    the two gap copies; TWT SPs are non-preemptive by design).

### Theorem 9 (8-approximation)

Assume (A2') d_i <= t0/4 for all i, where t0 is the grid anchor chosen by
the algorithm (t0 <= tmin). Consider the analysis variant of the algorithm:

1. Solve the relaxation (Lemma 1) with density target rho = 1/4:
   periods T_i(1/4) = max(Tmin_i, sqrt(lambda_{1/4} d_i / a_i)).
2. Round each d_i up to the next dyadic divisor d'_i of t0 (factor < 2,
   so d'_i <= t0/2 by (A2')), and each T_i(1/4) up to the grid t0 2^j
   (factor < 2). The resulting density satisfies
   U <= 2 * (1/4) = 1/2  (d-rounding doubles at most; T-rounding only
   decreases density).
3. Pack with SF-BF: succeeds by Corollary 8. The station only transmits
   for d_i <= d'_i inside its reserved block, so the actual duty cycle is
   d_i / T'_i <= d_i / Tmin_i = rho_i: energy budgets hold with the TRUE
   d_i (reservations are schedule-internal and cost no energy).

Cost chain, writing J(T) = sum_i a_i T_i (constants C cancel as before):

    J(T') <= 2 J(T(1/4))          [step 2 period rounding]
          <= 2 * 4 * J(T*(1))     [Lemma 2 generalized: T*(1)/rho is
                                   feasible at target rho, so
                                   J(T(rho)) <= J(T*)/rho with rho = 1/4]
          <= 8 (OPT - C).

Hence ALG <= 8 OPT. With the best-of-uniform safeguard, additionally
ALG <= cost(uniform).

### Corollary 10 (candidate-argmin anchor: factor 1/ln 2 on rounding)

Let T = (T_i) be the pre-rounding periods, tmin = min_i T_i, and for
theta in [0,1) let t0(theta) = tmin 2^{-theta} and T'_i(theta) the rounding
of T_i up to the grid t0(theta) 2^j. Write C(theta) = sum_i a_i T'_i(theta).
Then

    min over the n folded candidates of C  <=  (1/ln 2) sum_i a_i T_i.

Proof. (a) Expectation. The per-station rounding factor is
ell_i(theta) = 2^{ceil(z) - z} with z = y_i + theta, y_i = log2(T_i/tmin).
For theta uniform on [0,1), frac(z) is uniform, and
E[2^{ceil(z)-z}] = int_0^1 2^{1-f} df = 1/ln 2. By linearity (correlations
across i are irrelevant), E_theta[C(theta)] = (1/ln2) sum_i a_i T_i.
(b) The minimum over theta is attained at a candidate. On any open interval
of theta containing no breakpoint (a breakpoint is a theta with
frac(y_i + theta) = 0 for some i), every frac(y_i + theta) increases with
theta without wrapping, so every ell_i(theta) = 2^{1 - frac(y_i + theta)}
strictly decreases; hence C strictly decreases on each such interval.
As theta approaches a breakpoint from below, the wrapping station's factor
tends to 1, which equals its value AT the breakpoint (an exact power of two
rounds to itself), so C is left-continuous there and the infimum over
[0, 1) is attained at some breakpoint. The breakpoints are exactly
theta_c = 1 - frac(y_c) (and 0), i.e. t0(theta_c) = T_c / 2^{ceil(y_c)} --
precisely the folded candidates evaluated by the implementation.
(c) min_theta C(theta) <= E_theta[C(theta)] completes the proof. QED.

Consequently Theorem 9's constant improves DETERMINISTICALLY from
2 * 4 = 8 to (1/ln2) * 4 ~= 5.77 (the d-rounding factor enters only the
density budget, which must hold surely and is unchanged).

### Lemma 1' (long-SP valid inequality)

In ANY feasible schedule, T_i >= d_max := max_k d_k for every i.

Proof. Suppose T_A < d_B for some stations A != B (note A's own SP trivially
satisfies d_A <= rho_A T_A <= T_A). B's SP is a contiguous interval of
length d_B > T_A; the starts of A's SPs are spaced T_A apart, so at least
one start lies inside B's SP; the corresponding A-SP then overlaps B's SP.
Contradiction. QED.

Hence the relaxation may add the constraints T_i >= d_max without losing
the lower-bound property:

    OPT >= J*' := min { sum a_i T_i : sum d_i/T_i <= 1,
                        T_i >= max(Tmin_i, d_max) } + C.

### Theorem 11 (assumption-free approximation)

Without assumption (A2'), the following analysis variant is a
12-approximation (worst-case anchor), and a (6/ln2 ~= 8.66)-approximation
with the candidate-argmin anchor of Corollary 10:

1. Solve the relaxation of Lemma 1' and set T-hat_i = 6 T*'_i. Then:
   density(T-hat) <= 1/6; every T-hat_i >= 6 d_max; cost = 6 J*'.
2. Choose t0 among the folded candidates of T-hat: t0 > tmin-hat/2
   >= 3 d_max. Round periods up to the grid (factor < 2, or 1/ln2 via
   Corollary 10); density only decreases.
3. Round d_i up to dyadic divisors of t0: valid since d_i <= d_max < t0/3
   < t0; factor < 2, so density U' <= 2/6 = 1/3, and
   d'_i < 2 d_max < (2/3) t0 <= t0 (1 - U').
4. Pack with SF-BF: succeeds by Lemma 7 (condition verified in step 3).

Cost: ALG <= 2 * 6 * J*' <= 12 OPT (or (6/ln2) OPT). The constant 6 is
optimal for this accounting: the packing condition requires
2 d_max <= (s/2) d_max (1 - 2/s), i.e. s >= 6.

### Remaining gaps / next steps for the paper

1. (resolved negatively -- see R1)
2. (resolved -- Corollary 10: deterministic 4/ln2 ~= 5.77 under (A2'),
   6/ln2 ~= 8.66 unconditional via Theorem 11)
3. (resolved -- Lemma 1' + Theorem 11 remove (A2') at constant 12 resp.
   8.66; the scaling by 6 absorbs both the density budget and the
   long-SP margin in one step)
4. Dyadic-reservation mode: IMPLEMENTED as HarmonicGreedyTwtScheduler
   attribute "DyadicReservations" (with DensityTarget 0.25) -- the analyzed
   algorithm is now runnable; see simulation comparison in Section
   "multi-station findings".
5. The simulation results bracket the theory: empirical regimes where
   ALG = uniform (collapse) match exactly the cases where the relaxation
   solution is sub-octave (Section "octave quantization limit").
6. Open for the journal version: direct analysis of the implemented greedy
   (true-d placement, density 0.9), and the T2/T3 theorem targets
   (index policy asymptotics; N=2 exact).

### T3 RESULT (exact N=2; 2026-06-11) — with a structural surprise

Setup: stations 1, 2 with SP lengths d_1, d_2, coefficients a_i > 0, energy
floors Tmin_i; wlog T_1 <= T_2 in the optimum. J = a_1 T_1 + a_2 T_2.

Lemma T3.1 (rationality). Any feasible schedule has T_2/T_1 rational.
Proof: if T_2/T_1 is irrational, {j T_2 - k T_1 : j,k in Z} is dense in R
(equidistribution), so some difference of SP start times lies in
(-d_1, d_2), i.e. the SPs overlap. QED.

Lemma T3.2 (phase/packing characterization). Let T_2/T_1 = p/q (reduced,
p >= q >= 1). The starts of station 2's SPs, taken modulo T_1, form exactly
q equally spaced phases (spacing T_1/q), because {j p mod q} covers all
residues. Feasibility (disjoint SPs) holds iff

    T_1 >= q (d_1 + d_2),

i.e. station 1's contiguous SP fits in one of the q gaps of length
T_1/q - d_2. (Necessity: the SP cannot straddle an SP-2 interval;
sufficiency: place phi_1 in any gap.)

Lemma T3.3 (quasiconvexity in the ratio). For fixed q, writing r = p/q,

    J(r, q) = (a_1 + a_2 r) * max( Tmin_1, Tmin_2 / r, q (d_1+d_2) )

is quasiconvex in r on [1, infinity): strictly decreasing while
Tmin_2/r is the active max (J = a_1 Tmin_2 / r + a_2 Tmin_2), strictly
increasing once the max is constant. The continuous minimizer is

    r_q^* = max( 1, Tmin_2 / max(Tmin_1, q (d_1+d_2)) ).

THEOREM T3 (exact solution). The optimal N=2 cost is

    min over q = 1..Q  of  min over p in {floor(q r_q^*), ceil(q r_q^*)}
        (p >= q)  of  J(p/q, q),

where Q is the explicit finite bound: any q with
(a_1 + a_2) q (d_1+d_2) > J(1, m0) (m0 = ceil(r_1^*)) is dominated, so
Q <= J(1, m0) / ((a_1+a_2)(d_1+d_2)). Each candidate is a closed form;
the whole optimum is O(Q) arithmetic. Proof: T3.1 restricts to rationals,
T3.2 gives the exact feasible region for each (p, q), T3.3 reduces each q
to two integer candidates (quasiconvex + integer grid), the bound on Q
discards the tail. QED.

VERIFIED: Theorem T3's algorithm matches exhaustive search over the (p,q)
grid on 4,000 random instances (0 mismatches); counterexample numbers
confirmed (J(3/2)=25 < J(1)=J(2)=30 at a1=a2=1). Full proofs transcribed
to contrib/aoi-twt/paper/appendix.tex (techreport build target).

SURPRISE (paper-worthy): non-integer ratios are STRICTLY optimal on an
open set of instances — the natural conjecture "T_2 = m T_1 integer" is
FALSE. Example: Tmin_1 = 10, Tmin_2 = 15, d_1+d_2 = 1: ratio 3/2 achieves
J = 10 a_1 + 15 a_2 with BOTH energy constraints tight (feasible since
T_1 = 10 >= 2(d_1+d_2) = 2), while the best integer ratios give
15 a_1 + 15 a_2 (m=1) or 10 a_1 + 20 a_2 (m=2) — strictly worse for every
a_1, a_2 > 0. Consequence for the paper narrative: the harmonic (octave)
restriction of the N-station algorithm has a real price even at N=2; the
fractional-ratio gain is exactly what the rounding loss gives away, and
T3 quantifies it exactly in the smallest case.

### T2 SCOPE NOTE (deferred to journal version)

Stochastic arrivals turn each station into a constrained MDP coupled only
through the schedule-density constraint; the Lagrangian decomposition is
the same water-filling structure as Lemma 2 (density multiplier lambda =
the "index"). The asymptotic-optimality program (N -> infinity, density
held fixed) would follow the Weber-Weiss/fluid-limit route, with the novel
ingredient being the periodic-packing constraint replacing per-slot
capacity. Nontrivial; not attempted here.

### Proposition (peak-AoI carry-over)

E[A_peak] = delta + T/p (Section 2), so the weighted peak-AoI objective is
again linear in T with coefficients a_i^peak = w_i / p_i > 0. Every result
in this section (relaxation lower bound, water-filling form, rounding,
packing, Theorems 9/11, Corollary 10) uses only linearity and positivity of
the coefficients, hence carries over verbatim to weighted peak AoI with
a_i replaced by a_i^peak. The simulation now also measures per-cycle peak
AoI (SUMMARY peak_ms field).

## 4. What to validate in ns-3 first

1. Single-station E[A] formula vs simulation across (T, d, p) grid — also validates
   the TWT implementation itself (Section 2 formula is the test oracle).
   STATUS (2026-06-11): DONE, both regimes.
   - p ~= 1: error 0.016% (T=100ms,d=8ms), 0.065% (T=50ms,d=4ms).
   - p < 1 via per-SP block fading (window-coherent error model, coherence = T,
     known per-SP success p = 1-e): error 1.17% (p=0.8), 0.56% (p=0.6),
     0.88% (p=0.6... e=0.6 -> p=0.4). Renewal components verified directly:
     E[G] and E[G^2]/2E[G] within ~1% of T/p and T(1/p - 1/2).
   - Duty cycle exactly 0.080 = d/T at every error rate.

   Protocol lessons learned on the way (each one is paper-worthy motivation
   material for AP-side TWT awareness):
   a. Beacon-loss monitoring disassociates a dozing TWT STA (~1s); needs
      TWT-aware link maintenance (worked around via MaxMissedBeacons).
   b. Downlink to a dozing STA deadlocks: ARP replies, and worse, ADDBA
      Responses. A half-established (AP-side-only) BA agreement makes the AP's
      reordering buffer silently swallow ACKED uplink frames behind sequence
      gaps. Fixed by ANNOUNCED TWT (PM-bit toggling at SP boundaries) so the
      AP flushes buffered frames during SPs with existing PS machinery.
   c. PM=1 doze announcements cannot complete over a bad channel (the
      indication itself needs an ACK) -> announcements must be best-effort
      and dozing schedule-based; PM state reconciles in a later good SP.
   d. Without queue gating, pending retransmissions keep the STA awake past
      SP end (retry storm until the fading flips: ~156k transmissions in one
      run). Fixed via BlockUnicastTxOnLinks at SP end, unblock at SP start —
      bounded SPs restored, retx deferred to next SP, which is exactly the
      "k attempts per SP" structure assumed by the model.
   e. Per-frame i.i.d. loss with in-SP retries degenerates to pSucc ~= 1;
      the regime where the geometric AoI term matters is BLOCK fading with
      coherence >= SP duration. The theory's per-SP success p is the right
      primitive; per-attempt p is a derived quantity (p_SP = 1-(1-p)^k only
      when fading is per-attempt i.i.d.).
2. Per-wake overhead c_w measured from the ns-3 energy model (StateHelper traces)
   — feeds back into the model as a calibrated constant.
3. Two-station disjoint-SP schedule: AoI additivity check (no coupling when SPs
   disjoint) — validates the decoupling assumption behind T2.

## 5. Open questions / risks

- Does 802.11 beacon-anchored timing quantize offsets (nextTwt is defined relative
  to beacon in the wifiTwt API)? If offsets are beacon-quantized, packing granularity
  changes — check 802.11ax spec 26.8.2 / implementation.
- Clock drift between AP and STA shifts SPs (cf. arXiv:2008.02697) — ignore in
  theory, note as robustness experiment.
- Downlink AoI (AP -> stations) is a different problem (broadcast TWT) — out of
  scope for paper 1; mention as future work.
